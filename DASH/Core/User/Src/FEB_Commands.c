/**
 ******************************************************************************
 * @file           : FEB_Commands.c
 * @brief          : DASH-specific console commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Commands.h"
#include "feb_console.h"
#include "FEB_CAN_PingPong.h"
#include "feb_can_lib.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "FEB_CAN_LVPDB.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_PCU.h"

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

static const FEB_Console_Cmd_t dash_cmd_ping = {
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

static const FEB_Console_Cmd_t dash_cmd_pong = {
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

static const FEB_Console_Cmd_t dash_cmd_canstop = {
    .name = "canstop",
    .help = "Stop CAN ping/pong: canstop|<1-4|all>",
    .handler = cmd_canstop,
};

static void cmd_canstatus(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("CAN Ping/Pong Status:\r\n");
  FEB_Console_Printf("%-3s %-6s %-5s %8s %8s %8s %10s\r\n", "Ch", "FrameID", "Mode", "TX OK", "TX Fail", "RX",
                     "Last RX");
  FEB_Console_Printf("--- ------ ----- -------- -------- -------- ----------\r\n");

  for (int ch = 1; ch <= 4; ch++)
  {
    FEB_PingPong_Mode_t mode = FEB_CAN_PingPong_GetMode((uint8_t)ch);
    uint32_t tx_count = FEB_CAN_PingPong_GetTxCount((uint8_t)ch);
    uint32_t tx_fail = FEB_CAN_PingPong_GetTxFailCount((uint8_t)ch);
    uint32_t rx_count = FEB_CAN_PingPong_GetRxCount((uint8_t)ch);
    int32_t last_rx = FEB_CAN_PingPong_GetLastCounter((uint8_t)ch);

    FEB_Console_Printf("%-3d 0x%02X   %-5s %8u %8u %8u %10d\r\n", ch, (unsigned int)pingpong_frame_ids[ch - 1],
                       mode_names[mode], (unsigned int)tx_count, (unsigned int)tx_fail, (unsigned int)rx_count,
                       (int)last_rx);
  }

  /* Display CAN library error counters */
  FEB_Console_Printf("\r\nCAN Library Errors:\r\n");
  FEB_Console_Printf("  HAL Errors:        %lu\r\n", (unsigned long)FEB_CAN_GetHalErrorCount());
  FEB_Console_Printf("  TX Timeout:        %lu\r\n", (unsigned long)FEB_CAN_GetTxTimeoutCount());
  FEB_Console_Printf("  TX Queue Overflow: %lu\r\n", (unsigned long)FEB_CAN_GetTxQueueOverflowCount());
  FEB_Console_Printf("  RX Queue Overflow: %lu\r\n", (unsigned long)FEB_CAN_GetRxQueueOverflowCount());
}

static const FEB_Console_Cmd_t dash_cmd_canstatus = {
    .name = "canstatus",
    .help = "Show CAN ping/pong status",
    .handler = cmd_canstatus,
};

/* ============================================================================
 * LVPDB Commands for DASH Display
 * ============================================================================ */

static void cmd_lvpdb(int argc, char *argv[])
{
  (void)argc; // note to anyone reading this:
  (void)argv; // this is so that the compiler won't complain about unused parameters & this matches the FEB UART library

  uint16_t v24 = FEB_CAN_LVPDB_GetLast24VVoltage();
  uint16_t v12 = FEB_CAN_LVPDB_GetLast12VVoltage();

  FEB_Console_Printf("LVPDB Voltages:\r\n");
  FEB_Console_Printf("  24V Bus: %u (raw)\r\n", (unsigned int)v24);
  FEB_Console_Printf("  12V Bus: %u (raw)\r\n", (unsigned int)v12);
}

static const FEB_Console_Cmd_t dash_cmd_lvpdb = {
    .name = "lvpdb",
    .help = "Show LVPDB bus voltages",
    .handler = cmd_lvpdb,
};

/* ============================================================================
 * BMS Commands for DASH Display
 * ============================================================================ */

static void cmd_bms(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  BMS_State_t state = FEB_CAN_BMS_GetLastState();
  int16_t temp = FEB_CAN_BMS_GetLastCellMaxTemperature();
  uint16_t voltage = FEB_CAN_BMS_GetLastAccumulatorTotalVoltage();

  FEB_Console_Printf("BMS Status:\r\n");
  FEB_Console_Printf("  State:       %d (raw)\r\n", (int)state);
  FEB_Console_Printf("  Max Cell Temp: %d (raw)\r\n", (int)temp);
  FEB_Console_Printf("  Pack Voltage:  %u (raw)\r\n", (unsigned int)voltage);
}

static const FEB_Console_Cmd_t dash_cmd_bms = {
    .name = "bms",
    .help = "Show BMS state, max cell temperature, and pack voltage",
    .handler = cmd_bms,
};

/* ============================================================================
 * PCU Commands for DASH Display
 * ============================================================================ */
static void cmd_pcu(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  int16_t torque = FEB_CAN_PCU_GetLastTorque();
  int8_t direction = FEB_CAN_PCU_GetLastDirection();
  int8_t enabled = FEB_CAN_PCU_GetLastRMSEnabled();
  uint16_t brake = FEB_CAN_PCU_GetLastBreakPosition();

  FEB_Console_Printf("PCU / RMS Status:\r\n");
  FEB_Console_Printf("  Torque:         %d (raw)\r\n", (int)torque);
  FEB_Console_Printf("  Direction:      %d (raw)\r\n", (int)direction);
  FEB_Console_Printf("  RMS Enabled:    %d (raw)\r\n", (int)enabled);
  FEB_Console_Printf("  Brake Position: %u (raw)\r\n", (unsigned int)brake);
}

static const FEB_Console_Cmd_t dash_cmd_pcu = {
    .name = "pcu",
    .help = "Show PCU torque, direction, RMS enable, and brake position",
    .handler = cmd_pcu,
};

/* ============================================================================
 * Registration
 * ============================================================================ */
void DASH_RegisterCommands(void)
{
  // CAN Ping Pong commands
  FEB_Console_Register(&dash_cmd_ping);
  FEB_Console_Register(&dash_cmd_pong);
  FEB_Console_Register(&dash_cmd_canstop);
  FEB_Console_Register(&dash_cmd_canstatus);

  // LVPDB commands for DASH display
  FEB_Console_Register(&dash_cmd_lvpdb);
  // BMS commands for DASH display
  FEB_Console_Register(&dash_cmd_bms);
  // PCU commands for DASH display
  FEB_Console_Register(&dash_cmd_pcu);
}
