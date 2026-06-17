/**
 ******************************************************************************
 * @file           : FEB_Commands.c
 * @brief          : BMS-specific console commands
 * @author         : Formula Electric @ Berkeley
 *
 * Text mode (human): one `BMS` descriptor sub-dispatches on the second
 * token (BMS|status, BMS|cells, BMS|state, ...).
 *
 * CSV mode (spec): parser strips the `BMS|csv|<tx_id>|` prefix so each
 * command needs its own top-level descriptor. We register the text-mode
 * BMS descriptor with no .csv_handler, and register a CSV-only descriptor
 * per spec-facing command. Mandatory: `cell-stats`. The rest surface
 * the existing diagnostic subcommands to CSV clients.
 ******************************************************************************
 */

#include "FEB_Commands.h"
#include "feb_console.h"
#include "feb_log.h"
#include "feb_string_utils.h"
#include "FEB_ADBMS6830B.h"
#include "ADBMS6830B_Registers.h"
#include "FEB_CAN_IVT.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_CAN_State.h"
#include "FEB_Const.h"
#include "FEB_HW_Relay.h"
#include "FEB_SM.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Logging tag */
#define TAG_BMS "[BMS]"

/* External task handles for stack monitoring */
extern osThreadId_t uartRxTaskHandle;
extern osThreadId_t ADBMSTaskHandle;
extern osThreadId_t TPSTaskHandle;
extern osThreadId_t BMSTaskRxHandle;
extern osThreadId_t BMSTaskTxHandle;

/* ============================================================================
 * Subcommand: status - Show BMS status summary (C-code/S-code)
 * ============================================================================ */
static void subcmd_status(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  /* Compute min/max for both C and S codes */
  float min_c = 999.0f, max_c = 0.0f;
  float min_s = 999.0f, max_s = 0.0f;
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
    {
      float v_c = FEB_ADBMS_GET_Cell_Voltage(bank, cell);
      float v_s = FEB_ADBMS_GET_Cell_Voltage_S(bank, cell);
      if (v_c > 0)
      {
        if (v_c < min_c)
          min_c = v_c;
        if (v_c > max_c)
          max_c = v_c;
      }
      if (v_s > 0)
      {
        if (v_s < min_s)
          min_s = v_s;
        if (v_s > max_s)
          max_s = v_s;
      }
    }
  }

  FEB_Console_Printf("\r\n=== BMS Status ===\r\n");
  FEB_Console_Printf("State: %s\r\n", FEB_CAN_State_GetStateName(FEB_SM_Get_Current_State()));
  FEB_Console_Printf("Pack Voltage: %.2fV\r\n", FEB_ADBMS_GET_ACC_Total_Voltage());
  FEB_Console_Printf("Min Cell (C/S): %.3fV / %.3fV\r\n", min_c, min_s);
  FEB_Console_Printf("Max Cell (C/S): %.3fV / %.3fV\r\n", max_c, max_s);
  FEB_Console_Printf("Min Temp: %.1fC  Max Temp: %.1fC  Avg: %.1fC\r\n", FEB_ADBMS_GET_ACC_MIN_Temp(),
                     FEB_ADBMS_GET_ACC_MAX_Temp(), FEB_ADBMS_GET_ACC_AVG_Temp());
  FEB_Console_Printf("Balancing: %s\r\n", FEB_Cell_Balancing_Status() ? "ON" : "OFF");
  FEB_Console_Printf("Cell delta: %.0fmV  Balance done: %s\r\n", FEB_ADBMS_GET_Cell_Voltage_Delta_mV(),
                     FEB_Cell_Balance_Complete() ? "YES" : "NO");
  FEB_Console_Printf("Error Type: 0x%02X\r\n", FEB_ADBMS_Get_Error_Type());
}

/* ============================================================================
 * Subcommand: cells - Show individual cell voltages (C-code/S-code)
 * ============================================================================ */
static void subcmd_cells(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== Cell Voltages (C/S) ===\r\n");
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    FEB_Console_Printf("Bank %d:\r\n", bank + 1);
    for (int cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
    {
      float v_c = FEB_ADBMS_GET_Cell_Voltage(bank, cell);
      float v_s = FEB_ADBMS_GET_Cell_Voltage_S(bank, cell);
      uint8_t bal = FEB_ADBMS_GET_Cell_Discharging(bank, cell);
      FEB_Console_Printf("  C%02d: %.3f/%.3f%s\r\n", cell + 1, v_c, v_s, bal ? "  *BAL" : "");
    }
  }
  FEB_Console_Printf("Balancing: %u cells active | delta %.0fmV | done: %s\r\n",
                     (unsigned int)FEB_ADBMS_GET_Balancing_Cell_Count(), FEB_ADBMS_GET_Cell_Voltage_Delta_mV(),
                     FEB_Cell_Balance_Complete() ? "YES" : "NO");
}

/* ============================================================================
 * Subcommand: temps - Show temperature sensor readings
 * ============================================================================ */
static void subcmd_temps(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== Temperature Readings ===\r\n");
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    FEB_Console_Printf("Bank %d: ", bank + 1);
    for (int sensor = 0; sensor < FEB_NUM_TEMP_SENSORS; sensor++)
    {
      FEB_Console_Printf("%.1fC ", FEB_ADBMS_GET_Cell_Temperature(bank, sensor));
    }
    FEB_Console_Printf("\r\n");
  }
  FEB_Console_Printf("Pack: Min=%.1fC Max=%.1fC Avg=%.1fC\r\n", FEB_ADBMS_GET_ACC_MIN_Temp(),
                     FEB_ADBMS_GET_ACC_MAX_Temp(), FEB_ADBMS_GET_ACC_AVG_Temp());
}

/* ============================================================================
 * Subcommand: therm-raw - Show raw thermistor voltages and ADC codes
 * ============================================================================ */
static void subcmd_therm_raw(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== Thermistor Raw Voltages (mV) ===\r\n");
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int ic = 0; ic < FEB_NUM_ICPBANK; ic++)
    {
      FEB_Console_Printf("Bank %d IC %d\r\n", bank + 1, ic + 1);
      for (int mux = 0; mux < 6; mux++)
      {
        FEB_Console_Printf("  MUX%d ch0..6:", mux + 1);
        for (int ch = 0; ch < 7; ch++)
        {
          uint16_t sensor = (uint16_t)(ic * FEB_NUM_TEMP_SENSE_PER_IC + mux * 7 + ch);
          FEB_Console_Printf(" %7.1f", FEB_ADBMS_GET_Therm_Raw_mV((uint8_t)bank, sensor));
        }
        FEB_Console_Printf("\r\n");
      }
    }
  }
  FEB_Console_Printf("\r\n=== Thermistor Raw Codes (hex) ===\r\n");
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int ic = 0; ic < FEB_NUM_ICPBANK; ic++)
    {
      FEB_Console_Printf("Bank %d IC %d\r\n", bank + 1, ic + 1);
      for (int mux = 0; mux < 6; mux++)
      {
        FEB_Console_Printf("  MUX%d ch0..6:", mux + 1);
        for (int ch = 0; ch < 7; ch++)
        {
          uint16_t sensor = (uint16_t)(ic * FEB_NUM_TEMP_SENSE_PER_IC + mux * 7 + ch);
          FEB_Console_Printf(" 0x%04X", FEB_ADBMS_GET_Therm_Raw_Code((uint8_t)bank, sensor));
        }
        FEB_Console_Printf("\r\n");
      }
    }
  }
  FEB_Console_Printf("Note: 0x%04X / NaN = PEC failure on that aux register\r\n", 0xFFFF);
}

/* ============================================================================
 * Subcommand: balance - Show/control cell balancing
 * ============================================================================ */

/**
 * @brief Check if balancing is allowed in the current state
 * @return true if balancing can be started/running
 *
 * Balancing is only safe when the vehicle is not in motion and not energized
 * for driving. Allowed states:
 * - BATTERY_FREE: Accumulator isolated, safest for balancing
 * - BALANCE: Explicit balancing state
 */
static bool is_balancing_allowed(void)
{
  BMS_State_t state = FEB_SM_Get_Current_State();
  return (state == BMS_STATE_BATTERY_FREE || state == BMS_STATE_BALANCE);
}

