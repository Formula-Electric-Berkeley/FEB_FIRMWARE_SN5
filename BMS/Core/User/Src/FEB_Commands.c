/**
 ******************************************************************************
 * @file           : FEB_Commands.c
 * @brief          : BMS-specific console commands
 * @author         : Formula Electric @ Berkeley
 *
 * Commands follow hierarchical pattern: BMS|subcommand|args
 ******************************************************************************
 */

#include "FEB_Commands.h"
#include "feb_console.h"
#include "feb_uart_log.h"
#include "FEB_ADBMS6830B.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_CAN_State.h"
#include "FEB_Const.h"
#include "FEB_SM.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Logging tag */
#define TAG_BMS "[BMS]"

/* Forward declaration */
static int strcasecmp_local(const char *s1, const char *s2);

/* ============================================================================
 * Subcommand: status - Show BMS status summary
 * ============================================================================ */
static void subcmd_status(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== BMS Status ===\r\n");
  FEB_Console_Printf("State: %s\r\n", FEB_CAN_State_GetStateName(FEB_SM_Get_Current_State()));
  FEB_Console_Printf("Pack Voltage: %.2fV\r\n", FEB_ADBMS_GET_ACC_Total_Voltage());
  FEB_Console_Printf("Min Cell: %.3fV  Max Cell: %.3fV\r\n", FEB_ADBMS_GET_ACC_MIN_Voltage(),
                     FEB_ADBMS_GET_ACC_MAX_Voltage());
  FEB_Console_Printf("Min Temp: %.1fC  Max Temp: %.1fC  Avg: %.1fC\r\n", FEB_ADBMS_GET_ACC_MIN_Temp(),
                     FEB_ADBMS_GET_ACC_MAX_Temp(), FEB_ADBMS_GET_ACC_AVG_Temp());
  FEB_Console_Printf("Balancing: %s\r\n", FEB_Cell_Balancing_Status() ? "ON" : "OFF");
  FEB_Console_Printf("Error Type: 0x%02X\r\n", FEB_ADBMS_Get_Error_Type());
}

/* ============================================================================
 * Subcommand: cells - Show individual cell voltages
 * ============================================================================ */
static void subcmd_cells(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== Cell Voltages ===\r\n");
  for (int bank = 0; bank < FEB_NBANKS; bank++)
  {
    FEB_Console_Printf("Bank %d: ", bank);
    for (int cell = 0; cell < FEB_NUM_CELLS_PER_BANK; cell++)
    {
      FEB_Console_Printf("%.3f ", FEB_ADBMS_GET_Cell_Voltage(bank, cell));
    }
    FEB_Console_Printf("\r\n");
  }
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
    FEB_Console_Printf("Bank %d: ", bank);
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

  if (argv[1][0] == 'o' && argv[1][1] == 'n')
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
    FEB_Console_Printf("Balancing started\r\n");
  }
  else if (argv[1][0] == 'o' && argv[1][1] == 'f')
  {
    FEB_Stop_Balance();
    FEB_Console_Printf("Balancing stopped\r\n");
  }
  else
  {
    FEB_Console_Printf("Unknown option: %s\r\n", argv[1]);
    FEB_Console_Printf("Usage: BMS|balance|on  or  BMS|balance|off\r\n");
  }
}

/* ============================================================================
 * Subcommand: dump - Print full accumulator status
 * ============================================================================ */
static void subcmd_dump(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_ADBMS_Print_Accumulator();
}

/* ============================================================================
 * Subcommand: state - Show/set BMS state (with safety restrictions)
 * ============================================================================ */

/**
 * @brief Check if a state transition is allowed via console command
 * @param current Current BMS state
 * @param target Target BMS state
 * @return true if transition is allowed
 *
 * Allowed transitions:
 * - Any fault state (FAULT_BMS, FAULT_BSPD, FAULT_IMD, FAULT_CHARGING)
 * - ENERGIZED <-> DRIVE (overrides R2D signal)
 * - LV_POWER or BUS_HEALTH_CHECK -> BATTERY_FREE
 */
