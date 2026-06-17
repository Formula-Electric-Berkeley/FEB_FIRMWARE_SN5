/**
 * @file FEB_CAN_State.c
 * @brief BMS CAN state publishing module
 */

#include "FEB_CAN_State.h"
#include "FEB_CAN_DASH.h"
#include "FEB_SM.h"
#include "FEB_ADBMS6830B.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "stm32f4xx_hal.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Note: Critical sections removed - current_state is volatile and 1 byte (atomic on ARM) */

/* R2D timeout for state transitions */
#define R2D_TIMEOUT_MS 500

/* CAN ready flag - prevents transmission before CAN is initialized */
static volatile bool can_ready = false;

/* Current BMS state - volatile for ISR/task access */
static volatile BMS_State_t current_state = BMS_STATE_BOOT;

/* BMS state message data */
static struct feb_can_bms_state_t bms_state_msg;

/* Accumulator telemetry messages broadcast to the PCU (consumed by FEB_CAN_BMS
 * on the PCU for the `bms` console display). total_pack_voltage is the cell-sum
 * pack voltage in decivolts; max_cell_temperature is in deci-degrees C. */
static struct feb_can_bms_accumulator_voltage_t acc_voltage_msg;
static struct feb_can_bms_accumulator_temperature_t acc_temp_msg;

/* State name lookup table - must match BMS_State_t enum order */
static const char *state_names[] = {
    "BOOT",              // 0
    "LV_POWER",          // 1
    "BUS_HEALTH_CHECK",  // 2
    "PRECHARGE",         // 3
    "ENERGIZED",         // 4
    "DRIVE",             // 5
    "BATTERY_FREE",      // 6
    "CHARGER_PRECHARGE", // 7
    "CHARGING",          // 8
    "BALANCE",           // 9
    "FAULT_BMS",         // 10
    "FAULT_BSPD",        // 11
    "FAULT_IMD",         // 12
    "FAULT_CHARGING",    // 13
};

void FEB_CAN_State_Init(void)
{
  memset(&bms_state_msg, 0, sizeof(bms_state_msg));
  memset(&acc_voltage_msg, 0, sizeof(acc_voltage_msg));
  memset(&acc_temp_msg, 0, sizeof(acc_temp_msg));
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
  if (state >= BMS_STATE_COUNT)
  {
    return -1;
  }
  current_state = state;
  return 0;
}

const char *FEB_CAN_State_GetStateName(BMS_State_t state)
{
  if (state >= BMS_STATE_COUNT)
  {
    return "UNKNOWN";
  }
  return state_names[state];
}

/* ---- Cell voltage broadcast -------------------------------------------------
 * CAN schema (see generate.py): modules 1..FEB_NBANKS, 4 pages per module,
 * 4 cells per page -> 16 cell slots per module. Each module maps to one
 * hardware bank (module m -> banks[m-1]). Banks hold FEB_NUM_CELLS_PER_BANK
 * (14) cells, so the top two slots of the last page carry no data and are sent
 * as zero. Every voltage frame shares the same wire layout: 4x uint16
 * little-endian millivolts at byte offsets 0/2/4/6, and frame IDs run
 * contiguously from FEB_CAN_BMS_MODULE_1_VOLTAGE_0_FRAME_ID. */
#define BMS_CELL_VOLTAGE_PAGES 4u
#define BMS_CELL_VOLTAGE_CELLS_PER_PAGE 4u
#define BMS_CELL_VOLTAGE_BASE_FRAME_ID FEB_CAN_BMS_MODULE_1_VOLTAGE_0_FRAME_ID

/* Keep this hand-rolled packer in sync with the generated definitions. */
_Static_assert(FEB_CAN_BMS_MODULE_1_VOLTAGE_0_LENGTH == 8, "cell voltage frame must be 8 bytes (4 x uint16)");
_Static_assert(FEB_CAN_BMS_MODULE_10_VOLTAGE_3_FRAME_ID == BMS_CELL_VOLTAGE_BASE_FRAME_ID +
                                                               (FEB_NBANKS - 1) * BMS_CELL_VOLTAGE_PAGES +
                                                               (BMS_CELL_VOLTAGE_PAGES - 1),
               "cell voltage frame IDs are not contiguous as assumed");

static uint16_t cell_voltage_to_millivolts(float voltage_V)
{
  int32_t millivolts = (int32_t)(voltage_V * 1000.0f + 0.5f);
  if (millivolts < 0)
  {
    millivolts = 0;
  }
  if (millivolts > UINT16_MAX)
  {
    millivolts = UINT16_MAX;
  }
  return (uint16_t)millivolts;
}