static void subcmd_balance(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Balancing: %s\r\n", FEB_Cell_Balancing_Status() ? "ON" : "OFF");
    FEB_Console_Printf("State: %s\r\n", FEB_CAN_State_GetStateName(FEB_SM_Get_Current_State()));
    FEB_Console_Printf("Usage: BMS|balance|on  or  BMS|balance|off\r\n");
    FEB_Console_Printf("Note: Balancing only allowed in BATTERY_FREE or BALANCE states\r\n");
    return;
  }

  if (FEB_strcasecmp(argv[1], "on") == 0)
  {
    /* Safety check: only allow balancing in safe states */
    if (!is_balancing_allowed())
    {
      FEB_Console_Printf("Error: Balancing not allowed in %s state\r\n",
                         FEB_CAN_State_GetStateName(FEB_SM_Get_Current_State()));
      FEB_Console_Printf("Allowed states: BATTERY_FREE, BALANCE\r\n");
      return;
    }
    FEB_Cell_Balance_Start();
    if (FEB_SM_Get_Current_State() == BMS_STATE_BATTERY_FREE)
    {
      FEB_SM_Transition(BMS_STATE_BALANCE); /* 6->8 begin_balance */
    }
    FEB_Console_Printf("Balancing started\r\n");
  }
  else if (FEB_strcasecmp(argv[1], "off") == 0)
  {
    FEB_Stop_Balance();
    if (FEB_SM_Get_Current_State() == BMS_STATE_BALANCE)
    {
      FEB_SM_Transition(BMS_STATE_BATTERY_FREE); /* 8->6 stop_balance */
    }
    FEB_Console_Printf("Balancing stopped\r\n");
  }
  else
  {
    FEB_Console_Printf("Unknown option: %s\r\n", argv[1]);
    FEB_Console_Printf("Usage: BMS|balance|on  or  BMS|balance|off\r\n");
  }
}

/* ============================================================================
 * Subcommand: state - Show/set BMS state (with safety restrictions)
 * ============================================================================ */

static bool is_state_transition_allowed(BMS_State_t current, BMS_State_t target)
{
  if (target >= BMS_STATE_FAULT_BMS && target <= BMS_STATE_FAULT_CHARGING)
  {
    return true;
  }
  if ((current == BMS_STATE_ENERGIZED && target == BMS_STATE_DRIVE) ||
      (current == BMS_STATE_DRIVE && target == BMS_STATE_ENERGIZED))
  {
    return true;
  }
  /* Manual stepping through the HV path (skips the BUS_HEALTH auto-gate).
   * With a healthy shutdown loop this mirrors what the SM does on its own;
   * with the loop open, the per-state Shutdown/AIR- backouts eject to
   * LV_POWER. ENERGIZED->LV_POWER is the teardown (opens AIR+/precharge). */
  if ((current == BMS_STATE_LV_POWER && (target == BMS_STATE_BUS_HEALTH_CHECK || target == BMS_STATE_PRECHARGE)) ||
      (current == BMS_STATE_BUS_HEALTH_CHECK && target == BMS_STATE_PRECHARGE) ||
      (current == BMS_STATE_ENERGIZED && target == BMS_STATE_LV_POWER))
  {
    LOG_W(TAG_BMS, "Manual transition override: %s -> %s", FEB_CAN_State_GetStateName(current),
          FEB_CAN_State_GetStateName(target));
    return true;
  }
  if (target == BMS_STATE_BATTERY_FREE)
  {
    if (current == BMS_STATE_LV_POWER || current == BMS_STATE_BUS_HEALTH_CHECK)
    {
      return true;
    }
    LOG_W(TAG_BMS, "BATTERY_FREE only allowed from LV_POWER or BUS_HEALTH_CHECK (current: %s)",
          FEB_CAN_State_GetStateName(current));
    return false;
  }
  return false;
}

static void subcmd_state(int argc, char *argv[])
{
  BMS_State_t current_state = FEB_SM_Get_Current_State();

  if (argc < 2)
  {
    /* Read-only: always allowed */
    FEB_Console_Printf("BMS State: %s (%d)\r\n", FEB_CAN_State_GetStateName(current_state), current_state);
    FEB_Console_Printf("\r\nUsage: BMS|state|<name|number>\r\n");
    FEB_Console_Printf("States: boot(0), lv_power(1), bus_health(2), precharge(3),\r\n");
    FEB_Console_Printf("        energized(4), drive(5), battery_free(6), charger_pre(7),\r\n");
    FEB_Console_Printf("        charging(8), balance(9), fault_bms(10), fault_bspd(11),\r\n");
    FEB_Console_Printf("        fault_imd(12), fault_charging(13)\r\n");
    FEB_Console_Printf("\r\nSafe transitions: ENERGIZED<->DRIVE, LV/BUS_HEALTH->BATTERY_FREE, ->FAULT_*\r\n");
    FEB_Console_Printf("Manual: LV->BUS_HEALTH/PRECHARGE, BUS_HEALTH->PRECHARGE, ENERGIZED->LV\r\n");
    return;
  }

  BMS_State_t new_state;
  const char *arg = argv[1];

  /* Try numeric first */
  if (arg[0] >= '0' && arg[0] <= '9')
  {
    int val = atoi(arg);
    if (val < 0 || val >= BMS_STATE_COUNT)
    {
      FEB_Console_Printf("Error: State must be 0-%d\r\n", BMS_STATE_COUNT - 1);
      return;
    }
    new_state = (BMS_State_t)val;
  }
  else
  {
    /* Try name match */
    if (FEB_strcasecmp(arg, "boot") == 0)
      new_state = BMS_STATE_BOOT;
    else if (FEB_strcasecmp(arg, "lv_power") == 0 || FEB_strcasecmp(arg, "lv") == 0)
      new_state = BMS_STATE_LV_POWER;
    else if (FEB_strcasecmp(arg, "bus_health") == 0 || FEB_strcasecmp(arg, "bus") == 0)
      new_state = BMS_STATE_BUS_HEALTH_CHECK;
    else if (FEB_strcasecmp(arg, "precharge") == 0 || FEB_strcasecmp(arg, "pre") == 0)
      new_state = BMS_STATE_PRECHARGE;
    else if (FEB_strcasecmp(arg, "energized") == 0)
      new_state = BMS_STATE_ENERGIZED;
    else if (FEB_strcasecmp(arg, "drive") == 0)
      new_state = BMS_STATE_DRIVE;
    else if (FEB_strcasecmp(arg, "battery_free") == 0 || FEB_strcasecmp(arg, "free") == 0)
      new_state = BMS_STATE_BATTERY_FREE;
    else if (FEB_strcasecmp(arg, "charger_precharge") == 0 || FEB_strcasecmp(arg, "charger_pre") == 0)
      new_state = BMS_STATE_CHARGER_PRECHARGE;
    else if (FEB_strcasecmp(arg, "charging") == 0 || FEB_strcasecmp(arg, "charge") == 0)
      new_state = BMS_STATE_CHARGING;
    else if (FEB_strcasecmp(arg, "balance") == 0 || FEB_strcasecmp(arg, "bal") == 0)
      new_state = BMS_STATE_BALANCE;
    else if (FEB_strcasecmp(arg, "fault_bms") == 0 || FEB_strcasecmp(arg, "fault") == 0)
      new_state = BMS_STATE_FAULT_BMS;
    else if (FEB_strcasecmp(arg, "fault_bspd") == 0)
      new_state = BMS_STATE_FAULT_BSPD;
    else if (FEB_strcasecmp(arg, "fault_imd") == 0)
      new_state = BMS_STATE_FAULT_IMD;
    else if (FEB_strcasecmp(arg, "fault_charging") == 0)
      new_state = BMS_STATE_FAULT_CHARGING;
    else
    {
      FEB_Console_Printf("Unknown state: %s\r\n", arg);
      return;
    }
  }

  if (!is_state_transition_allowed(current_state, new_state))
  {
    FEB_Console_Printf("Error: Transition %s -> %s not allowed\r\n", FEB_CAN_State_GetStateName(current_state),
                       FEB_CAN_State_GetStateName(new_state));
    FEB_Console_Printf("Allowed: ENERGIZED<->DRIVE, LV/BUS_HEALTH->BATTERY_FREE, ->FAULT_*\r\n");
    FEB_Console_Printf("Manual: LV->BUS_HEALTH/PRECHARGE, BUS_HEALTH->PRECHARGE, ENERGIZED->LV\r\n");
    return;
  }

  FEB_SM_Transition(new_state);
  FEB_Console_Printf("State transition requested: %s -> %s\r\n", FEB_CAN_State_GetStateName(current_state),
                     FEB_CAN_State_GetStateName(new_state));
}

