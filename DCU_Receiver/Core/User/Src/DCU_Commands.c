/**
 ******************************************************************************
 * @file           : DCU_Commands.c
 * @brief          : Console commands for DCU_Receiver (radio-only)
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_Commands.h"
#include "FEB_RFM95.h"
#include "FEB_Task_Radio.h"
#include "feb_can_latest.h"
#include "feb_console.h"
#include "feb_string_utils.h"
#include "feb_log.h"
#include "rfm95.h"
#include "spi.h"
#include "main.h"
#include <stdlib.h>
#include <string.h>

#define TAG_DCU "[DCU]"

/* ============================================================================
 * Help
 * ============================================================================ */

static void print_dcu_help(void)
{
  FEB_Console_Printf("DCU_Receiver Commands:\r\n");
  FEB_Console_Printf("  dcu                    - Show this help\r\n");
  FEB_Console_Printf("  dcu|radio              - Radio commands (see dcu|radio for help)\r\n");
  FEB_Console_Printf("  dcu|can|state          - Show latest value of each received CAN message\r\n");
  FEB_Console_Printf("  dcu|can|msg|<name>     - Show signals for one CAN message\r\n");
}

/* ============================================================================
 * Radio Commands
 * ============================================================================ */

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
  FEB_Console_Printf("Radio Commands:\r\n");
  FEB_Console_Printf("  dcu|radio                       - Show this help\r\n");
  FEB_Console_Printf("  dcu|radio|status                - RSSI/SNR + GPIO/register state\r\n");
  FEB_Console_Printf("  dcu|radio|stats [reset]         - TX/RX counters\r\n");
  FEB_Console_Printf("  dcu|radio|tx <message>          - Transmit a string\r\n");
  FEB_Console_Printf("  dcu|radio|rx <timeout_ms>       - Receive once with timeout\r\n");
  FEB_Console_Printf("  dcu|radio|listen [on|off]       - Toggle/set listen-only mode\r\n");
  FEB_Console_Printf("  dcu|radio|config <p> <value>    - p in {freq,power,sf,bw}\r\n");
  FEB_Console_Printf("  dcu|radio|reset                 - Hardware reset of RFM95\r\n");
  FEB_Console_Printf("  dcu|radio|spi [sep|raw]         - Low-level SPI test\r\n");
  FEB_Console_Printf("  dcu|radio|en                    - Toggle EN pin test\r\n");
}

static void cmd_radio_status(void)
{
  init_debug_handle();

  FEB_RFM95_Stats_t stats;
  FEB_RFM95_GetStats(&stats);

  uint8_t op_mode = rfm95_read_register(&s_debug_handle, RFM95_REG_OP_MODE);
  uint8_t frf_msb = rfm95_read_register(&s_debug_handle, RFM95_REG_FRF_MSB);
  uint8_t frf_mid = rfm95_read_register(&s_debug_handle, RFM95_REG_FRF_MID);
  uint8_t frf_lsb = rfm95_read_register(&s_debug_handle, RFM95_REG_FRF_LSB);
  uint8_t mc1 = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_1);
  uint8_t mc2 = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_2);
  uint32_t frf = ((uint32_t)frf_msb << 16) | ((uint32_t)frf_mid << 8) | frf_lsb;
  uint32_t freq_hz = (uint32_t)(((uint64_t)frf * 32000000ULL) >> 19);

  FEB_Console_Printf("Radio Status:\r\n");
  FEB_Console_Printf("  Last RSSI:    %d dBm\r\n", (int)stats.last_rssi);
  FEB_Console_Printf("  Last SNR:     %d dB\r\n", (int)stats.last_snr);
  FEB_Console_Printf("  Listen mode:  %s\r\n", FEB_Task_Radio_GetListenMode() ? "ON" : "off");
  FEB_Console_Printf("  OpMode:       0x%02X\r\n", op_mode);
  FEB_Console_Printf("  Frequency:    %lu Hz\r\n", (unsigned long)freq_hz);
  FEB_Console_Printf("  ModemCfg1/2:  0x%02X 0x%02X\r\n", mc1, mc2);
  FEB_Console_Printf("\r\n");
  rfm95_debug_gpio_status(&s_debug_handle);
}

