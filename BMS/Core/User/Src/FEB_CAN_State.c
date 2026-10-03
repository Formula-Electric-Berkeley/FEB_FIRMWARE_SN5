/**
 * @file FEB_CAN_State.c
 * @brief BMS CAN state publishing module
 */

#include "FEB_CAN_State.h"
#include "FEB_ADBMS6830B.h"
#include "FEB_CAN_DASH.h"
#include "FEB_Const.h"
#include "FEB_SM.h"
#include "feb_can_lib.h"
#include "feb_can_db.h"
#include "stm32f4xx_hal.h"
#include <math.h>
#include <stdbool.h>

/* Note: Critical sections removed - current_state is volatile and 1 byte (atomic on ARM) */

/* R2D timeout for state transitions */
#define R2D_TIMEOUT_MS 500

#define CELL_DATA_PERIOD_MS 10
#define CELL_MODULE_BITS 4
#define CELL_CELL_BITS 4
#define CELL_VOLTAGE_BITS 16
#define CELL_TEMP_BITS 13
#define CELL_TEMPS 3
#define CELL_VOLTAGE_SHIFT (CELL_MODULE_BITS + CELL_CELL_BITS)
#define CELL_TEMP_SHIFT (CELL_VOLTAGE_SHIFT + CELL_VOLTAGE_BITS)
#define CELL_VOLTAGE_V_PER_LSB 0.00015f
#define CELL_TEMP_C_PER_LSB 0.05f
#define CELL_VOLTAGE_INVALID 0xFFFFu
#define CELL_TEMP_MASK ((1u << CELL_TEMP_BITS) - 1u)
#define CELL_TEMP_RAW_MIN (-(1 << (CELL_TEMP_BITS - 1)))
#define CELL_TEMP_RAW_MAX ((1 << (CELL_TEMP_BITS - 1)) - 1)
#define CELL_TEMP_INVALID CELL_TEMP_RAW_MIN
#define CELL_NO_SENSOR 0xFF
#define PACK_VOLTAGE_INVALID 0x3FFFFFu

static const uint8_t cell_temp_slots[][CELL_TEMPS] = {
    {0, 28, 35}, {22, 29, 36}, {23, 30, 37}, {24, 31, 38}, {25, 32, 40}, {26, 33, 41}, {27, 34, CELL_NO_SENSOR},
    {7, 14, 21}, {6, 13, 20},  {5, 12, 19},  {4, 11, 18},  {3, 10, 17},  {2, 9, 16},   {1, 8, 15},
};

/* CAN ready flag - prevents transmission before CAN is initialized */
static volatile bool can_ready = false;

/* Current BMS state - volatile for ISR/task access */
static volatile BMS_State_t current_state = BMS_STATE_BOOT;

/* State name lookup table, indexed by BMS_State_t (reserved values are NULL) */
static const char *const state_names[BMS_STATE_COUNT] = {
    [BMS_STATE_BOOT] = "BOOT",
    [BMS_STATE_LV_POWER] = "LV_POWER",
    [BMS_STATE_BUS_HEALTH_CHECK] = "BUS_HEALTH_CHECK",
    [BMS_STATE_PRECHARGE] = "PRECHARGE",
    [BMS_STATE_ENERGIZED] = "ENERGIZED",
    [BMS_STATE_DRIVE] = "DRIVE",
    [BMS_STATE_BATTERY_FREE] = "BATTERY_FREE",
    [BMS_STATE_CHARGER_PRECHARGE] = "CHARGER_PRECHARGE",
    [BMS_STATE_CHARGING] = "CHARGING",
    [BMS_STATE_BALANCE] = "BALANCE",
    [BMS_STATE_FAULT_CELL_OVERVOLTAGE] = "FAULT_CELL_OVERVOLTAGE",
    [BMS_STATE_FAULT_CELL_UNDERVOLTAGE] = "FAULT_CELL_UNDERVOLTAGE",
    [BMS_STATE_FAULT_CELL_OVERTEMP] = "FAULT_CELL_OVERTEMP",
    [BMS_STATE_FAULT_CELL_UNDERTEMP] = "FAULT_CELL_UNDERTEMP",
    [BMS_STATE_FAULT_TEMP_SENSOR_LOSS] = "FAULT_TEMP_SENSOR_LOSS",
    [BMS_STATE_FAULT_ADBMS_INIT] = "FAULT_ADBMS_INIT",
    [BMS_STATE_FAULT_ADBMS_TIMEOUT] = "FAULT_ADBMS_TIMEOUT",
    [BMS_STATE_FAULT_IVT_TIMEOUT] = "FAULT_IVT_TIMEOUT",
    [BMS_STATE_FAULT_OVERCURRENT] = "FAULT_OVERCURRENT",
    [BMS_STATE_FAULT_IMD] = "FAULT_IMD",
    [BMS_STATE_FAULT_BSPD] = "FAULT_BSPD",
    [BMS_STATE_FAULT_CONTACTOR_MISMATCH] = "FAULT_CONTACTOR_MISMATCH",
    [BMS_STATE_FAULT_BALANCE_HV_ACTIVE] = "FAULT_BALANCE_HV_ACTIVE",
    [BMS_STATE_FAULT_PRECHARGE_TIMEOUT] = "FAULT_PRECHARGE_TIMEOUT",
    [BMS_STATE_FAULT_PRECHARGE_TOO_FAST] = "FAULT_PRECHARGE_TOO_FAST",
    [BMS_STATE_FAULT_CHARGER_PRECHARGE_TIMEOUT] = "FAULT_CHARGER_PRECHARGE_TIMEOUT",
    [BMS_STATE_FAULT_SHUTDOWN_OPEN] = "FAULT_SHUTDOWN_OPEN",
    [BMS_STATE_FAULT_AIR_MINUS_OPEN] = "FAULT_AIR_MINUS_OPEN",
    [BMS_STATE_FAULT_CHARGER_HW] = "FAULT_CHARGER_HW",
    [BMS_STATE_FAULT_MANUAL] = "FAULT_MANUAL",
};
_Static_assert(BMS_STATE_COUNT <= 256, "bms_state is an 8-bit CAN signal");