/* ============================================================================
 * Subcommand: gpio - Show hardware I/O status
 * ============================================================================ */
static void subcmd_gpio(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== GPIO Status ===\r\n");

  FEB_Console_Printf("Sense Inputs:\r\n");
  FEB_Console_Printf("  AIR+ Sense:      %s\r\n", FEB_HW_AIR_Plus_Sense() == FEB_RELAY_STATE_CLOSE ? "CLOSED" : "OPEN");
  FEB_Console_Printf("  AIR- Sense:      %s\r\n",
                     FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_CLOSE ? "CLOSED" : "OPEN");
  FEB_Console_Printf("  Precharge Sense: %s\r\n",
                     FEB_HW_Precharge_Sense() == FEB_RELAY_STATE_CLOSE ? "CLOSED" : "OPEN");
  FEB_Console_Printf("  Shutdown Loop:   %s\r\n", FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_CLOSE ? "CLOSED" : "OPEN");
  FEB_Console_Printf("  IMD Status:      %s\r\n", FEB_HW_IMD_Sense() == FEB_RELAY_STATE_CLOSE
                                                      ? "OK (armed)"
                                                      : (FEB_SM_IMD_Armed() ? "FAULT" : "LOW (not armed)"));
  FEB_Console_Printf("  Reset Button:    %s\r\n", FEB_HW_Reset_Button_Pressed() ? "PRESSED" : "NOT_PRESSED");

  FEB_Console_Printf("Outputs:\r\n");
  FEB_Console_Printf("  BMS SHDN (PC1):  %s\r\n", FEB_HW_BMS_Shutdown_Get() ? "CLOSED" : "OPEN");
  FEB_Console_Printf("  BMS IND (PC0):   %s\r\n", FEB_HW_BMS_Indicator_Get() ? "ON" : "OFF");
  FEB_Console_Printf("  TSMS Indicator:  %s\r\n", FEB_HW_TSMS_Indicator_Get() ? "ON" : "OFF");

  FEB_Console_Printf("\r\nHV Safe: %s\r\n", FEB_HW_Is_HV_Safe() ? "YES" : "NO");
}

/* ============================================================================
 * Subcommand: ivt - Show IVT sensor data
 * ============================================================================ */
static void subcmd_ivt(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  const FEB_CAN_IVT_Data_t *ivt = FEB_CAN_IVT_GetData();
  uint32_t now = HAL_GetTick();
  uint32_t age = now - ivt->last_rx_tick;
  bool fresh = FEB_CAN_IVT_IsDataFresh(1000);

  FEB_Console_Printf("\r\n=== IVT Sensor Data ===\r\n");
  FEB_Console_Printf("Pack Current:  %.2f A\r\n", ivt->current_mA / 1000.0f);
  FEB_Console_Printf("Voltage 1:     %.2f V\r\n", ivt->voltage_1_mV / 1000.0f);
  FEB_Console_Printf("Voltage 2:     %.2f V\r\n", ivt->voltage_2_mV / 1000.0f);
  FEB_Console_Printf("Voltage 3:     %.2f V\r\n", ivt->voltage_3_mV / 1000.0f);
  FEB_Console_Printf("Pack Voltage:  %.2f V (U%d)\r\n", FEB_CAN_IVT_GetVoltage(), FEB_IVT_PACK_VOLTAGE_CHANNEL);
  FEB_Console_Printf("Temperature:   %.1f C\r\n", ivt->temperature_C);
  FEB_Console_Printf("Data Age:      %lu ms (%s)\r\n", (unsigned long)age, fresh ? "FRESH" : "STALE");
}

/* ============================================================================
 * Subcommand: tasks - Show FreeRTOS task stats
 * ============================================================================ */
static void subcmd_tasks(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== Task Stack Usage ===\r\n");
  FEB_Console_Printf("%-12s %-18s %s\r\n", "Task", "Stack Free (words)", "Status");
  FEB_Console_Printf("------------ ------------------ ------\r\n");

  struct
  {
    const char *name;
    osThreadId_t handle;
    uint32_t stack_size;
  } tasks[] = {
      {"uartRxTask", uartRxTaskHandle, 512}, {"ADBMSTask", ADBMSTaskHandle, 2048}, {"TPSTask", TPSTaskHandle, 256},
      {"BMSTaskRx", BMSTaskRxHandle, 512},   {"BMSTaskTx", BMSTaskTxHandle, 512},
  };

  for (size_t i = 0; i < sizeof(tasks) / sizeof(tasks[0]); i++)
  {
    if (tasks[i].handle != NULL)
    {
      UBaseType_t hwm = uxTaskGetStackHighWaterMark((TaskHandle_t)tasks[i].handle);
      const char *status = (hwm < 50) ? "LOW!" : "OK";
      FEB_Console_Printf("%-12s %-18u %s\r\n", tasks[i].name, (unsigned int)hwm, status);
    }
  }
}

/* ============================================================================
 * Subcommand: mem - Show memory usage
 * ============================================================================ */
static void subcmd_mem(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  size_t total = configTOTAL_HEAP_SIZE;
  size_t free_heap = xPortGetFreeHeapSize();
  size_t min_free = xPortGetMinimumEverFreeHeapSize();
  size_t used = total - free_heap;
  uint32_t percent = (used * 100) / (total > 0 ? total : 1);

  FEB_Console_Printf("\r\n=== Memory Usage ===\r\n");
  FEB_Console_Printf("Total Heap:    %u bytes\r\n", (unsigned int)total);
  FEB_Console_Printf("Free Heap:     %u bytes\r\n", (unsigned int)free_heap);
  FEB_Console_Printf("Min Free Ever: %u bytes\r\n", (unsigned int)min_free);
  FEB_Console_Printf("Used:          %u bytes (%u%%)\r\n", (unsigned int)used, (unsigned int)percent);
}

/* ============================================================================
 * Subcommand: cell - Show single cell details (C-code/S-code comparison)
 * ============================================================================ */
static void subcmd_cell(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: BMS|cell|<bank>|<cell>\r\n");
    FEB_Console_Printf("Banks: 1-%d, Cells: 1-%d\r\n", FEB_NBANKS, FEB_NUM_CELLS_PER_BANK);
    return;
  }

  int bank = atoi(argv[1]);
  int cell = atoi(argv[2]);

  if (bank < 1 || bank > FEB_NBANKS)
  {
    FEB_Console_Printf("Error: Bank must be 1-%d\r\n", FEB_NBANKS);
    return;
  }

  if (cell < 1 || cell > FEB_NUM_CELLS_PER_BANK)
  {
    FEB_Console_Printf("Error: Cell must be 1-%d\r\n", FEB_NUM_CELLS_PER_BANK);
    return;
  }

  int bank_idx = bank - 1;
  int cell_idx = cell - 1;
  float voltage_c = FEB_ADBMS_GET_Cell_Voltage((uint8_t)bank_idx, (uint16_t)cell_idx);
  float voltage_s = FEB_ADBMS_GET_Cell_Voltage_S((uint8_t)bank_idx, (uint16_t)cell_idx);
  float temp = FEB_ADBMS_GET_Cell_Temperature((uint8_t)bank_idx, (uint16_t)cell_idx);
  uint8_t violations = FEB_ADBMS_GET_Cell_Violations((uint8_t)bank_idx, (uint16_t)cell_idx);
  float delta = voltage_c - voltage_s;

  FEB_Console_Printf("\r\n=== Cell [Bank %d, Cell %d] ===\r\n", bank, cell);
  FEB_Console_Printf("Voltage (C):   %.3f V  (Primary)\r\n", voltage_c);
  FEB_Console_Printf("Voltage (S):   %.3f V  (Secondary)\r\n", voltage_s);
  FEB_Console_Printf("Delta:         %.4f V\r\n", delta);
  FEB_Console_Printf("Temperature:   %.1f C\r\n", temp);
  FEB_Console_Printf("Violations:    %d\r\n", violations);
}

