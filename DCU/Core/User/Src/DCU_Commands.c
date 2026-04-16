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
#include "feb_log.h"
#include "rfm95.h"
#include "spi.h"
#include "main.h"
#include <string.h>

#define TAG_DCU "[DCU]"

/* ============================================================================
 * Help Command
 * ============================================================================ */

static void print_dcu_help(void)
{
  FEB_Console_Printf("DCU Commands:\r\n");
  FEB_Console_Printf("  dcu              - Show this help\r\n");
  FEB_Console_Printf("  dcu|tps          - Show TPS power measurements\r\n");
  FEB_Console_Printf("  dcu|can          - Show CAN status and error counters\r\n");
  FEB_Console_Printf("  dcu|radio        - Radio debug commands (see dcu|radio for help)\r\n");
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
 * Radio Debug Command
 * ============================================================================ */

/* Static handle for debug commands - initialized on first use */
static rfm95_handle_t s_debug_handle;
static bool s_debug_handle_init = false;

static void init_debug_handle(void)
{
  if (!s_debug_handle_init)
  {
    s_debug_handle.spi_handle = &hspi3;
    s_debug_handle.nss_port = RD_CS_GPIO_Port;
    s_debug_handle.nss_pin = RD_CS_Pin;
    s_debug_handle.nrst_port = RD_RST_GPIO_Port;
    s_debug_handle.nrst_pin = RD_RST_Pin;
    s_debug_handle.en_port = RD_EN_GPIO_Port;
    s_debug_handle.en_pin = RD_EN_Pin;
    s_debug_handle_init = true;
  }
}

static void print_radio_help(void)
{
  FEB_Console_Printf("Radio Debug Commands:\r\n");
  FEB_Console_Printf("  dcu|radio           - Show this help\r\n");
  FEB_Console_Printf("  dcu|radio|status    - Show GPIO pin states\r\n");
  FEB_Console_Printf("  dcu|radio|spi       - Test SPI (full-duplex)\r\n");
  FEB_Console_Printf("  dcu|radio|spi|sep   - Test SPI (separate TX/RX)\r\n");
  FEB_Console_Printf("  dcu|radio|spi|raw   - Test SPI (direct register)\r\n");
  FEB_Console_Printf("  dcu|radio|reset     - Hardware reset sequence\r\n");
  FEB_Console_Printf("  dcu|radio|en        - Toggle EN pin test\r\n");
}

static void cmd_radio(int argc, char *argv[])
{
  init_debug_handle();

  if (argc < 3)
  {
    print_radio_help();
    return;
  }

  const char *subcmd = argv[2];

  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    rfm95_debug_gpio_status(&s_debug_handle);
  }
  else if (FEB_strcasecmp(subcmd, "spi") == 0)
  {
    if (argc >= 4 && FEB_strcasecmp(argv[3], "sep") == 0)
    {
      rfm95_debug_spi_separate(&s_debug_handle);
    }
    else if (argc >= 4 && FEB_strcasecmp(argv[3], "raw") == 0)
    {
      rfm95_debug_spi_raw(&s_debug_handle);
    }
    else
    {
      rfm95_debug_spi_poll(&s_debug_handle);
    }
  }
  else if (FEB_strcasecmp(subcmd, "reset") == 0)
  {
    rfm95_debug_reset(&s_debug_handle);
  }
  else if (FEB_strcasecmp(subcmd, "en") == 0)
  {
    rfm95_debug_enable(&s_debug_handle);
  }
  else
  {
    FEB_Console_Printf("Unknown radio subcommand: %s\r\n", subcmd);
    print_radio_help();
  }
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
    cmd_radio(argc, argv);
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

bool DCU_RegisterCommands(void)
{
  if (!FEB_Console_Register(&dcu_cmd))
  {
    LOG_E(TAG_DCU, "Failed to register dcu command");
    return false;
  }
  return true;
}