static uint16_t quantize_cell_voltage(float volts)
{
  if (!(volts >= 0.0f))
  {
    return CELL_VOLTAGE_INVALID;
  }
  long raw = lroundf(volts / CELL_VOLTAGE_V_PER_LSB);
  return raw >= (long)CELL_VOLTAGE_INVALID ? CELL_VOLTAGE_INVALID - 1 : (uint16_t)raw;
}

static uint32_t quantize_pack_voltage(float volts)
{
  if (!(volts >= 0.0f))
  {
    return PACK_VOLTAGE_INVALID;
  }
  long raw = lroundf(volts / CELL_VOLTAGE_V_PER_LSB);
  return raw >= (long)PACK_VOLTAGE_INVALID ? PACK_VOLTAGE_INVALID - 1 : (uint32_t)raw;
}

static int16_t quantize_temp(float celsius)
{
  if (isnan(celsius))
  {
    return CELL_TEMP_INVALID;
  }
  long raw = lroundf(celsius / CELL_TEMP_C_PER_LSB);
  if (raw <= CELL_TEMP_RAW_MIN)
    raw = CELL_TEMP_RAW_MIN + 1;
  if (raw > CELL_TEMP_RAW_MAX)
    raw = CELL_TEMP_RAW_MAX;
  return (int16_t)raw;
}

static uint16_t quantize_cell_temp(uint8_t bank, uint8_t slot)
{
  if (slot == CELL_NO_SENSOR)
  {
    return 0;
  }
  return (uint16_t)quantize_temp(FEB_ADBMS_GET_Cell_Temperature(bank, slot)) & CELL_TEMP_MASK;
}

static uint8_t to_module(uint8_t bank)
{
  return bank == FEB_ADBMS_NO_LOCATION ? 0 : bank + 1;
}

static uint8_t to_cell(uint8_t cell)
{
  return cell == FEB_ADBMS_NO_LOCATION ? 0 : cell + 1;
}

static uint8_t temp_slot_to_cell(uint8_t slot)
{
  for (uint8_t cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
  {
    for (uint8_t t = 0; t < CELL_TEMPS; t++)
    {
      if (cell_temp_slots[cell][t] == slot)
      {
        return cell + 1;
      }
    }
  }
  return 0;
}

static void send_bms_state(void)
{
  FEB_ADBMS_Voltage_Summary_t v;
  FEB_ADBMS_GET_ACC_Voltage_Summary(&v);

  struct feb_can_bms_state_t msg = {
      .bms_state = (uint8_t)FEB_SM_Get_Current_State(),
      .total_pack_voltage = quantize_pack_voltage(v.total_V),
  };
  uint8_t tx_data[FEB_CAN_BMS_STATE_LENGTH];
  feb_can_bms_state_pack(tx_data, &msg, sizeof(tx_data));
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_STATE_FRAME_ID, FEB_CAN_ID_STD, tx_data, sizeof(tx_data));
}

static void send_accumulator_voltage(void)
{
  FEB_ADBMS_Voltage_Summary_t v;
  FEB_ADBMS_GET_ACC_Voltage_Summary(&v);
  bool valid = v.min_bank != FEB_ADBMS_NO_LOCATION;

  struct feb_can_bms_accumulator_voltage_t msg = {
      .average_cell_voltage = quantize_cell_voltage(valid ? v.avg_V : NAN),
      .min_cell_voltage = quantize_cell_voltage(valid ? v.min_V : NAN),
      .max_cell_voltage = quantize_cell_voltage(valid ? v.max_V : NAN),
      .min_voltage_module = to_module(v.min_bank),
      .min_voltage_cell = to_cell(v.min_cell),
      .max_voltage_module = to_module(v.max_bank),
      .max_voltage_cell = to_cell(v.max_cell),
  };
  uint8_t tx_data[FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_LENGTH];
  feb_can_bms_accumulator_voltage_pack(tx_data, &msg, sizeof(tx_data));
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                  sizeof(tx_data));
}