/* ============================================================================
 * Subcommand: spi - Show isoSPI status
 * ============================================================================ */
static void subcmd_spi(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  const char *mode_str;
  switch (ISOSPI_MODE)
  {
  case ISOSPI_MODE_REDUNDANT:
    mode_str = "REDUNDANT";
    break;
  case ISOSPI_MODE_SPI1_ONLY:
    mode_str = "SPI1_ONLY";
    break;
  case ISOSPI_MODE_SPI2_ONLY:
    mode_str = "SPI2_ONLY";
    break;
  default:
    mode_str = "UNKNOWN";
    break;
  }

  FEB_Console_Printf("\r\n=== isoSPI Status ===\r\n");
  FEB_Console_Printf("Mode:             %s\r\n", mode_str);
  FEB_Console_Printf("Primary Channel:  SPI%d\r\n", ISOSPI_PRIMARY_CHANNEL);
  FEB_Console_Printf("Failover Thresh:  %d PEC errors\r\n", ISOSPI_FAILOVER_PEC_THRESHOLD);
}

/* ============================================================================
 * Subcommand: errors - Show error summary
 * ============================================================================ */
static const char *err_type_name(uint8_t err)
{
  switch (err)
  {
  case 0x00:
    return "None";
  case ERROR_TYPE_TEMP_VIOLATION:
    return "Temperature Violation";
  case ERROR_TYPE_LOW_TEMP_READS:
    return "Low Temp Reads";
  case ERROR_TYPE_VOLTAGE_VIOLATION:
    return "Voltage Violation";
  case ERROR_TYPE_INIT_FAILURE:
    return "Init Failure";
  default:
    return "Unknown";
  }
}

static void subcmd_errors(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  uint8_t err = FEB_ADBMS_Get_Error_Type();
  BMS_State_t state = FEB_SM_Get_Current_State();
  bool faulted = FEB_SM_Is_Faulted();

  FEB_Console_Printf("\r\n=== Error Summary ===\r\n");
  FEB_Console_Printf("Error Type:     0x%02X (%s)\r\n", err, err_type_name(err));
  FEB_Console_Printf("Current State:  %s\r\n", FEB_CAN_State_GetStateName(state));
  FEB_Console_Printf("Faulted:        %s\r\n", faulted ? "YES" : "NO");
  FEB_Console_Printf("HV Active:      %s\r\n", FEB_SM_Is_HV_Active() ? "YES" : "NO");
}

/* ============================================================================
 * Subcommand: config - Show configuration constants
 * ============================================================================ */
static void subcmd_config(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  const char *mode_str;
  switch (ISOSPI_MODE)
  {
  case ISOSPI_MODE_REDUNDANT:
    mode_str = "REDUNDANT";
    break;
  case ISOSPI_MODE_SPI1_ONLY:
    mode_str = "SPI1_ONLY";
    break;
  case ISOSPI_MODE_SPI2_ONLY:
    mode_str = "SPI2_ONLY";
    break;
  default:
    mode_str = "UNKNOWN";
    break;
  }

  FEB_Console_Printf("\r\n=== Configuration ===\r\n");
  FEB_Console_Printf("Banks:            %d\r\n", FEB_NBANKS);
  FEB_Console_Printf("ICs per Bank:     %d\r\n", FEB_NUM_ICPBANK);
  FEB_Console_Printf("Cells per Bank:   %d\r\n", FEB_NUM_CELLS_PER_BANK);
  FEB_Console_Printf("Temp Sensors:     %d\r\n", FEB_NUM_TEMP_SENSORS);
  FEB_Console_Printf("Total Cells:      %d\r\n", FEB_NBANKS * FEB_NUM_CELLS_PER_BANK);
  FEB_Console_Printf("isoSPI Mode:      %s\r\n", mode_str);
  FEB_Console_Printf("Max Cell V:       %.3f V\r\n", FEB_CELL_MAX_VOLTAGE_MV / 1000.0f);
  FEB_Console_Printf("Min Cell V:       %.3f V\r\n", FEB_CELL_MIN_VOLTAGE_MV / 1000.0f);
  FEB_Console_Printf("Max Cell Temp:    %.1f C\r\n", FEB_CELL_MAX_TEMP_DC / 10.0f);
  FEB_Console_Printf("Min Cell Temp:    %.1f C\r\n", FEB_CELL_MIN_TEMP_DC / 10.0f);
#if FEB_BMS_DISABLE_TEMP_CHECKS
  FEB_Console_Printf("Temp Checks:      DISABLED (BENCH MODE)\r\n");
#else
  FEB_Console_Printf("Temp Checks:      enabled\r\n");
#endif
#if FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS
  FEB_Console_Printf("Volt Checks (C):  DISABLED (BENCH MODE)\r\n");
#else
  FEB_Console_Printf("Volt Checks (C):  enabled\r\n");
#endif
#if FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS
  FEB_Console_Printf("Volt Checks (S):  DISABLED (BENCH MODE)\r\n");
#else
  FEB_Console_Printf("Volt Checks (S):  enabled\r\n");
#endif
}

/* ============================================================================
 * CAN Ping/Pong Subcommands
 * ============================================================================ */

static const char *mode_names[] = {"OFF", "PING", "PONG"};
static const uint32_t pingpong_frame_ids[] = {0xE0, 0xE1, 0xE2, 0xE3};

