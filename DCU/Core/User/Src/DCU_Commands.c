/**
 ******************************************************************************
 * @file           : DCU_Commands.c
 * @brief          : Console commands for DCU
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_Commands.h"
#include "DCU_TPS.h"
#include "DCU_CAN.h"
#include "feb_console.h"
#include "feb_can_lib.h"
#include "feb_string_utils.h"
#include <string.h>

/* ============================================================================
 * Help Command
 * ============================================================================ */

static void print_dcu_help(void)
{
  FEB_Console_Printf("DCU Commands:\r\n");
  FEB_Console_Printf("  dcu              - Show this help\r\n");
  FEB_Console_Printf("  dcu|tps          - Show TPS power measurements\r\n");
  FEB_Console_Printf("  dcu|can          - Show CAN status and error counters\r\n");
  FEB_Console_Printf("  dcu|radio        - Show radio status\r\n");
}

/* ============================================================================
 * TPS Command
 * ============================================================================ */

static void cmd_tps(void)
{
  DCU_TPS_Data_t data;
  DCU_TPS_GetData(&data);

  FEB_Console_Printf("TPS2482 Power Monitor:\r\n");
  if (data.valid)
  {
    FEB_Console_Printf("  Bus Voltage: %u mV (%.2f V)\r\n", data.bus_voltage_mv, data.bus_voltage_mv / 1000.0f);
    FEB_Console_Printf("  Current:     %d mA (%.3f A)\r\n", data.current_ma, data.current_ma / 1000.0f);
    FEB_Console_Printf("  Shunt:       %ld uV\r\n", (long)data.shunt_voltage_uv);

    /* Calculate power */
    float power_mw = (float)data.bus_voltage_mv * data.current_ma / 1000.0f;
    FEB_Console_Printf("  Power:       %.1f mW (%.3f W)\r\n", power_mw, power_mw / 1000.0f);
  }
  else
  {
    FEB_Console_Printf("  Status: NO DATA (check I2C connection)\r\n");
  }
}

/* ============================================================================
 * CAN Status Command
 * ============================================================================ */

static void cmd_can(void)
{
  FEB_Console_Printf("CAN Status:\r\n");
  FEB_Console_Printf("  Initialized:   %s\r\n", DCU_CAN_IsInitialized() ? "Yes" : "No");
  FEB_Console_Printf("  TX Registered: %lu\r\n", (unsigned long)FEB_CAN_TX_GetRegisteredCount());
  FEB_Console_Printf("  RX Registered: %lu\r\n", (unsigned long)FEB_CAN_RX_GetRegisteredCount());
  FEB_Console_Printf("\r\nError Counters:\r\n");
  FEB_Console_Printf("  HAL Errors:        %lu\r\n", (unsigned long)FEB_CAN_GetHalErrorCount());
  FEB_Console_Printf("  TX Timeout:        %lu\r\n", (unsigned long)FEB_CAN_GetTxTimeoutCount());
  FEB_Console_Printf("  TX Queue Overflow: %lu\r\n", (unsigned long)FEB_CAN_GetTxQueueOverflowCount());
  FEB_Console_Printf("  RX Queue Overflow: %lu\r\n", (unsigned long)FEB_CAN_GetRxQueueOverflowCount());
}

/* ============================================================================
 * Radio Status Command
 * ============================================================================ */

static void cmd_radio(void)
{
  FEB_Console_Printf("Radio Status:\r\n");
  FEB_Console_Printf("  See radioTask for RFM95 ping-pong status\r\n");
  /* TODO: Add accessor functions to FEB_Task_Radio.c for live status */
}

/* ============================================================================
 * Main DCU Command Handler
 * ============================================================================ */

static void cmd_dcu(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_dcu_help();
    return;
  }

  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "tps") == 0)
  {
    cmd_tps();
  }
  else if (FEB_strcasecmp(subcmd, "can") == 0)
  {
    cmd_can();
  }
  else if (FEB_strcasecmp(subcmd, "radio") == 0)
  {
    cmd_radio();
  }
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_dcu_help();
  }
}

static const FEB_Console_Cmd_t dcu_cmd = {
    .name = "dcu",
    .help = "DCU board commands (dcu|tps, dcu|can, dcu|radio)",
    .handler = cmd_dcu,
};

/* ============================================================================
 * Registration
 * ============================================================================ */

void DCU_RegisterCommands(void)
{
  FEB_Console_Register(&dcu_cmd);
}
