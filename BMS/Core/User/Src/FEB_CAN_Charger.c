/**
 * @file FEB_CAN_Charger.c
 * @brief Charger CAN interface (ported from SN4 BMS, adapted to SN5)
 * @author Formula Electric @ Berkeley
 *
 * Port of SN4 /BMS_REDO_withdrivers/Core/Src/FEB_CAN_Charger.c. Logic is
 * preserved; the transport is the SN5 feb_can_lib (extended-ID RX/TX,
 * FreeRTOS-safe TX queue) rather than SN4's raw HAL mailbox busy-wait, and
 * pack telemetry comes from the SN5 ADBMS accessors.
 */

#include "FEB_CAN_Charger.h"
#include "feb_can_lib.h"
#include "FEB_Const.h"
#include "FEB_ADBMS6830B.h"
#include "FEB_SM.h"
#include "FEB_CAN_State.h"
#include "stm32f4xx_hal.h"
#include "cmsis_compiler.h"

/* Extended (29-bit) charger CAN IDs (CCS charger protocol). */
#define FEB_CAN_ID_CHARGER_BMS 0x1806E5F4u /* BMS -> charger */
#define FEB_CAN_ID_CHARGER_CCS 0x18FF50E5u /* charger -> BMS */

typedef struct
{
  uint16_t max_voltage_dV;
  uint16_t max_current_dA;
  uint8_t control; /* 0 = start charge, 1 = stop charge */
  bool done_charging;
} bms_to_charger_t;

typedef struct
{
  volatile uint16_t op_voltage_dV;
  volatile uint16_t op_current_dA;
  volatile uint8_t status;
  volatile uint32_t last_rx_tick;
} charger_to_bms_t;

static bms_to_charger_t tx_msg = {0};
static charger_to_bms_t rx_msg = {0};

/* Trickle-charge state (near top of charge). */
static bool trickle_enabled = false;
static uint32_t last_trickle_toggle = 0;
static bool trickle_on = true;

/* ============================================================================
 * CAN reception
 * ============================================================================ */

static void FEB_CAN_Charger_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                     const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)user_data;

  if (can_id != FEB_CAN_ID_CHARGER_CCS || length < 5)
  {
    return;
  }

  rx_msg.op_voltage_dV = (uint16_t)((data[0] << 8) | data[1]);
  rx_msg.op_current_dA = (uint16_t)((data[2] << 8) | data[3]);
  rx_msg.status = data[4];
  __DMB(); /* publish data before timestamp */
  rx_msg.last_rx_tick = HAL_GetTick();
}

void FEB_CAN_Charger_Init(void)
{
  tx_msg.max_voltage_dV = FEB_CHARGE_TARGET_VOLTAGE_dV;
  tx_msg.max_current_dA = FEB_CHARGE_CURRENT_dA;
  tx_msg.control = 1; /* default: stopped */
  tx_msg.done_charging = false;
  rx_msg.last_rx_tick = 0;

  FEB_CAN_RX_Params_t params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_ID_CHARGER_CCS,
      .id_type = FEB_CAN_ID_EXT,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_Charger_Callback,
      .user_data = NULL,
  };
  FEB_CAN_RX_Register(&params);
}

bool FEB_CAN_Charger_Received(void)
{
  if (rx_msg.last_rx_tick == 0)
  {
    return false;
  }
  return ((HAL_GetTick() - rx_msg.last_rx_tick) < FEB_CHARGER_RX_TIMEOUT_MS);
}

/* ============================================================================
 * Charge decision (SN4 FEB_CAN_Charging_Status, retargeted to SN5 accessors)
 * ============================================================================ */