static void subcmd_ping(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: BMS|ping|<channel>\r\n");
    FEB_Console_Printf("Channels: 1-4 (Frame IDs 0xE0-0xE3)\r\n");
    return;
  }

  int ch = atoi(argv[1]);
  if (ch < 1 || ch > 4)
  {
    FEB_Console_Printf("Error: Channel must be 1-4\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)ch, PINGPONG_MODE_PING);
  FEB_Console_Printf("Channel %d (0x%02X): PING mode started\r\n", ch, (unsigned int)pingpong_frame_ids[ch - 1]);
}

static void subcmd_pong(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: BMS|pong|<channel>\r\n");
    FEB_Console_Printf("Channels: 1-4 (Frame IDs 0xE0-0xE3)\r\n");
    return;
  }

  int ch = atoi(argv[1]);
  if (ch < 1 || ch > 4)
  {
    FEB_Console_Printf("Error: Channel must be 1-4\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)ch, PINGPONG_MODE_PONG);
  FEB_Console_Printf("Channel %d (0x%02X): PONG mode started\r\n", ch, (unsigned int)pingpong_frame_ids[ch - 1]);
}

static void subcmd_canstop(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: BMS|canstop|<channel|all>\r\n");
    return;
  }

  if (FEB_strcasecmp(argv[1], "all") == 0)
  {
    FEB_CAN_PingPong_Reset();
    FEB_Console_Printf("All channels stopped\r\n");
    return;
  }

  int ch = atoi(argv[1]);
  if (ch < 1 || ch > 4)
  {
    FEB_Console_Printf("Error: Channel must be 1-4 or 'all'\r\n");
    return;
  }

  FEB_CAN_PingPong_SetMode((uint8_t)ch, PINGPONG_MODE_OFF);
  FEB_Console_Printf("Channel %d stopped\r\n", ch);
}

static void subcmd_canstatus(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("CAN Ping/Pong Status:\r\n");
  FEB_Console_Printf("%-3s %-6s %-5s %10s %10s %12s\r\n", "Ch", "FrameID", "Mode", "TX Count", "RX Count", "Last RX");
  FEB_Console_Printf("--- ------ ----- ---------- ---------- ------------\r\n");

  for (int ch = 1; ch <= 4; ch++)
  {
    FEB_PingPong_Mode_t mode = FEB_CAN_PingPong_GetMode((uint8_t)ch);
    uint32_t tx_count = FEB_CAN_PingPong_GetTxCount((uint8_t)ch);
    uint32_t rx_count = FEB_CAN_PingPong_GetRxCount((uint8_t)ch);
    int32_t last_rx = FEB_CAN_PingPong_GetLastCounter((uint8_t)ch);

    FEB_Console_Printf("%-3d 0x%02X   %-5s %10u %10u %12d\r\n", ch, (unsigned int)pingpong_frame_ids[ch - 1],
                       mode_names[mode], (unsigned int)tx_count, (unsigned int)rx_count, (int)last_rx);
  }
}

/* ============================================================================
 * Subcommand: cell-stats - Combined cell voltages and temperatures
 * ============================================================================ */
static void subcmd_cell_stats(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  subcmd_cells(0, NULL);
  subcmd_temps(0, NULL);
}

/* ============================================================================
 * Help Display
 * ============================================================================ */
static void print_bms_help(void)
{
  FEB_Console_Printf("BMS Commands:\r\n");
  FEB_Console_Printf("  BMS|status              - Show BMS status summary\r\n");
  FEB_Console_Printf("  BMS|cells               - Show all cell voltages\r\n");
  FEB_Console_Printf("  BMS|temps               - Show temperature readings\r\n");
  FEB_Console_Printf("  BMS|therm-raw           - Show raw thermistor voltages + ADC codes\r\n");
  FEB_Console_Printf("  BMS|state [<name>]      - Show/set BMS state\r\n");
  FEB_Console_Printf("  BMS|balance|on/off      - Control cell balancing\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("Diagnostics:\r\n");
  FEB_Console_Printf("  BMS|gpio                - Show hardware I/O status\r\n");
  FEB_Console_Printf("  BMS|ivt                 - Show IVT sensor data\r\n");
  FEB_Console_Printf("  BMS|tasks               - Show FreeRTOS task stats\r\n");
  FEB_Console_Printf("  BMS|mem                 - Show memory usage\r\n");
  FEB_Console_Printf("  BMS|cell|<b>|<c>        - Show single cell details\r\n");
  FEB_Console_Printf("  BMS|spi                 - Show isoSPI status\r\n");
  FEB_Console_Printf("  BMS|errors              - Show error summary\r\n");
  FEB_Console_Printf("  BMS|config              - Show configuration\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("Register Access:\r\n");
  FEB_Console_Printf("  BMS|reg|list            - List all ADBMS commands\r\n");
  FEB_Console_Printf("  BMS|reg|read|<name>     - Read register (e.g. RDCFGA)\r\n");
  FEB_Console_Printf("  BMS|reg|write|<name>|<hex> - Write register\r\n");
  FEB_Console_Printf("  BMS|reg|cmd|<name>      - Send command (e.g. ADCV)\r\n");
  FEB_Console_Printf("  BMS|reg|dump            - Dump all registers\r\n");
  FEB_Console_Printf("  BMS|reg|status          - Status summary\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("CAN Ping/Pong:\r\n");
  FEB_Console_Printf("  BMS|ping|<ch>           - Start ping mode (1-4)\r\n");
  FEB_Console_Printf("  BMS|pong|<ch>           - Start pong mode (1-4)\r\n");
  FEB_Console_Printf("  BMS|canstop|<ch|all>    - Stop channel(s)\r\n");
  FEB_Console_Printf("  BMS|canstatus           - Show CAN ping/pong status\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("CSV Protocol (machine-readable):\r\n");
  FEB_Console_Printf("  BMS|csv|<tx_id>|<sub>   - CSV-capable subs: status, cells, temps,\r\n");
  FEB_Console_Printf("                            therm-raw, state, gpio, ivt, tasks, mem,\r\n");
  FEB_Console_Printf("                            errors, config, canstatus, cell-stats\r\n");
  FEB_Console_Printf("  BMS|csv|<tx_id>|cell-stats - Voltages + temps (per cell / per sensor)\r\n");
  FEB_Console_Printf("  *|csv|<tx_id>|hello     - Discover all boards (system command)\r\n");
  FEB_Console_Printf("Each request emits: ack -> [rows] -> done\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("States: boot, lv_power, bus_health, precharge, energized,\r\n");
  FEB_Console_Printf("        drive, battery_free, charger_pre, charging, balance,\r\n");
  FEB_Console_Printf("        fault_bms, fault_bspd, fault_imd, fault_charging\r\n");
}

/* Forward declaration: the table-driven cmd_bms is defined after BMS_SUBCMDS
 * (which references the per-subcommand descriptors below the CSV handlers). */
static void cmd_bms(int argc, char *argv[]);

/* ============================================================================
 * CSV-Mode Handlers (one per spec command, registered at top level)
 * ============================================================================ */

/* Mandatory per spec. Emits voltage,<module>,<cell>,<primary>,<secondary>,<balancing>
 * for every cell, then temp,<module>,<sensor>,<temp> for every sensor.
 * "module" is the 1-indexed bank number; "balancing" is 1 if the cell's discharge
 * FET is active this balance cycle, else 0. */
static void cmd_cell_stats_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
    {
      float v_c = FEB_ADBMS_GET_Cell_Voltage(bank, cell);
      float v_s = FEB_ADBMS_GET_Cell_Voltage_S(bank, cell);
      uint8_t bal = FEB_ADBMS_GET_Cell_Discharging(bank, cell);
      FEB_Console_CsvEmit("voltage", "%d,%d,%.3f,%.3f,%d", bank + 1, cell + 1, v_c, v_s, bal);
    }
  }
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int sensor = 0; sensor < FEB_NUM_TEMP_SENSORS; sensor++)
    {
      float temp = FEB_ADBMS_GET_Cell_Temperature(bank, sensor);
      FEB_Console_CsvEmit("temp", "%d,%d,%.1f", bank + 1, sensor + 1, temp);
    }
  }
}

static void cmd_status_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  float min_c = 999.0f, max_c = 0.0f, min_s = 999.0f, max_s = 0.0f;
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
    {
      float v_c = FEB_ADBMS_GET_Cell_Voltage(bank, cell);
      float v_s = FEB_ADBMS_GET_Cell_Voltage_S(bank, cell);
      if (v_c > 0)
      {
        if (v_c < min_c)
          min_c = v_c;
        if (v_c > max_c)
          max_c = v_c;
      }
      if (v_s > 0)
      {
        if (v_s < min_s)
          min_s = v_s;
        if (v_s > max_s)
          max_s = v_s;
      }
    }
  }

  /* Body fields: state,pack_v,min_c,max_c,min_s,max_s,min_t,max_t,avg_t,balancing,err_type,balance_done,delta_mV */
  FEB_Console_CsvEmit("status", "%s,%.2f,%.3f,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%d,0x%02X,%d,%.0f",
                      FEB_CAN_State_GetStateName(FEB_SM_Get_Current_State()), FEB_ADBMS_GET_ACC_Total_Voltage(), min_c,
                      max_c, min_s, max_s, FEB_ADBMS_GET_ACC_MIN_Temp(), FEB_ADBMS_GET_ACC_MAX_Temp(),
                      FEB_ADBMS_GET_ACC_AVG_Temp(), FEB_Cell_Balancing_Status() ? 1 : 0, FEB_ADBMS_Get_Error_Type(),
                      FEB_Cell_Balance_Complete() ? 1 : 0, FEB_ADBMS_GET_Cell_Voltage_Delta_mV());
}

/* Emits voltage,<module>,<cell>,<primary>,<secondary>,<balancing> per cell.
 * "balancing" is 1 if the cell's discharge FET is active this cycle, else 0. */
static void cmd_cells_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
    {
      float v_c = FEB_ADBMS_GET_Cell_Voltage(bank, cell);
      float v_s = FEB_ADBMS_GET_Cell_Voltage_S(bank, cell);
      uint8_t bal = FEB_ADBMS_GET_Cell_Discharging(bank, cell);
      FEB_Console_CsvEmit("voltage", "%d,%d,%.3f,%.3f,%d", bank + 1, cell + 1, v_c, v_s, bal);
    }
  }
}

static void cmd_temps_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int sensor = 0; sensor < FEB_NUM_TEMP_SENSORS; sensor++)
    {
      float temp = FEB_ADBMS_GET_Cell_Temperature(bank, sensor);
      FEB_Console_CsvEmit("temp", "%d,%d,%.1f", bank + 1, sensor + 1, temp);
    }
  }
}

/* Body fields: module,ic,mux,channel,raw_code,voltage_mV
 * raw_code == 0xFFFF and voltage_mV == NaN indicates aux-PEC failure. */