static bool is_state_transition_allowed(BMS_State_t current, BMS_State_t target)
{
  /* Always allow entering fault states */
  if (target >= BMS_STATE_FAULT_BMS && target <= BMS_STATE_FAULT_CHARGING)
  {
    return true;
  }

  /* Allow R2D override: ENERGIZED <-> DRIVE */
  if ((current == BMS_STATE_ENERGIZED && target == BMS_STATE_DRIVE) ||
      (current == BMS_STATE_DRIVE && target == BMS_STATE_ENERGIZED))
  {
    return true;
  }

  /* BATTERY_FREE only allowed from LV_POWER or BUS_HEALTH_CHECK */
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
    if (strcasecmp_local(arg, "boot") == 0)
      new_state = BMS_STATE_BOOT;
    else if (strcasecmp_local(arg, "lv_power") == 0 || strcasecmp_local(arg, "lv") == 0)
      new_state = BMS_STATE_LV_POWER;
    else if (strcasecmp_local(arg, "bus_health") == 0 || strcasecmp_local(arg, "bus") == 0)
      new_state = BMS_STATE_BUS_HEALTH_CHECK;
    else if (strcasecmp_local(arg, "precharge") == 0 || strcasecmp_local(arg, "pre") == 0)
      new_state = BMS_STATE_PRECHARGE;
    else if (strcasecmp_local(arg, "energized") == 0)
      new_state = BMS_STATE_ENERGIZED;
    else if (strcasecmp_local(arg, "drive") == 0)
      new_state = BMS_STATE_DRIVE;
    else if (strcasecmp_local(arg, "battery_free") == 0 || strcasecmp_local(arg, "free") == 0)
      new_state = BMS_STATE_BATTERY_FREE;
    else if (strcasecmp_local(arg, "charger_precharge") == 0 || strcasecmp_local(arg, "charger_pre") == 0)
      new_state = BMS_STATE_CHARGER_PRECHARGE;
    else if (strcasecmp_local(arg, "charging") == 0 || strcasecmp_local(arg, "charge") == 0)
      new_state = BMS_STATE_CHARGING;
    else if (strcasecmp_local(arg, "balance") == 0 || strcasecmp_local(arg, "bal") == 0)
      new_state = BMS_STATE_BALANCE;
    else if (strcasecmp_local(arg, "fault_bms") == 0 || strcasecmp_local(arg, "fault") == 0)
      new_state = BMS_STATE_FAULT_BMS;
    else if (strcasecmp_local(arg, "fault_bspd") == 0)
      new_state = BMS_STATE_FAULT_BSPD;
    else if (strcasecmp_local(arg, "fault_imd") == 0)
      new_state = BMS_STATE_FAULT_IMD;
    else if (strcasecmp_local(arg, "fault_charging") == 0)
      new_state = BMS_STATE_FAULT_CHARGING;
    else
    {
      FEB_Console_Printf("Unknown state: %s\r\n", arg);
      return;
    }
  }

  /* Check if transition is allowed */
  if (!is_state_transition_allowed(current_state, new_state))
  {
    FEB_Console_Printf("Error: Transition %s -> %s not allowed\r\n", FEB_CAN_State_GetStateName(current_state),
                       FEB_CAN_State_GetStateName(new_state));
    FEB_Console_Printf("Allowed: ENERGIZED<->DRIVE, LV/BUS_HEALTH->BATTERY_FREE, ->FAULT_*\r\n");
    return;
  }

  /* Use state machine transition for proper relay handling */
  FEB_SM_Transition(new_state);
  FEB_Console_Printf("State transition requested: %s -> %s\r\n", FEB_CAN_State_GetStateName(current_state),
                     FEB_CAN_State_GetStateName(new_state));
}

/* ============================================================================
 * CAN Ping/Pong Subcommands
 * ============================================================================ */

static int strcasecmp_local(const char *s1, const char *s2)
{
  while (*s1 && *s2)
  {
    int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
    if (diff != 0)
      return diff;
    s1++;
    s2++;
  }
  return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

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

  if (strcasecmp_local(argv[1], "all") == 0)
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
 * Help Display
 * ============================================================================ */
static void print_bms_help(void)
{
  FEB_Console_Printf("BMS Commands:\r\n");
  FEB_Console_Printf("  BMS|status              - Show BMS status summary\r\n");
  FEB_Console_Printf("  BMS|cells               - Show all cell voltages\r\n");
  FEB_Console_Printf("  BMS|temps               - Show temperature readings\r\n");
  FEB_Console_Printf("  BMS|state [<name>]      - Show/set BMS state\r\n");
  FEB_Console_Printf("  BMS|balance|on/off      - Control cell balancing\r\n");
  FEB_Console_Printf("  BMS|dump                - Full accumulator status\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("CAN Ping/Pong:\r\n");
  FEB_Console_Printf("  BMS|ping|<ch>           - Start ping mode (1-4)\r\n");
  FEB_Console_Printf("  BMS|pong|<ch>           - Start pong mode (1-4)\r\n");
  FEB_Console_Printf("  BMS|canstop|<ch|all>    - Stop channel(s)\r\n");
  FEB_Console_Printf("  BMS|canstatus           - Show CAN ping/pong status\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("States: boot, lv_power, bus_health, precharge, energized,\r\n");
  FEB_Console_Printf("        drive, battery_free, charger_pre, charging, balance,\r\n");
  FEB_Console_Printf("        fault_bms, fault_bspd, fault_imd, fault_charging\r\n");
}

/* ============================================================================
 * Main BMS Command Handler
 * ============================================================================ */
static void cmd_bms(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_bms_help();
    return;
  }

  const char *subcmd = argv[1];

  /* Dispatch to subcommand handler */
  if (strcasecmp_local(subcmd, "status") == 0)
  {
    subcmd_status(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "cells") == 0)
  {
    subcmd_cells(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "temps") == 0)
  {
    subcmd_temps(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "state") == 0)
  {
    subcmd_state(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "balance") == 0)
  {
    subcmd_balance(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "dump") == 0)
  {
    subcmd_dump(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "ping") == 0)
  {
    subcmd_ping(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "pong") == 0)
  {
    subcmd_pong(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "canstop") == 0 || strcasecmp_local(subcmd, "stop") == 0)
  {
    subcmd_canstop(argc - 1, argv + 1);
  }
  else if (strcasecmp_local(subcmd, "canstatus") == 0)
  {
    subcmd_canstatus(argc - 1, argv + 1);
  }
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_bms_help();
  }
}

/* ============================================================================
 * Command Descriptor
 * ============================================================================ */
static const FEB_Console_Cmd_t bms_cmd = {
    .name = "BMS",
    .help = "BMS commands (BMS|status, BMS|cells, BMS|state, etc.)",
    .handler = cmd_bms,
};

/* ============================================================================
 * Registration
 * ============================================================================ */
void BMS_RegisterCommands(void)
{
  FEB_Console_Register(&bms_cmd);
}