static void cmd_radio_stats(int argc, char *argv[])
{
  if (argc >= 4 && FEB_strcasecmp(argv[3], "reset") == 0)
  {
    FEB_RFM95_ResetStats();
    FEB_Console_Printf("Radio stats reset.\r\n");
    return;
  }

  FEB_RFM95_Stats_t stats;
  FEB_RFM95_GetStats(&stats);
  FEB_Console_Printf("Radio Stats:\r\n");
  FEB_Console_Printf("  TX count:     %lu\r\n", (unsigned long)stats.tx_count);
  FEB_Console_Printf("  TX errors:    %lu\r\n", (unsigned long)stats.tx_errors);
  FEB_Console_Printf("  RX count:     %lu\r\n", (unsigned long)stats.rx_count);
  FEB_Console_Printf("  RX errors:    %lu\r\n", (unsigned long)stats.rx_errors);
  FEB_Console_Printf("  RX timeouts:  %lu\r\n", (unsigned long)stats.rx_timeouts);
  FEB_Console_Printf("  Last RSSI:    %d dBm\r\n", (int)stats.last_rssi);
  FEB_Console_Printf("  Last SNR:     %d dB\r\n", (int)stats.last_snr);
}

static void cmd_radio_tx(int argc, char *argv[])
{
  if (argc < 4)
  {
    FEB_Console_Printf("Usage: dcu|radio|tx <message>\r\n");
    return;
  }

  char payload[128];
  size_t pos = 0;
  for (int i = 3; i < argc && pos < sizeof(payload) - 1; i++)
  {
    if (i > 3 && pos < sizeof(payload) - 1)
      payload[pos++] = ' ';
    size_t arg_len = strlen(argv[i]);
    size_t to_copy = (arg_len < sizeof(payload) - 1 - pos) ? arg_len : sizeof(payload) - 1 - pos;
    memcpy(&payload[pos], argv[i], to_copy);
    pos += to_copy;
  }
  payload[pos] = '\0';

  FEB_RFM95_Status_t s = FEB_RFM95_Transmit((const uint8_t *)payload, (uint8_t)pos, 1000);
  if (s == FEB_RFM95_OK)
    FEB_Console_Printf("TX OK: %u bytes\r\n", (unsigned int)pos);
  else
    FEB_Console_Printf("TX failed: status=%d\r\n", (int)s);
}

static void cmd_radio_rx(int argc, char *argv[])
{
  if (argc < 4)
  {
    FEB_Console_Printf("Usage: dcu|radio|rx <timeout_ms>\r\n");
    return;
  }
  uint32_t timeout = (uint32_t)strtoul(argv[3], NULL, 10);
  if (timeout == 0)
  {
    FEB_Console_Printf("Invalid timeout\r\n");
    return;
  }

  uint8_t buf[64];
  uint8_t len = 0;
  FEB_RFM95_Status_t s = FEB_RFM95_Receive(buf, &len, timeout);
  if (s == FEB_RFM95_OK)
  {
    FEB_Console_Printf("RX %u bytes  RSSI=%d  SNR=%d\r\n",
                       (unsigned int)len, (int)FEB_RFM95_GetRSSI(), (int)FEB_RFM95_GetSNR());
    FEB_Console_Printf("ASCII: \"");
    for (uint8_t i = 0; i < len; i++)
    {
      char c = (char)buf[i];
      FEB_Console_Printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
    }
    FEB_Console_Printf("\"\r\nHEX:   ");
    for (uint8_t i = 0; i < len; i++)
      FEB_Console_Printf("%02X ", buf[i]);
    FEB_Console_Printf("\r\n");
  }
  else if (s == FEB_RFM95_ERR_RX_TIMEOUT)
    FEB_Console_Printf("RX timeout\r\n");
  else
    FEB_Console_Printf("RX failed: status=%d\r\n", (int)s);
}

static void cmd_radio_listen(int argc, char *argv[])
{
  bool target;
  if (argc >= 4)
  {
    if (FEB_strcasecmp(argv[3], "on") == 0)
      target = true;
    else if (FEB_strcasecmp(argv[3], "off") == 0)
      target = false;
    else
    {
      FEB_Console_Printf("Usage: dcu|radio|listen [on|off]\r\n");
      return;
    }
  }
  else
  {
    target = !FEB_Task_Radio_GetListenMode();
  }
  FEB_Task_Radio_SetListenMode(target);
  FEB_Console_Printf("Listen mode: %s\r\n", target ? "ON" : "off");
}