static void send_accumulator_temperature(void)
{
  FEB_ADBMS_Temp_Summary_t t;
  FEB_ADBMS_GET_ACC_Temp_Summary(&t);

  struct feb_can_bms_accumulator_temperature_t msg = {
      .average_pack_temperature = quantize_temp(t.avg_C),
      .min_cell_temperature = quantize_temp(t.min_C),
      .max_cell_temperature = quantize_temp(t.max_C),
      .min_temperature_module = to_module(t.min_bank),
      .min_temperature_cell = t.min_sensor == FEB_ADBMS_NO_LOCATION ? 0 : temp_slot_to_cell(t.min_sensor),
      .max_temperature_module = to_module(t.max_bank),
      .max_temperature_cell = t.max_sensor == FEB_ADBMS_NO_LOCATION ? 0 : temp_slot_to_cell(t.max_sensor),
  };
  uint8_t tx_data[FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_LENGTH];
  feb_can_bms_accumulator_temperature_pack(tx_data, &msg, sizeof(tx_data));
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                  sizeof(tx_data));
}

static void send_next_cell_data(void)
{
  static uint8_t bank = 0;
  static uint8_t cell = 0;

  uint64_t bits = (uint64_t)(bank + 1) | ((uint64_t)(cell + 1) << CELL_MODULE_BITS) |
                  ((uint64_t)quantize_cell_voltage(FEB_ADBMS_GET_Cell_Voltage(bank, cell)) << CELL_VOLTAGE_SHIFT);
  for (uint8_t t = 0; t < CELL_TEMPS; t++)
  {
    bits |= (uint64_t)quantize_cell_temp(bank, cell_temp_slots[cell][t]) << (CELL_TEMP_SHIFT + t * CELL_TEMP_BITS);
  }

  uint8_t tx_data[FEB_CAN_BMS_CELL_DATA_LENGTH];
  for (uint8_t i = 0; i < sizeof(tx_data); i++)
  {
    tx_data[i] = (uint8_t)(bits >> (8 * i));
  }
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_CELL_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data, sizeof(tx_data));

  if (++cell >= FEB_NUM_CELLS_PER_BANK)
  {
    cell = 0;
    bank = (bank + 1) % FEB_NBANKS;
  }
}

void FEB_CAN_State_Init(void)
{
  current_state = BMS_STATE_BOOT;
}

void FEB_CAN_State_SetReady(void)
{
  can_ready = true;
}

BMS_State_t FEB_CAN_State_GetState(void)
{
  return current_state;
}

int FEB_CAN_State_SetState(BMS_State_t state)
{
  if (!BMS_State_Is_Valid(state))
  {
    return -1;
  }
  current_state = state;
  return 0;
}

const char *FEB_CAN_State_GetStateName(BMS_State_t state)
{
  if (!BMS_State_Is_Valid(state))
  {
    return "UNKNOWN";
  }
  return state_names[state];
}

void FEB_CAN_State_Tick(void)
{
  /* Don't transmit until CAN is initialized */
  if (!can_ready)
  {
    return;
  }

  static uint16_t state_divider = 0;
  if (++state_divider >= 100)
  {
    state_divider = 0;
    send_bms_state();
  }

  static uint16_t voltage_divider = 33;
  if (++voltage_divider >= 100)
  {
    voltage_divider = 0;
    send_accumulator_voltage();
  }

  static uint16_t temp_divider = 66;
  if (++temp_divider >= 100)
  {
    temp_divider = 0;
    send_accumulator_temperature();
  }

  static uint8_t cell_data_divider = 0;
  if (++cell_data_divider >= CELL_DATA_PERIOD_MS)
  {
    cell_data_divider = 0;
    send_next_cell_data();
  }
}

void FEB_CAN_State_ProcessTransitions(void)
{
  /* Don't process until CAN is initialized */
  if (!can_ready)
  {
    return;
  }

  /*
   * NOTE: This function now routes through FEB_SM_Transition() to ensure
   * proper relay control and state validation. Previously this function
   * directly modified current_state, bypassing the state machine's
   * relay control logic.
   *
   * The FEB_SM_Process() function (called from timer ISR) already handles
   * ENERGIZED <-> DRIVE transitions via EnergizedTransition() and
   * DriveTransition(). This function is kept for API compatibility.
   */
  BMS_State_t state = FEB_SM_Get_Current_State();

  /* ENERGIZED -> DRIVE: When R2D is active and fresh */
  if (state == BMS_STATE_ENERGIZED)
  {
    if (FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    {
      FEB_SM_Transition(BMS_STATE_DRIVE);
    }
  }
  /* DRIVE -> ENERGIZED: When R2D is inactive or stale */
  else if (state == BMS_STATE_DRIVE)
  {
    if (!FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    {
      FEB_SM_Transition(BMS_STATE_ENERGIZED);
    }
  }
}