static void cmd_therm_raw_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    for (int ic = 0; ic < FEB_NUM_ICPBANK; ic++)
    {
      for (int mux = 0; mux < 6; mux++)
      {
        for (int ch = 0; ch < 7; ch++)
        {
          uint16_t sensor = (uint16_t)(ic * FEB_NUM_TEMP_SENSE_PER_IC + mux * 7 + ch);
          uint16_t code = FEB_ADBMS_GET_Therm_Raw_Code((uint8_t)bank, sensor);
          float mV = FEB_ADBMS_GET_Therm_Raw_mV((uint8_t)bank, sensor);
          FEB_Console_CsvEmit("therm-raw", "%d,%d,%d,%d,0x%04X,%.1f", bank + 1, ic + 1, mux + 1, ch, code, mV);
        }
      }
    }
  }
}

static void cmd_state_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  BMS_State_t s = FEB_SM_Get_Current_State();
  FEB_Console_CsvEmit("state", "%s,%d", FEB_CAN_State_GetStateName(s), (int)s);
}

static void cmd_ivt_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  const FEB_CAN_IVT_Data_t *ivt = FEB_CAN_IVT_GetData();
  uint32_t now = HAL_GetTick();
  uint32_t age = now - ivt->last_rx_tick;
  bool fresh = FEB_CAN_IVT_IsDataFresh(1000);
  FEB_Console_CsvEmit("ivt", "%.3f,%.3f,%.3f,%.3f,%.1f,%lu,%d", ivt->current_mA / 1000.0f, ivt->voltage_1_mV / 1000.0f,
                      ivt->voltage_2_mV / 1000.0f, ivt->voltage_3_mV / 1000.0f, ivt->temperature_C, (unsigned long)age,
                      fresh ? 1 : 0);
}

static void cmd_errors_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  uint8_t err = FEB_ADBMS_Get_Error_Type();
  BMS_State_t state = FEB_SM_Get_Current_State();
  FEB_Console_CsvEmit("errors", "0x%02X,\"%s\",%s,%d,%d", err, err_type_name(err), FEB_CAN_State_GetStateName(state),
                      FEB_SM_Is_Faulted() ? 1 : 0, FEB_SM_Is_HV_Active() ? 1 : 0);
}

static void cmd_config_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  const char *mode_str;
  switch (ISOSPI_MODE)
  {
  case ISOSPI_MODE_REDUNDANT:
    mode_str = "REDUNDANT";
    break;
  case ISOSPI_MODE_SPI1_ONLY:
    mode_str = "SPI1_ONLY";
    break;
  case ISOSPI_MODE_SPI2_ONLY:
    mode_str = "SPI2_ONLY";
    break;
  default:
    mode_str = "UNKNOWN";
    break;
  }
  FEB_Console_CsvEmit("config", "%d,%d,%d,%d,%s,%.3f,%.3f,%.1f,%.1f", FEB_NBANKS, FEB_NUM_ICPBANK,
                      FEB_NUM_CELLS_PER_BANK, FEB_NUM_TEMP_SENSORS, mode_str, FEB_CELL_MAX_VOLTAGE_MV / 1000.0f,
                      FEB_CELL_MIN_VOLTAGE_MV / 1000.0f, FEB_CELL_MAX_TEMP_DC / 10.0f, FEB_CELL_MIN_TEMP_DC / 10.0f);
}

static void cmd_gpio_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  /* Body: air_plus_sense,precharge_sense,air_minus_sense,air_plus_sense,
   *       shutdown,imd_ok,tsms_ind,reset_btn,hv_safe  (all 0/1)
   * Note: cols 0 and 3 both report AIR+ sense — schema preserved for
   * compatibility with existing log consumers. Col 6 (formerly the bogus
   * "tsms_active" sense) now reports the TSMS indicator output level. */
  FEB_Console_CsvEmit("gpio", "%d,%d,%d,%d,%d,%d,%d,%d,%d", FEB_HW_AIR_Plus_Sense() == FEB_RELAY_STATE_CLOSE ? 1 : 0,
                      FEB_HW_Precharge_Sense() == FEB_RELAY_STATE_CLOSE ? 1 : 0,
                      FEB_HW_AIR_Minus_Sense() == FEB_RELAY_STATE_CLOSE ? 1 : 0,
                      FEB_HW_AIR_Plus_Sense() == FEB_RELAY_STATE_CLOSE ? 1 : 0,
                      FEB_HW_Shutdown_Sense() == FEB_RELAY_STATE_CLOSE ? 1 : 0,
                      FEB_HW_IMD_Sense() == FEB_RELAY_STATE_CLOSE ? 1 : 0, FEB_HW_TSMS_Indicator_Get() ? 1 : 0,
                      FEB_HW_Reset_Button_Pressed() ? 1 : 0, FEB_HW_Is_HV_Safe() ? 1 : 0);
}

static void cmd_mem_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  size_t total = configTOTAL_HEAP_SIZE;
  size_t free_heap = xPortGetFreeHeapSize();
  size_t min_free = xPortGetMinimumEverFreeHeapSize();
  size_t used = total - free_heap;
  uint32_t percent = (used * 100) / (total > 0 ? total : 1);
  FEB_Console_CsvEmit("mem", "%u,%u,%u,%u,%u", (unsigned)total, (unsigned)free_heap, (unsigned)min_free, (unsigned)used,
                      (unsigned)percent);
}

static void cmd_tasks_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  struct
  {
    const char *name;
    osThreadId_t handle;
  } tasks[] = {{"uartRxTask", uartRxTaskHandle},
               {"ADBMSTask", ADBMSTaskHandle},
               {"TPSTask", TPSTaskHandle},
               {"BMSTaskRx", BMSTaskRxHandle},
               {"BMSTaskTx", BMSTaskTxHandle}};
  for (size_t i = 0; i < sizeof(tasks) / sizeof(tasks[0]); i++)
  {
    if (tasks[i].handle != NULL)
    {
      UBaseType_t hwm = uxTaskGetStackHighWaterMark((TaskHandle_t)tasks[i].handle);
      FEB_Console_CsvEmit("task", "%s,%u", tasks[i].name, (unsigned)hwm);
    }
  }
}

static void cmd_canstatus_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  for (int ch = 1; ch <= 4; ch++)
  {
    FEB_PingPong_Mode_t mode = FEB_CAN_PingPong_GetMode((uint8_t)ch);
    uint32_t tx_count = FEB_CAN_PingPong_GetTxCount((uint8_t)ch);
    uint32_t rx_count = FEB_CAN_PingPong_GetRxCount((uint8_t)ch);
    int32_t last_rx = FEB_CAN_PingPong_GetLastCounter((uint8_t)ch);
    FEB_Console_CsvEmit("can", "%d,0x%02X,%s,%u,%u,%d", ch, (unsigned int)pingpong_frame_ids[ch - 1], mode_names[mode],
                        (unsigned)tx_count, (unsigned)rx_count, (int)last_rx);
  }
}

/* ============================================================================
 * Subcommand: volts - ADBMS6830 supply/reference voltages (VREF2/VA/VD/ITMP)
 *
 * Triggers an ADSTAT-all conversion, briefly waits, then reads STATA + STATB
 * for every IC in the daisy chain and prints a per-IC table.
 * ============================================================================ */
static void subcmd_volts(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  /* Trigger ADSTAT-all (CH=0). The ADBMS6830 datasheet opcode for a status
   * conversion of all channels is 0x0468. Sent directly because the repo's
   * `#define ADSTAT 0x0570` in FEB_CMDCODES.h is a pre-existing mistake — it
   * collides with CMD_START_AUX_ADC and is unused (ADBMS6830B_adax builds
   * its own bytes). Wait ~1 ms for the conversion to settle. */
  ADBMS_SendCmd(0x0468);
  osDelay(pdMS_TO_TICKS(1));

  FEB_Console_Printf("\r\n=== ADBMS6830 Supply Voltages ===\r\n");
  FEB_Console_Printf("IC   VREF2(V)  VA(V)    VD(V)    ITMP(C)\r\n");

  for (uint8_t ic = 0; ic < FEB_NUM_IC; ic++)
  {
    ADBMS_STATA_t a = {0};
    ADBMS_STATB_t b = {0};
    ADBMS_ReadReg(RDSTATA, ic, a.raw);
    ADBMS_ReadReg(RDSTATB, ic, b.raw);

    float vref2 = ADBMS_CodeToVoltage_mV(a.values.VREF2) / 1000.0f;
    float va = ADBMS_CodeToVoltage_mV(a.values.VA) / 1000.0f;
    float vd = ADBMS_CodeToVoltage_mV(b.bits.VD) / 1000.0f;
    float itmp = ADBMS_CodeToTemp_C(a.values.ITMP);

    FEB_Console_Printf("%-2u   %7.3f   %7.3f  %7.3f  %6.1f\r\n", (unsigned)ic, vref2, va, vd, itmp);
  }
}