static void cmd_radio_config(int argc, char *argv[])
{
  if (argc < 5)
  {
    FEB_Console_Printf("Usage: dcu|radio|config <freq|power|sf|bw> <value>\r\n");
    return;
  }
  init_debug_handle();
  const char *param = argv[3];
  long value = strtol(argv[4], NULL, 0);

  if (FEB_strcasecmp(param, "freq") == 0)
  {
    if (rfm95_set_frequency(&s_debug_handle, (uint32_t)value))
      FEB_Console_Printf("Frequency set to %ld Hz\r\n", value);
    else
      FEB_Console_Printf("Failed to set frequency\r\n");
  }
  else if (FEB_strcasecmp(param, "power") == 0)
  {
    if (rfm95_set_power(&s_debug_handle, (int8_t)value))
      FEB_Console_Printf("TX power set to %ld dBm\r\n", value);
    else
      FEB_Console_Printf("Failed to set power\r\n");
  }
  else if (FEB_strcasecmp(param, "sf") == 0)
  {
    if (value < 6 || value > 12)
    {
      FEB_Console_Printf("Spreading factor must be 6-12\r\n");
      return;
    }
    uint8_t cur = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_2);
    uint8_t new_val = (cur & 0x0F) | ((uint8_t)(value & 0x0F) << 4);
    rfm95_write_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_2, new_val);
    FEB_Console_Printf("SF set to %ld (MC2 0x%02X -> 0x%02X)\r\n", value, cur, new_val);
  }
  else if (FEB_strcasecmp(param, "bw") == 0)
  {
    if (value < 0 || value > 9)
    {
      FEB_Console_Printf("BW code must be 0..9 (see datasheet)\r\n");
      return;
    }
    uint8_t cur = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_1);
    uint8_t new_val = (cur & 0x0F) | ((uint8_t)(value & 0x0F) << 4);
    rfm95_write_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_1, new_val);
    FEB_Console_Printf("BW set to code %ld (MC1 0x%02X -> 0x%02X)\r\n", value, cur, new_val);
  }
  else
  {
    FEB_Console_Printf("Unknown parameter: %s\r\n", param);
  }
}

static void cmd_radio(int argc, char *argv[])
{
  if (argc < 3)
  {
    print_radio_help();
    return;
  }

  const char *subcmd = argv[2];

  if (FEB_strcasecmp(subcmd, "status") == 0)
    cmd_radio_status();
  else if (FEB_strcasecmp(subcmd, "stats") == 0)
    cmd_radio_stats(argc, argv);
  else if (FEB_strcasecmp(subcmd, "tx") == 0)
    cmd_radio_tx(argc, argv);
  else if (FEB_strcasecmp(subcmd, "rx") == 0)
    cmd_radio_rx(argc, argv);
  else if (FEB_strcasecmp(subcmd, "listen") == 0)
    cmd_radio_listen(argc, argv);
  else if (FEB_strcasecmp(subcmd, "config") == 0)
    cmd_radio_config(argc, argv);
  else if (FEB_strcasecmp(subcmd, "reset") == 0)
  {
    init_debug_handle();
    rfm95_debug_reset(&s_debug_handle);
  }
  else if (FEB_strcasecmp(subcmd, "spi") == 0)
  {
    init_debug_handle();
    if (argc >= 4 && FEB_strcasecmp(argv[3], "sep") == 0)
      rfm95_debug_spi_separate(&s_debug_handle);
    else if (argc >= 4 && FEB_strcasecmp(argv[3], "raw") == 0)
      rfm95_debug_spi_raw(&s_debug_handle);
    else
      rfm95_debug_spi_poll(&s_debug_handle);
  }
  else if (FEB_strcasecmp(subcmd, "en") == 0)
  {
    init_debug_handle();
    rfm95_debug_enable(&s_debug_handle);
  }
  else
  {
    FEB_Console_Printf("Unknown radio subcommand: %s\r\n", subcmd);
    print_radio_help();
  }
}

/* ============================================================================
 * CAN State Commands
 * ============================================================================ */

static void cmd_can(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: dcu|can|state  or  dcu|can|msg|<name>\r\n");
    return;
  }
  const char *sub = argv[2];
  if (FEB_strcasecmp(sub, "state") == 0)
  {
    FEB_CAN_State_Print(FEB_Console_Printf);
  }
  else if (FEB_strcasecmp(sub, "msg") == 0)
  {
    if (argc < 4)
    {
      FEB_Console_Printf("Usage: dcu|can|msg|<name>\r\n");
      return;
    }
    if (FEB_CAN_State_PrintOne(argv[3], FEB_Console_Printf) != 0)
    {
      FEB_Console_Printf("Unknown CAN message: %s\r\n", argv[3]);
    }
  }
  else
  {
    FEB_Console_Printf("Unknown can subcommand: %s\r\n", sub);
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

  if (FEB_strcasecmp(subcmd, "radio") == 0)
    cmd_radio(argc, argv);
  else if (FEB_strcasecmp(subcmd, "can") == 0)
    cmd_can(argc, argv);
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_dcu_help();
  }
}

static const FEB_Console_Cmd_t dcu_cmd = {
    .name = "dcu",
    .help = "DCU_Receiver board commands (dcu|radio, dcu|can)",
    .handler = cmd_dcu,
};

/* ============================================================================
 * Registration
 * ============================================================================ */

bool DCU_RegisterCommands(void)
{
  if (FEB_Console_Register(&dcu_cmd) != 0)
  {
    LOG_E(TAG_DCU, "Failed to register dcu command");
    return false;
  }
  return true;
}
