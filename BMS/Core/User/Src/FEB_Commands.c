/**
 ******************************************************************************
 * @file           : FEB_Commands.c
 * @brief          : BMS-specific console commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Commands.h"
#include "feb_console.h"
#include "FEB_ADBMS6830B.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_CAN_State.h"
#include "FEB_Const.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration */
static int strcasecmp_local(const char *s1, const char *s2);

/* ============================================================================
 * Command: status - Show BMS status summary
 * ============================================================================ */
static void cmd_status(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== BMS Status ===\r\n");
  FEB_Console_Printf("Pack Voltage: %.2fV\r\n", FEB_ADBMS_GET_ACC_Total_Voltage());
  FEB_Console_Printf("Min Cell: %.3fV  Max Cell: %.3fV\r\n", FEB_ADBMS_GET_ACC_MIN_Voltage(),
                     FEB_ADBMS_GET_ACC_MAX_Voltage());
  FEB_Console_Printf("Min Temp: %.1fC  Max Temp: %.1fC  Avg: %.1fC\r\n", FEB_ADBMS_GET_ACC_MIN_Temp(),
                     FEB_ADBMS_GET_ACC_MAX_Temp(), FEB_ADBMS_GET_ACC_AVG_Temp());
  FEB_Console_Printf("Balancing: %s\r\n", FEB_Cell_Balancing_Status() ? "ON" : "OFF");
  FEB_Console_Printf("Error Type: 0x%02X\r\n", FEB_ADBMS_Get_Error_Type());
}

static const FEB_Console_Cmd_t bms_cmd_status = {
    .name = "status",
    .help = "Show BMS status summary",
    .handler = cmd_status,
};

/* ============================================================================
 * Command: cells - Show individual cell voltages
 * ============================================================================ */
static void cmd_cells(int argc, char *argv[])
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

static const FEB_Console_Cmd_t bms_cmd_cells = {
    .name = "cells",
    .help = "Show all cell voltages by bank",
    .handler = cmd_cells,
};

/* ============================================================================
 * Command: temps - Show temperature sensor readings
 * ============================================================================ */
static void cmd_temps(int argc, char *argv[])
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

static const FEB_Console_Cmd_t bms_cmd_temps = {
    .name = "temps",
    .help = "Show temperature readings by bank",
    .handler = cmd_temps,
};

/* ============================================================================
 * Command: balance - Show/control cell balancing
 * ============================================================================ */
static void cmd_balance(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Balancing: %s\r\n", FEB_Cell_Balancing_Status() ? "ON" : "OFF");
    FEB_Console_Printf("Usage: balance|on  or  balance|off\r\n");
    return;
  }

  if (argv[1][0] == 'o' && argv[1][1] == 'n')
  {
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
    FEB_Console_Printf("Usage: balance|on  or  balance|off\r\n");
  }
}

static const FEB_Console_Cmd_t bms_cmd_balance = {
    .name = "balance",
    .help = "Control cell balancing: balance|on/off",
    .handler = cmd_balance,
};

/* ============================================================================
 * Command: dump - Print full accumulator status
 * ============================================================================ */
static void cmd_dump(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_ADBMS_Print_Accumulator();
}

static const FEB_Console_Cmd_t bms_cmd_dump = {
    .name = "dump",
    .help = "Print full accumulator status",
    .handler = cmd_dump,
};

/* ============================================================================
 * Command: state - Show/set BMS state
 * ============================================================================ */
static void cmd_state(int argc, char *argv[])
{
  if (argc < 2)
  {
    BMS_State_t state = FEB_CAN_State_GetState();
    FEB_Console_Printf("BMS State: %s (%d)\r\n", FEB_CAN_State_GetStateName(state), state);
    FEB_Console_Printf("Usage: state <name|number>\r\n");
    FEB_Console_Printf("States: boot(0), origin(1), lv_power(2), bus_health(3),\r\n");
    FEB_Console_Printf("        precharge(4), energized(5), drive(6), fault(7),\r\n");
    FEB_Console_Printf("        charging(8), battery_free(9), balance(10)\r\n");
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
    else if (strcasecmp_local(arg, "origin") == 0)
      new_state = BMS_STATE_ORIGIN;
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
    else if (strcasecmp_local(arg, "fault") == 0)
      new_state = BMS_STATE_FAULT;
    else if (strcasecmp_local(arg, "charging") == 0 || strcasecmp_local(arg, "charge") == 0)
      new_state = BMS_STATE_CHARGING;
    else if (strcasecmp_local(arg, "battery_free") == 0 || strcasecmp_local(arg, "free") == 0)
      new_state = BMS_STATE_BATTERY_FREE;
    else if (strcasecmp_local(arg, "balance") == 0 || strcasecmp_local(arg, "bal") == 0)
      new_state = BMS_STATE_BALANCE;
    else
    {
      FEB_Console_Printf("Unknown state: %s\r\n", arg);
      return;
    }
  }

  BMS_State_t old_state = FEB_CAN_State_GetState();
  if (FEB_CAN_State_SetState(new_state) == 0)
  {
    FEB_Console_Printf("State: %s -> %s\r\n", FEB_CAN_State_GetStateName(old_state),
                       FEB_CAN_State_GetStateName(new_state));
  }
  else
  {
    FEB_Console_Printf("Error: Failed to set state\r\n");
  }
}

static const FEB_Console_Cmd_t bms_cmd_state = {
    .name = "state",
    .help = "Show/set BMS state: state <name|0-10>",
    .handler = cmd_state,
};

/* ============================================================================
 * CAN Ping/Pong Commands
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

static void cmd_ping(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: ping|<channel>\r\n");
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

static const FEB_Console_Cmd_t bms_cmd_ping = {
    .name = "ping",
    .help = "Start CAN ping mode: ping|<1-4>",
    .handler = cmd_ping,
};

static void cmd_pong(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: pong|<channel>\r\n");
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

static const FEB_Console_Cmd_t bms_cmd_pong = {
    .name = "pong",
    .help = "Start CAN pong mode: pong|<1-4>",
    .handler = cmd_pong,
};

static void cmd_canstop(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: canstop|<channel|all>\r\n");
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

static const FEB_Console_Cmd_t bms_cmd_canstop = {
    .name = "canstop",
    .help = "Stop CAN ping/pong: canstop|<1-4|all>",
    .handler = cmd_canstop,
};

static void cmd_canstatus(int argc, char *argv[])
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

static const FEB_Console_Cmd_t bms_cmd_canstatus = {
    .name = "canstatus",
    .help = "Show CAN ping/pong status",
    .handler = cmd_canstatus,
};

/* ============================================================================
 * Registration
 * ============================================================================ */
void BMS_RegisterCommands(void)
{
  FEB_Console_Register(&bms_cmd_status);
  FEB_Console_Register(&bms_cmd_cells);
  FEB_Console_Register(&bms_cmd_temps);
  FEB_Console_Register(&bms_cmd_balance);
  FEB_Console_Register(&bms_cmd_dump);
  FEB_Console_Register(&bms_cmd_state);
  FEB_Console_Register(&bms_cmd_ping);
  FEB_Console_Register(&bms_cmd_pong);
  FEB_Console_Register(&bms_cmd_canstop);
  FEB_Console_Register(&bms_cmd_canstatus);
}
