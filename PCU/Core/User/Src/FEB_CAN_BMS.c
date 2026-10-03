#include "FEB_CAN_BMS.h"
#include "feb_can_db.h"
#include "feb_log.h"
#include <math.h>
#include <stdbool.h>
#include "FEB_CAN_Diagnostics.h"

/* Timeout for BMS CAN communication (ms) */
#define BMS_STATE_TIMEOUT_MS 500

#define BMS_PACK_VOLTAGE_INVALID 0x3FFFFFu
#define BMS_TEMP_INVALID (-4096)

/* Global BMS message data */
BMS_MESSAGE_TYPE BMS_MESSAGE;

/* Flag for deferred heartbeat transmission (set in ISR, processed in main loop) */
static volatile bool heartbeat_pending = false;

/* BMS state simulation (bench testing only — refused when BMS is active on bus) */
static bool bms_sim_active = false;
static FEB_SM_ST_t bms_sim_state = FEB_SM_ST_BOOT;

/* Forward declaration of callback with new signature */
static void FEB_CAN_BMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data);

FEB_SM_ST_t FEB_CAN_BMS_getState(void)
{
  if (bms_sim_active)
    return bms_sim_state;
  return BMS_MESSAGE.state;
}

float FEB_CAN_BMS_getAccumulatorVoltage(void)
{
  return BMS_MESSAGE.accumulator_voltage;
}

float FEB_CAN_BMS_getMaxTemperature(void)
{
  return BMS_MESSAGE.max_temperature;
}

void FEB_CAN_BMS_Init(void)
{
  LOG_I(TAG_BMS, "Initializing BMS CAN communication");

  // Register RX callbacks using new API
  FEB_CAN_RX_Params_t params = {
      .instance = FEB_CAN_INSTANCE_1,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_BMS_Callback,
      .user_data = NULL,
  };

  params.can_id = FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID;
  FEB_CAN_RX_Register(&params);

  params.can_id = FEB_CAN_BMS_STATE_FRAME_ID;
  FEB_CAN_RX_Register(&params);

  BMS_MESSAGE.state = FEB_SM_ST_BOOT;
  BMS_MESSAGE.max_temperature = NAN;
  BMS_MESSAGE.accumulator_voltage = NAN;
  BMS_MESSAGE.last_rx_timestamp = 0;

  LOG_I(TAG_BMS, "BMS CAN initialization complete");
}

static void FEB_CAN_BMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)user_data;

  /* NOTE: This callback runs in ISR context - avoid logging and blocking operations */

  BMS_MESSAGE.last_rx_timestamp = HAL_GetTick();

  if (can_id == FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID)
  {
    struct feb_can_bms_accumulator_temperature_t t;
    feb_can_bms_accumulator_temperature_unpack(&t, data, length);
    BMS_MESSAGE.max_temperature =
        t.max_cell_temperature == BMS_TEMP_INVALID
            ? NAN
            : (float)feb_can_bms_accumulator_temperature_max_cell_temperature_decode(t.max_cell_temperature);
  }
  else if (can_id == FEB_CAN_BMS_STATE_FRAME_ID)
  {
    struct feb_can_bms_state_t m;
    if (feb_can_bms_state_unpack(&m, data, length) == 0)
    {
      BMS_MESSAGE.state = (FEB_SM_ST_t)m.bms_state;
      BMS_MESSAGE.accumulator_voltage = m.total_pack_voltage == BMS_PACK_VOLTAGE_INVALID
                                            ? NAN
                                            : (float)feb_can_bms_state_total_pack_voltage_decode(m.total_pack_voltage);

      /* Defer heartbeat TX to main loop - do NOT transmit from ISR */
      if (BMS_MESSAGE.state == FEB_SM_ST_BUS_HEALTH_CHECK)
      {
        heartbeat_pending = true;
      }
    }
  }
}

void FEB_CAN_HEARTBEAT_Transmit(void)
{
  APPS_DataTypeDef apps_data;
  FEB_ADC_GetAPPSData(&apps_data);

  uint8_t tx_data[FEB_CAN_PCU_HEARTBEAT_LENGTH] = {0};
  struct feb_can_pcu_heartbeat_t heartbeat_msg = {.error0 = !apps_data.plausible};
  feb_can_pcu_heartbeat_pack(tx_data, &heartbeat_msg, sizeof(tx_data));

  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_PCU_HEARTBEAT_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                                            FEB_CAN_PCU_HEARTBEAT_LENGTH);

  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_BMS, "Failed to transmit heartbeat: %s", FEB_CAN_StatusToString(status));
  }
  else
  {
    LOG_D(TAG_BMS, "Heartbeat transmitted");
  }
}

void FEB_CAN_BMS_ProcessHeartbeat(void)
{
  if (bms_sim_active && !FEB_CAN_BMS_IsSilent())
  {
    bms_sim_active = false;
    LOG_W(TAG_BMS, "BMS sim cancelled: BMS active on CAN bus");
  }

  if (heartbeat_pending)
  {
    heartbeat_pending = false;
    LOG_D(TAG_BMS, "Processing deferred heartbeat (state=%d)", BMS_MESSAGE.state);
    FEB_CAN_HEARTBEAT_Transmit();
  }
}

bool FEB_CAN_BMS_IsSilent(void)
{
  return BMS_MESSAGE.last_rx_timestamp == 0 || (HAL_GetTick() - BMS_MESSAGE.last_rx_timestamp > BMS_STATE_TIMEOUT_MS);
}

bool FEB_CAN_BMS_SetStateSim(bool enabled, FEB_SM_ST_t state)
{
  if (enabled && !FEB_CAN_BMS_IsSilent())
    return false;
  bms_sim_active = enabled;
  bms_sim_state = state;
  return true;
}

bool FEB_CAN_BMS_InDriveState(void)
{
  if (bms_sim_active)
    return bms_sim_state == FEB_SM_ST_DRIVE;

  if (BMS_MESSAGE.last_rx_timestamp == 0)
    return false;

  if (HAL_GetTick() - BMS_MESSAGE.last_rx_timestamp > BMS_STATE_TIMEOUT_MS)
    return false;

  return BMS_MESSAGE.state == FEB_SM_ST_DRIVE;
}