int8_t FEB_CAN_Charging_Status(void)
{
  if (tx_msg.done_charging)
  {
    return 1;
  }

  /* Lock-free snapshots only: this runs from the 1ms SM task and must never
   * block on the ADBMS mutex (held for tens of ms during a temp scan). */
  float pack_v = FEB_ADBMS_Snapshot_Total_Voltage();

  /* No cell data yet (before the first voltage scan): not ready to charge. */
  if (pack_v <= 0.0f)
  {
    return 1;
  }

#if !FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS
  /* Pack-level hard over-voltage (volts vs volts). */
  if (pack_v >= FEB_CONFIG_PACK_HARD_MAX_VOLTAGE_V)
  {
    return -1;
  }

  /* Highest cell: soft limit -> stop charging, hard limit -> fault. */
  float max_cell_mV = FEB_ADBMS_Snapshot_Max_Cell_Voltage() * 1000.0f;
  if (max_cell_mV >= FEB_CONFIG_CELL_SOFT_MAX_VOLTAGE_mV)
  {
    if (max_cell_mV >= FEB_CONFIG_CELL_HARD_MAX_VOLTAGE_mV)
    {
      return -1;
    }
    return 1;
  }
#endif

#if !FEB_BMS_DISABLE_TEMP_CHECKS
  /* Highest temperature: soft limit -> stop charging, hard limit -> fault.
   * NaN (no temp scan yet) fails both comparisons -> treated as OK; cell-data
   * presence is already gated by pack_v above and ADBMS staleness is a fault. */
  float max_temp_dC = FEB_ADBMS_Snapshot_Max_Temp() * 10.0f;
  if (max_temp_dC >= FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC)
  {
    if (max_temp_dC >= FEB_CONFIG_CELL_HARD_MAX_TEMP_dC)
    {
      return -1;
    }
    return 1;
  }
#endif

  return 0;
}

void FEB_CAN_Charger_Start_Charge(void)
{
  tx_msg.control = 0;
  tx_msg.done_charging = false;
}

void FEB_CAN_Charger_Stop_Charge(void)
{
  tx_msg.control = 1;
  tx_msg.done_charging = true;
}

/* ============================================================================
 * Charger command transmission + trickle charge
 * ============================================================================ */

static void charger_can_transmit(void)
{
  uint8_t data[8];
  data[0] = (uint8_t)(tx_msg.max_voltage_dV >> 8);
  data[1] = (uint8_t)(tx_msg.max_voltage_dV & 0xFF);
  data[2] = (uint8_t)(tx_msg.max_current_dA >> 8);
  data[3] = (uint8_t)(tx_msg.max_current_dA & 0xFF);
  data[4] = tx_msg.control;
  data[5] = 0;
  data[6] = 0;
  data[7] = 0;
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_ID_CHARGER_BMS, FEB_CAN_ID_EXT, data, 8);
}

void FEB_CAN_Charger_Process(void)
{
  /* Re-arm: a charger unplug (RX timeout) ends the session, so a later replug
   * starts a fresh one. Re-entry to charging still requires Charging_Status()
   * == 0, i.e. cells back below the soft limits. */
  if (!FEB_CAN_Charger_Received())
  {
    tx_msg.done_charging = false;
  }

  if (FEB_SM_Get_Current_State() != BMS_STATE_CHARGING)
  {
    /* Not charging: keep a connected charger actively commanded OFF. Covers
     * faults latched by evaluate_faults()/fault_begin(), which bypass the
     * handler Stop_Charge() path — without this the charger would only stop
     * via its own RX timeout. */
    if (FEB_CAN_Charger_Received())
    {
      uint8_t stop[8] = {0};
      stop[0] = (uint8_t)(FEB_CHARGE_TARGET_VOLTAGE_dV >> 8);
      stop[1] = (uint8_t)(FEB_CHARGE_TARGET_VOLTAGE_dV & 0xFF);
      /* max current = 0, control = 1 (stop) */
      stop[4] = 1;
      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_ID_CHARGER_BMS, FEB_CAN_ID_EXT, stop, 8);
    }
    return;
  }

  float pack_v = FEB_ADBMS_Snapshot_Total_Voltage(); /* lock-free, SM-task safe */
  uint16_t pack_v_dV = (uint16_t)(pack_v * 10.0f);

  if (pack_v_dV >= FEB_TRICKLE_CHARGE_START_VOLTAGE_dV)
  {
    trickle_enabled = true;
  }
  else
  {
    trickle_enabled = false;
    trickle_on = true;
  }

  if (trickle_enabled)
  {
    uint32_t now = HAL_GetTick();
    if ((now - last_trickle_toggle) >= FEB_TRICKLE_CHARGE_INTERVAL_MS)
    {
      trickle_on = !trickle_on;
      last_trickle_toggle = now;
    }
    tx_msg.max_voltage_dV = FEB_CHARGE_TARGET_VOLTAGE_dV;
    tx_msg.max_current_dA = trickle_on ? FEB_TRICKLE_CHARGE_CURRENT_dA : 0;
    tx_msg.control = 0;
  }
  else
  {
    tx_msg.max_voltage_dV = FEB_CHARGE_TARGET_VOLTAGE_dV;
    tx_msg.max_current_dA = FEB_CHARGE_CURRENT_dA;
    tx_msg.control = 0;
  }

  charger_can_transmit();
}
