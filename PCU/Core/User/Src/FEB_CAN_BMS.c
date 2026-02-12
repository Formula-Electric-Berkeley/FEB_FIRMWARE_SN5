#include "FEB_CAN_BMS.h"
#include "feb_uart_log.h"

/* Global BMS message data */
BMS_MESSAGE_TYPE BMS_MESSAGE;

/* Flag for deferred heartbeat transmission (set in ISR, processed in main loop) */
static volatile bool heartbeat_pending = false;

/* Forward declaration of callback with new signature */
static void FEB_CAN_BMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data);

uint16_t FEB_CAN_BMS_getTemp(void)
{
  return BMS_MESSAGE.temperature;
}

uint16_t FEB_CAN_BMS_getVoltage(void)
{
  return BMS_MESSAGE.voltage;
}

uint8_t FEB_CAN_BMS_getDeviceSelect(void)
{
  return BMS_MESSAGE.ping_ack;
}

FEB_SM_ST_t FEB_CAN_BMS_getState(void)
{
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

  params.can_id = FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_FRAME_ID;
  FEB_CAN_RX_Register(&params);

  LOG_I(TAG_BMS, "Registered BMS CAN callbacks (Temp: 0x%03lX, State: 0x%03lX, Voltage: 0x%03lX)",
        FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID, FEB_CAN_BMS_STATE_FRAME_ID,
        FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_FRAME_ID);

  BMS_MESSAGE.temperature = 0;
  BMS_MESSAGE.voltage = 0;
  BMS_MESSAGE.state = FEB_SM_ST_DEFAULT;
  BMS_MESSAGE.ping_ack = FEB_HB_NULL;
  BMS_MESSAGE.max_temperature = 0.0f;
  BMS_MESSAGE.accumulator_voltage = 0.0f;
  BMS_MESSAGE.last_rx_timestamp = 0;

  LOG_I(TAG_BMS, "BMS CAN initialization complete");
}

static void FEB_CAN_BMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)length;
  (void)user_data;

  /* NOTE: This callback runs in ISR context - avoid logging and blocking operations */

  BMS_MESSAGE.last_rx_timestamp = HAL_GetTick();

  if (can_id == FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID)
  {
    BMS_MESSAGE.temperature = data[2] << 8 | data[3];
    BMS_MESSAGE.max_temperature = (float)BMS_MESSAGE.temperature / 10.0f;
  }
  else if (can_id == FEB_CAN_BMS_STATE_FRAME_ID)
  {
    BMS_MESSAGE.state = data[0] & 0x1F;
    BMS_MESSAGE.ping_ack = (data[0] & 0xE0) >> 5;

    /* Defer heartbeat TX to main loop - do NOT transmit from ISR */
    if (BMS_MESSAGE.state == FEB_SM_ST_HEALTH_CHECK || BMS_MESSAGE.ping_ack == FEB_HB_PCU)
    {
      heartbeat_pending = true;
    }
  }
  else if (can_id == FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_FRAME_ID)
  {
    BMS_MESSAGE.voltage = (data[0] << 8) | (data[1]);
    BMS_MESSAGE.accumulator_voltage = (float)BMS_MESSAGE.voltage / 10.0f;
  }
}

void FEB_CAN_HEARTBEAT_Transmit(void)
{
  uint8_t data[8] = {0};
  data[0] = 1;

  FEB_CAN_Status_t status =
      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_PCU_HEARTBEAT_FRAME_ID, FEB_CAN_ID_STD, data, 1);
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
  if (heartbeat_pending)
  {
    heartbeat_pending = false;
    LOG_D(TAG_BMS, "Processing deferred heartbeat (state=%d, ping_ack=%d)", BMS_MESSAGE.state, BMS_MESSAGE.ping_ack);
    FEB_CAN_HEARTBEAT_Transmit();
  }
}