static void FEB_CAN_State_SendVoltageFrame(uint8_t bank, uint8_t page)
{
  uint8_t tx_data[8] = {0};

  for (uint8_t i = 0; i < BMS_CELL_VOLTAGE_CELLS_PER_PAGE; i++)
  {
    uint8_t cell = page * BMS_CELL_VOLTAGE_CELLS_PER_PAGE + i;
    if (cell >= FEB_NUM_CELLS_PER_BANK)
    {
      break; /* unused trailing slots stay zero */
    }

    uint16_t cv = cell_voltage_to_millivolts(FEB_ADBMS_GET_Cell_Voltage(bank, cell));
    tx_data[i * 2] = (uint8_t)(cv & 0xFF);
    tx_data[i * 2 + 1] = (uint8_t)(cv >> 8);
  }

  uint32_t frame_id = BMS_CELL_VOLTAGE_BASE_FRAME_ID + bank * BMS_CELL_VOLTAGE_PAGES + page;
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_id, FEB_CAN_ID_STD, tx_data, sizeof(tx_data));
}

/* ---- Cell temperature broadcast ---------------------------------------------
 * CAN schema (see generate.py): modules 1..FEB_NBANKS, 11 pages per module,
 * 4 temps per page -> 41 populated sensors per module (last page carries 1).
 * Each module maps to one hardware bank (module m -> banks[m-1]).
 *
 * The bank stores FEB_NUM_TEMP_SENSORS (42) raw slots but only 41 are wired;
 * raw index 39 (MUX6[4]) is unconnected, so the 41 dense values skip it:
 *   dense 0..38 -> raw 0..38, dense 39..40 -> raw 40..41.
 * Values are signed int16 centi-degrees C, little-endian. Frame IDs run
 * contiguously from FEB_CAN_BMS_MODULE_1_TEMPERATURE_0_FRAME_ID. */
#define BMS_CELL_TEMP_PAGES 11u
#define BMS_CELL_TEMP_SENSORS_PER_PAGE 4u
#define BMS_CELL_TEMP_POPULATED FEB_TEMP_SENSORS_POPULATED_PER_BANK
#define BMS_CELL_TEMP_UNCONNECTED_RAW 39u /* MUX6[4], not wired (see FEB_Const.h) */
#define BMS_CELL_TEMP_BASE_FRAME_ID FEB_CAN_BMS_MODULE_1_TEMPERATURE_0_FRAME_ID

/* Keep this hand-rolled packer in sync with the generated definitions. */
_Static_assert(FEB_CAN_BMS_MODULE_1_TEMPERATURE_0_LENGTH == 8, "full temperature frame must be 8 bytes (4 x int16)");
_Static_assert(FEB_CAN_BMS_MODULE_1_TEMPERATURE_10_LENGTH == 2, "last temperature page must be 2 bytes (1 x int16)");
_Static_assert(FEB_CAN_BMS_MODULE_10_TEMPERATURE_10_FRAME_ID ==
                   BMS_CELL_TEMP_BASE_FRAME_ID + (FEB_NBANKS - 1) * BMS_CELL_TEMP_PAGES + (BMS_CELL_TEMP_PAGES - 1),
               "temperature frame IDs are not contiguous as assumed");
_Static_assert(BMS_CELL_TEMP_POPULATED <= BMS_CELL_TEMP_PAGES * BMS_CELL_TEMP_SENSORS_PER_PAGE,
               "not enough temperature pages for the populated sensor count");

static int16_t cell_temp_to_centidegrees(float temp_C)
{
  float scaled = temp_C * 100.0f;
  int32_t centideg = (int32_t)(scaled + (scaled >= 0.0f ? 0.5f : -0.5f));
  if (centideg < INT16_MIN)
  {
    centideg = INT16_MIN;
  }
  if (centideg > INT16_MAX)
  {
    centideg = INT16_MAX;
  }
  return (int16_t)centideg;
}

/* Map a dense (0..40) sensor index to its raw slot, skipping the unwired one. */
static uint16_t cell_temp_dense_to_raw(uint8_t dense)
{
  return (dense < BMS_CELL_TEMP_UNCONNECTED_RAW) ? dense : (uint16_t)(dense + 1);
}

static void FEB_CAN_State_SendTemperatureFrame(uint8_t bank, uint8_t page)
{
  uint8_t tx_data[8] = {0};
  uint8_t used = 0;

  for (uint8_t i = 0; i < BMS_CELL_TEMP_SENSORS_PER_PAGE; i++)
  {
    uint8_t dense = page * BMS_CELL_TEMP_SENSORS_PER_PAGE + i;
    if (dense >= BMS_CELL_TEMP_POPULATED)
    {
      break; /* last page is partial (1 sensor) */
    }

    float temp_C = FEB_ADBMS_GET_Cell_Temperature(bank, cell_temp_dense_to_raw(dense));
    uint16_t cd = (uint16_t)cell_temp_to_centidegrees(temp_C);
    tx_data[i * 2] = (uint8_t)(cd & 0xFF);
    tx_data[i * 2 + 1] = (uint8_t)(cd >> 8);
    used++;
  }

  uint32_t frame_id = BMS_CELL_TEMP_BASE_FRAME_ID + bank * BMS_CELL_TEMP_PAGES + page;
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_id, FEB_CAN_ID_STD, tx_data, used * 2);
}