/* ----------------------------------------------------------------------------
 * CSV handlers for the remaining diagnostic subcommands. Each mirrors the
 * fields its text handler prints, comma-separated with no human labels, so CSV
 * hosts can drive every BMS command. ack / done framing is automatic.
 * -------------------------------------------------------------------------- */

static void cmd_balance_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    /* Body: balancing,state */
    FEB_Console_CsvEmit("balance", "%d,%s", FEB_Cell_Balancing_Status() ? 1 : 0,
                        FEB_CAN_State_GetStateName(FEB_SM_Get_Current_State()));
    return;
  }
  if (FEB_strcasecmp(argv[1], "on") == 0)
  {
    if (!is_balancing_allowed())
    {
      FEB_Console_CsvError("error", "not_allowed,%s", FEB_CAN_State_GetStateName(FEB_SM_Get_Current_State()));
      return;
    }
    FEB_Cell_Balance_Start();
    if (FEB_SM_Get_Current_State() == BMS_STATE_BATTERY_FREE)
    {
      FEB_SM_Transition(BMS_STATE_BALANCE); /* 6->8 begin_balance */
    }
    FEB_Console_CsvEmit("balance", "%d", 1);
  }
  else if (FEB_strcasecmp(argv[1], "off") == 0)
  {
    FEB_Stop_Balance();
    if (FEB_SM_Get_Current_State() == BMS_STATE_BALANCE)
    {
      FEB_SM_Transition(BMS_STATE_BATTERY_FREE); /* 8->6 stop_balance */
    }
    FEB_Console_CsvEmit("balance", "%d", 0);
  }
  else
  {
    FEB_Console_CsvError("error", "invalid_option,%s", argv[1]);
  }
}

static void cmd_cell_csv(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_CsvError("error", "usage,cell|<bank>|<cell>");
    return;
  }
  int bank = atoi(argv[1]);
  int cell = atoi(argv[2]);
  if (bank < 1 || bank > FEB_NBANKS)
  {
    FEB_Console_CsvError("error", "invalid_bank,%d", bank);
    return;
  }
  if (cell < 1 || cell > FEB_NUM_CELLS_PER_BANK)
  {
    FEB_Console_CsvError("error", "invalid_cell,%d", cell);
    return;
  }
  int bank_idx = bank - 1;
  int cell_idx = cell - 1;
  float voltage_c = FEB_ADBMS_GET_Cell_Voltage((uint8_t)bank_idx, (uint16_t)cell_idx);
  float voltage_s = FEB_ADBMS_GET_Cell_Voltage_S((uint8_t)bank_idx, (uint16_t)cell_idx);
  float temp = FEB_ADBMS_GET_Cell_Temperature((uint8_t)bank_idx, (uint16_t)cell_idx);
  uint8_t violations = FEB_ADBMS_GET_Cell_Violations((uint8_t)bank_idx, (uint16_t)cell_idx);
  /* Body: bank,cell,v_primary,v_secondary,delta,temp,violations */
  FEB_Console_CsvEmit("cell", "%d,%d,%.3f,%.3f,%.4f,%.1f,%d", bank, cell, voltage_c, voltage_s, voltage_c - voltage_s,
                      temp, violations);
}

static void cmd_spi_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  const char *mode_str;
  switch (ISOSPI_MODE)
  {
  case ISOSPI_MODE_REDUNDANT:
    mode_str = "REDUNDANT";
    break;
  case ISOSPI_MODE_SPI1_ONLY:
    mode_str = "SPI1_ONLY";
    break;
  case ISOSPI_MODE_SPI2_ONLY:
    mode_str = "SPI2_ONLY";
    break;
  default:
    mode_str = "UNKNOWN";
    break;
  }
  /* Body: mode,primary_channel,failover_pec_threshold */
  FEB_Console_CsvEmit("spi", "%s,%d,%d", mode_str, ISOSPI_PRIMARY_CHANNEL, ISOSPI_FAILOVER_PEC_THRESHOLD);
}

static void cmd_ping_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvError("error", "usage,ping|<1-4>");
    return;
  }
  int ch = atoi(argv[1]);
  if (ch < 1 || ch > 4)
  {
    FEB_Console_CsvError("error", "invalid_channel,%d", ch);
    return;
  }
  FEB_CAN_PingPong_SetMode((uint8_t)ch, PINGPONG_MODE_PING);
  /* Body: channel,frame_id,mode */
  FEB_Console_CsvEmit("ping", "%d,0x%02X,started", ch, (unsigned int)pingpong_frame_ids[ch - 1]);
}

static void cmd_pong_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvError("error", "usage,pong|<1-4>");
    return;
  }
  int ch = atoi(argv[1]);
  if (ch < 1 || ch > 4)
  {
    FEB_Console_CsvError("error", "invalid_channel,%d", ch);
    return;
  }
  FEB_CAN_PingPong_SetMode((uint8_t)ch, PINGPONG_MODE_PONG);
  /* Body: channel,frame_id,mode */
  FEB_Console_CsvEmit("pong", "%d,0x%02X,started", ch, (unsigned int)pingpong_frame_ids[ch - 1]);
}

static void cmd_canstop_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvError("error", "usage,canstop|<1-4|all>");
    return;
  }
  if (FEB_strcasecmp(argv[1], "all") == 0)
  {
    FEB_CAN_PingPong_Reset();
    FEB_Console_CsvEmit("canstop", "all,stopped");
    return;
  }
  int ch = atoi(argv[1]);
  if (ch < 1 || ch > 4)
  {
    FEB_Console_CsvError("error", "invalid_channel,%d", ch);
    return;
  }
  FEB_CAN_PingPong_SetMode((uint8_t)ch, PINGPONG_MODE_OFF);
  FEB_Console_CsvEmit("canstop", "%d,stopped", ch);
}

/* Mirrors subcmd_volts: triggers an ADSTAT-all conversion then emits one row
 * per IC. Body: ic,vref2,va,vd,itmp. */
static void cmd_volts_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  ADBMS_SendCmd(0x0468);
  osDelay(pdMS_TO_TICKS(1));
  for (uint8_t ic = 0; ic < FEB_NUM_IC; ic++)
  {
    ADBMS_STATA_t a = {0};
    ADBMS_STATB_t b = {0};
    ADBMS_ReadReg(RDSTATA, ic, a.raw);
    ADBMS_ReadReg(RDSTATB, ic, b.raw);
    float vref2 = ADBMS_CodeToVoltage_mV(a.values.VREF2) / 1000.0f;
    float va = ADBMS_CodeToVoltage_mV(a.values.VA) / 1000.0f;
    float vd = ADBMS_CodeToVoltage_mV(b.bits.VD) / 1000.0f;
    float itmp = ADBMS_CodeToTemp_C(a.values.ITMP);
    FEB_Console_CsvEmit("volts", "%u,%.3f,%.3f,%.3f,%.1f", (unsigned)ic, vref2, va, vd, itmp);
  }
}

/* ============================================================================
 * Command Descriptors
 *
 * One unified FEB_Console_Cmd_t per subcommand: .handler is the human text
 * impl, .csv_handler is the machine-readable CSV impl. Either may be NULL if
 * that mode isn't supported. Each is registered top-level so the CSV parser's
 * `BMS|csv|<tx_id>|<name>` lookup resolves directly; bare-name text invocation
 * also works as a shorthand. The canonical text form remains `BMS|<sub>` via
 * the cmd_bms mega-dispatcher.
 * ============================================================================ */
static const FEB_Console_Cmd_t bms_status_cmd = {.name = "status",
                                                 .help = "Status summary",
                                                 .handler = subcmd_status,
                                                 .csv_handler = cmd_status_csv,
                                                 .hidden = true};
