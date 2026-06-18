/**
 * @file FEB_CAN_Charger.c
 * @brief Charger CAN interface (ported from SN4 BMS, adapted to SN5)
 * @author Formula Electric @ Berkeley
 *
 * Charger: Elcon / HK "TC" family, part HK-J-H650-12 GEN3 (170-650 VDC output).
 * DBC vendored at common/FEB_CAN_Library_SN4/elcon.dbc (upstream
 * https://github.com/karlding/elcon-charger-dbc, message names prefixed
 * Charger_*) and fused into the generated CAN library, so the frames are packed
 * and unpacked through feb_can.h rather than by hand:
 *   - charger -> BMS : Charger_Status (0x18FF50E5)
 *   - BMS  -> charger: Charger_Limits (0x1806E5F4)
 *
 * Port of SN4 /BMS_REDO_withdrivers/Core/Src/FEB_CAN_Charger.c. Logic is
 * preserved; the transport is the SN5 feb_can_lib (extended-ID RX/TX,
 * FreeRTOS-safe TX queue) rather than SN4's raw HAL mailbox busy-wait, and
 * pack telemetry comes from the SN5 ADBMS accessors.
 */

#include "FEB_CAN_Charger.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "feb_log.h"
#include "FEB_Const.h"
#include "FEB_ADBMS6830B.h"
#include "FEB_SM.h"
#include "FEB_CAN_State.h"
#include "stm32f4xx_hal.h"
#include "cmsis_compiler.h"

#define TAG_CHARGER "[CHG]"

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
  volatile uint8_t hw_status;           /* 0 OK, 1 FAIL */
  volatile uint8_t temperature;         /* 0 OK, 1 FAULT */
  volatile uint8_t input_voltage;       /* 0 OK, 1 FAULT */
  volatile uint8_t state;               /* 0 CHARGING, 1 OFF */
  volatile uint8_t communication_state; /* 0 OK, 1 TIMEOUT */
  volatile uint32_t rx_count;
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

  if (can_id != FEB_CAN_CHARGER_STATUS_FRAME_ID)
  {
    return;
  }

  struct feb_can_charger_status_t s;
  if (feb_can_charger_status_unpack(&s, data, length) != 0)
  {
    return; /* short / malformed frame */
  }

  rx_msg.op_voltage_dV = s.output_voltage;
  rx_msg.op_current_dA = s.output_current;
  rx_msg.hw_status = s.hw_status;
  rx_msg.temperature = s.temperature;
  rx_msg.input_voltage = s.input_voltage;
  rx_msg.state = s.state;
  rx_msg.communication_state = s.communication_state;
  rx_msg.rx_count++;
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
      .can_id = FEB_CAN_CHARGER_STATUS_FRAME_ID,
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

  /* Charger-reported faults (HK GEN3 Charger_Status flags). Only honoured when
   * the charger frame is fresh. Hardware failure is a hard fault; the others
   * are recoverable (stop and return to BATTERY_FREE, retried on re-plug or
   * once charging conditions clear). These are charger telemetry, independent
   * of the BMS primary/secondary measurement bypasses below. */
  if (FEB_CAN_Charger_Received())
  {
    if (rx_msg.hw_status == FEB_CAN_CHARGER_STATUS_HW_STATUS_HW_STATUS_FAIL_CHOICE)
    {
      return -1;
    }
    if (rx_msg.temperature == FEB_CAN_CHARGER_STATUS_TEMPERATURE_TEMP_FAULT_CHOICE ||
        rx_msg.input_voltage == FEB_CAN_CHARGER_STATUS_INPUT_VOLTAGE_INPUT_VOLTAGE_FAULT_CHOICE ||
        rx_msg.communication_state == FEB_CAN_CHARGER_STATUS_COMMUNICATION_STATE_COMM_STATE_TIMEOUT_CHOICE)
    {
      return 1;
    }
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
  struct feb_can_charger_limits_t cmd = {
      .max_voltage = tx_msg.max_voltage_dV,
      .max_current = tx_msg.max_current_dA,
      .control = tx_msg.control,
  };
  uint8_t data[FEB_CAN_CHARGER_LIMITS_LENGTH];
  int packed = feb_can_charger_limits_pack(data, &cmd, sizeof(data));
  if (packed < 0)
  {
    return;
  }
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_CHARGER_LIMITS_FRAME_ID, FEB_CAN_ID_EXT, data, (uint8_t)packed);
}

/* Edge-triggered logging of charger-reported faults so the reason behind a
 * charge stop/fault is visible on the console without spamming each tick. */
static void log_charger_faults(void)
{
  static uint8_t prev_hw = 0, prev_temp = 0, prev_in = 0, prev_comm = 0;

  if (FEB_CAN_Charger_Received())
  {
    if (rx_msg.hw_status && !prev_hw)
    {
      LOG_E(TAG_CHARGER, "Charger reports HARDWARE FAILURE");
    }
    if (rx_msg.temperature && !prev_temp)
    {
      LOG_W(TAG_CHARGER, "Charger reports OVER-TEMPERATURE");
    }
    if (rx_msg.input_voltage && !prev_in)
    {
      LOG_W(TAG_CHARGER, "Charger reports INPUT-VOLTAGE FAULT");
    }
    if (rx_msg.communication_state && !prev_comm)
    {
      LOG_W(TAG_CHARGER, "Charger reports COMMUNICATION TIMEOUT");
    }
    prev_hw = rx_msg.hw_status;
    prev_temp = rx_msg.temperature;
    prev_in = rx_msg.input_voltage;
    prev_comm = rx_msg.communication_state;
  }
  else
  {
    prev_hw = prev_temp = prev_in = prev_comm = 0;
  }
}

void FEB_CAN_Charger_Process(void)
{
  log_charger_faults();

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
      tx_msg.max_voltage_dV = FEB_CHARGE_TARGET_VOLTAGE_dV;
      tx_msg.max_current_dA = 0; /* no current while stopped */
      tx_msg.control = 1;        /* stop */
      charger_can_transmit();
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

/* ============================================================================
 * Diagnostic snapshot (BMS|charger console command)
 * ============================================================================ */

void FEB_CAN_Charger_GetSnapshot(FEB_Charger_Snapshot_t *out)
{
  if (out == NULL)
  {
    return;
  }

  uint32_t last_tick = rx_msg.last_rx_tick;
  out->ever_seen = (last_tick != 0);
  out->present = FEB_CAN_Charger_Received();
  out->age_ms = out->ever_seen ? (HAL_GetTick() - last_tick) : 0;
  out->rx_count = rx_msg.rx_count;
  out->op_voltage_dV = rx_msg.op_voltage_dV;
  out->op_current_dA = rx_msg.op_current_dA;
  out->hw_status = rx_msg.hw_status;
  out->temperature = rx_msg.temperature;
  out->input_voltage = rx_msg.input_voltage;
  out->state = rx_msg.state;
  out->communication_state = rx_msg.communication_state;

  out->cmd_voltage_dV = tx_msg.max_voltage_dV;
  out->cmd_current_dA = tx_msg.max_current_dA;
  out->control = tx_msg.control;
  out->trickle_active = trickle_enabled;
  out->trickle_on = trickle_on;
  out->done_charging = tx_msg.done_charging;
}