/* ---- Cell-data broadcast scheduler ------------------------------------------
 * Voltages (FEB_NBANKS * 4 pages) and temperatures (FEB_NBANKS * 11 pages) are
 * flattened into one frame index space and emitted one frame at a time, paced
 * evenly across BMS_CELL_BROADCAST_WINDOW_MS. This re-sends every cell-data
 * frame once per window (~1 s) without ever enqueuing a burst:
 *   index [0, BMS_VOLTAGE_FRAME_COUNT)      -> voltage frames
 *   index [BMS_VOLTAGE_FRAME_COUNT, total)  -> temperature frames */
#define BMS_CELL_BROADCAST_WINDOW_MS 1000u
#define BMS_VOLTAGE_FRAME_COUNT (FEB_NBANKS * BMS_CELL_VOLTAGE_PAGES)
#define BMS_TEMP_FRAME_COUNT (FEB_NBANKS * BMS_CELL_TEMP_PAGES)
#define BMS_CELL_BROADCAST_FRAME_COUNT (BMS_VOLTAGE_FRAME_COUNT + BMS_TEMP_FRAME_COUNT)

static void FEB_CAN_State_SendCellDataFrame(uint16_t index)
{
  if (index < BMS_VOLTAGE_FRAME_COUNT)
  {
    FEB_CAN_State_SendVoltageFrame((uint8_t)(index / BMS_CELL_VOLTAGE_PAGES),
                                   (uint8_t)(index % BMS_CELL_VOLTAGE_PAGES));
  }
  else
  {
    uint16_t t = index - BMS_VOLTAGE_FRAME_COUNT;
    FEB_CAN_State_SendTemperatureFrame((uint8_t)(t / BMS_CELL_TEMP_PAGES), (uint8_t)(t % BMS_CELL_TEMP_PAGES));
  }
}

void FEB_CAN_State_Tick(void)
{
  /* Don't transmit until CAN is initialized */
  if (!can_ready)
  {
    return;
  }

  /* Divider for 100ms period (called every 1ms) */
  static uint16_t state_divider = 0;
  state_divider++;

  if (state_divider >= 100)
  {
    state_divider = 0;

    /* Use authoritative state from FEB_SM so PCU always gets most recent state */
    bms_state_msg.bms_state = (uint8_t)FEB_SM_Get_Current_State();

    /* Pack and send */
    uint8_t tx_data[FEB_CAN_BMS_STATE_LENGTH];
    feb_can_bms_state_pack(tx_data, &bms_state_msg, sizeof(tx_data));

    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_STATE_FRAME_ID, FEB_CAN_ID_STD, tx_data, FEB_CAN_BMS_STATE_LENGTH);

    /* Accumulator voltage (0x02): total_pack_voltage = cell-sum in decivolts.
     * The PCU divides by 10 for display and uses it for the `bms` console view. */
    acc_voltage_msg.total_pack_voltage = (uint16_t)(FEB_ADBMS_Snapshot_Total_Voltage() * 10.0f + 0.5f);

    uint8_t acc_v_data[FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_LENGTH];
    feb_can_bms_accumulator_voltage_pack(acc_v_data, &acc_voltage_msg, sizeof(acc_v_data));
    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_FRAME_ID, FEB_CAN_ID_STD, acc_v_data,
                    FEB_CAN_BMS_ACCUMULATOR_VOLTAGE_LENGTH);

    /* Accumulator temperature (0x03): max_cell_temperature in deci-degrees C.
     * Snapshot is NaN until the first cell-monitor scan completes. */
    float max_temp_c = FEB_ADBMS_Snapshot_Max_Temp();
    acc_temp_msg.max_cell_temperature = isnan(max_temp_c) ? 0 : (int16_t)(max_temp_c * 10.0f);

    uint8_t acc_t_data[FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_LENGTH];
    feb_can_bms_accumulator_temperature_pack(acc_t_data, &acc_temp_msg, sizeof(acc_t_data));
    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID, FEB_CAN_ID_STD, acc_t_data,
                    FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_LENGTH);
  }

  /* Cell-data broadcast: spread all voltage + temperature frames evenly across
   * the window (called every 1ms) so each frame is re-sent once per window and
   * no single tick enqueues more than one frame. */
  static uint16_t broadcast_tick = 0;
  static uint16_t next_frame = 0;

  /* Emit frame i once broadcast_tick has reached its evenly-spaced slot
   * (i * window / count). The frame density is < 1 per tick, so this sends at
   * most one frame per call. */
  while (next_frame < BMS_CELL_BROADCAST_FRAME_COUNT && (uint32_t)next_frame * BMS_CELL_BROADCAST_WINDOW_MS <=
                                                            (uint32_t)broadcast_tick * BMS_CELL_BROADCAST_FRAME_COUNT)
  {
    FEB_CAN_State_SendCellDataFrame(next_frame);
    next_frame++;
  }

  broadcast_tick++;
  if (broadcast_tick >= BMS_CELL_BROADCAST_WINDOW_MS)
  {
    broadcast_tick = 0;
    next_frame = 0;
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