static const FEB_Console_Cmd_t bms_cells_cmd = {.name = "cells",
                                                .help = "Per-cell voltages (C/S)",
                                                .handler = subcmd_cells,
                                                .csv_handler = cmd_cells_csv,
                                                .hidden = true};
static const FEB_Console_Cmd_t bms_temps_cmd = {.name = "temps",
                                                .help = "Per-sensor temperatures",
                                                .handler = subcmd_temps,
                                                .csv_handler = cmd_temps_csv,
                                                .hidden = true};
static const FEB_Console_Cmd_t bms_therm_raw_cmd = {.name = "therm-raw",
                                                    .help = "Raw thermistor voltages and ADC codes (42 channels)",
                                                    .handler = subcmd_therm_raw,
                                                    .csv_handler = cmd_therm_raw_csv,
                                                    .hidden = true};
static const FEB_Console_Cmd_t bms_state_cmd = {.name = "state",
                                                .help = "Show/set BMS state",
                                                .handler = subcmd_state,
                                                .csv_handler = cmd_state_csv,
                                                .hidden = true};
static const FEB_Console_Cmd_t bms_balance_cmd = {.name = "balance",
                                                  .help = "Control cell balancing (on/off)",
                                                  .handler = subcmd_balance,
                                                  .csv_handler = cmd_balance_csv,
                                                  .hidden = true};
static const FEB_Console_Cmd_t bms_gpio_cmd = {
    .name = "gpio", .help = "Hardware I/O status", .handler = subcmd_gpio, .csv_handler = cmd_gpio_csv, .hidden = true};
static const FEB_Console_Cmd_t bms_ivt_cmd = {
    .name = "ivt", .help = "IVT sensor data", .handler = subcmd_ivt, .csv_handler = cmd_ivt_csv, .hidden = true};
static const FEB_Console_Cmd_t bms_tasks_cmd = {.name = "tasks",
                                                .help = "FreeRTOS task stack stats",
                                                .handler = subcmd_tasks,
                                                .csv_handler = cmd_tasks_csv,
                                                .hidden = true};
static const FEB_Console_Cmd_t bms_mem_cmd = {
    .name = "mem", .help = "Heap usage", .handler = subcmd_mem, .csv_handler = cmd_mem_csv, .hidden = true};
static const FEB_Console_Cmd_t bms_cell_cmd = {.name = "cell",
                                               .help = "Single cell details: cell|<bank>|<cell>",
                                               .handler = subcmd_cell,
                                               .csv_handler = cmd_cell_csv,
                                               .hidden = true};
static const FEB_Console_Cmd_t bms_spi_cmd = {
    .name = "spi", .help = "isoSPI status", .handler = subcmd_spi, .csv_handler = cmd_spi_csv, .hidden = true};
static const FEB_Console_Cmd_t bms_errors_cmd = {
    .name = "errors", .help = "Error summary", .handler = subcmd_errors, .csv_handler = cmd_errors_csv, .hidden = true};
static const FEB_Console_Cmd_t bms_config_cmd = {.name = "config",
                                                 .help = "Configuration constants",
                                                 .handler = subcmd_config,
                                                 .csv_handler = cmd_config_csv,
                                                 .hidden = true};
static const FEB_Console_Cmd_t bms_ping_cmd = {.name = "ping",
                                               .help = "Start CAN ping: ping|<1-4>",
                                               .handler = subcmd_ping,
                                               .csv_handler = cmd_ping_csv,
                                               .hidden = true};
static const FEB_Console_Cmd_t bms_pong_cmd = {.name = "pong",
                                               .help = "Start CAN pong: pong|<1-4>",
                                               .handler = subcmd_pong,
                                               .csv_handler = cmd_pong_csv,
                                               .hidden = true};
static const FEB_Console_Cmd_t bms_canstop_cmd = {.name = "canstop",
                                                  .help = "Stop CAN ch: canstop|<1-4|all>",
                                                  .handler = subcmd_canstop,
                                                  .csv_handler = cmd_canstop_csv,
                                                  .hidden = true};
static const FEB_Console_Cmd_t bms_canstatus_cmd = {.name = "canstatus",
                                                    .help = "CAN ping/pong status",
                                                    .handler = subcmd_canstatus,
                                                    .csv_handler = cmd_canstatus_csv,
                                                    .hidden = true};
static const FEB_Console_Cmd_t bms_cell_stats_cmd = {.name = "cell-stats",
                                                     .help = "Voltages + temps (per cell / per sensor)",
                                                     .handler = subcmd_cell_stats,
                                                     .csv_handler = cmd_cell_stats_csv,
                                                     .hidden = true};
static const FEB_Console_Cmd_t bms_reg_cmd = {.name = "reg",
                                              .help = "ADBMS register access (reg|list, reg|read|<n>, reg|dump, ...)",
                                              .handler = ADBMS_RegSubcmd,
                                              .csv_handler = ADBMS_RegSubcmd_Csv,
                                              .hidden = true};
static const FEB_Console_Cmd_t bms_volts_cmd = {.name = "volts",
                                                .help = "ADBMS supply/reference voltages (VREF2/VA/VD/ITMP per IC)",
                                                .handler = subcmd_volts,
                                                .csv_handler = cmd_volts_csv,
                                                .hidden = true};

/* Per-board subcommand table. cmd_bms iterates this for `BMS|<sub>` dispatch.
 * Adding a subcommand: define its FEB_Console_Cmd_t above and append a pointer
 * here. Registration auto-picks it up. */
static const FEB_Console_Cmd_t *const BMS_SUBCMDS[] = {
    &bms_status_cmd,     &bms_cells_cmd,  &bms_temps_cmd, &bms_therm_raw_cmd, &bms_state_cmd,   &bms_balance_cmd,
    &bms_gpio_cmd,       &bms_ivt_cmd,    &bms_tasks_cmd, &bms_mem_cmd,       &bms_cell_cmd,    &bms_spi_cmd,
    &bms_errors_cmd,     &bms_config_cmd, &bms_ping_cmd,  &bms_pong_cmd,      &bms_canstop_cmd, &bms_canstatus_cmd,
    &bms_cell_stats_cmd, &bms_reg_cmd,    &bms_volts_cmd,
};
#define BMS_SUBCMDS_COUNT (sizeof(BMS_SUBCMDS) / sizeof(BMS_SUBCMDS[0]))

/* Mega-dispatcher: `BMS|<sub>|<args>` looks up `<sub>` in the table and calls
 * its text handler. Bare `<sub>` (no BMS prefix) also works because each
 * subcommand is registered top-level. */
static void cmd_bms(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_bms_help();
    return;
  }
  const char *subcmd = argv[1];
  /* Legacy alias: "stop" maps to canstop. */
  if (FEB_strcasecmp(subcmd, "stop") == 0)
  {
    subcmd_canstop(argc - 1, argv + 1);
    return;
  }
  for (size_t i = 0; i < BMS_SUBCMDS_COUNT; i++)
  {
    if (FEB_strcasecmp(BMS_SUBCMDS[i]->name, subcmd) == 0)
    {
      BMS_SUBCMDS[i]->handler(argc - 1, argv + 1);
      return;
    }
  }
  FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
  print_bms_help();
}

/* Text-mode entry point: `BMS|<subcmd>|<args>` dispatches via cmd_bms. */
static const FEB_Console_Cmd_t bms_cmd = {
    .name = "BMS",
    .help = "BMS commands (BMS|<sub>) - run BMS alone for full list",
    .handler = cmd_bms,
    .csv_handler = NULL,
};

/* ============================================================================
 * Registration
 * ============================================================================ */
void BMS_RegisterCommands(void)
{
  int rc = FEB_Console_Register(&bms_cmd);
  if (rc != 0)
  {
    LOG_E(TAG_BMS, "Failed to register '%s' (rc=%d)", bms_cmd.name, rc);
  }
  for (size_t i = 0; i < BMS_SUBCMDS_COUNT; i++)
  {
    rc = FEB_Console_Register(BMS_SUBCMDS[i]);
    if (rc != 0)
    {
      LOG_E(TAG_BMS, "Failed to register '%s' (rc=%d)", BMS_SUBCMDS[i]->name, rc);
    }
  }
}
