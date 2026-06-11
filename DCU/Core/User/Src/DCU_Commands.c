/**
 ******************************************************************************
 * @file           : DCU_Commands.c
 * @brief          : Console commands for DCU
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Table-driven command set (same pattern as the other boards). Each functional
 * group (tps / can / radio / sd) is one FEB_Console_Cmd_t registered top-level
 * with a text handler and a CSV handler, plus a text-only `dcu` parent that
 * dispatches `dcu|<group>|...`. Because each group is registered top-level, the
 * CSV protocol resolves `DCU|csv|<tx>|<group>|...` directly. Both the parent
 * dispatcher and the CSV dispatcher hand the group handler an argv slice whose
 * argv[0] is the group name and argv[1..] are the args.
 ******************************************************************************
 */

#include "DCU_Commands.h"
#include "DCU_TPS.h"
#include "DCU_CAN.h"
#include "DCU_CAN_Log.h"
#include "DCU_CAN_Filter.h"
#include "DCU_SD.h"
#include "FEB_RFM95.h"
#include "FEB_Task_Radio.h"
#include "feb_console.h"
#include "feb_can_lib.h"
#include "feb_string_utils.h"
#include "feb_log.h"
#include "rfm95.h"
#include "spi.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG_DCU "[DCU]"

/* ============================================================================
 * TPS Command
 * ============================================================================ */

static void sub_tps(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  DCU_TPS_Data_t data;
  DCU_TPS_GetData(&data);

  FEB_Console_Printf("TPS2482 Power Monitor:\r\n");
  if (data.valid)
  {
    FEB_Console_Printf("  Bus Voltage: %u mV (%.2f V)\r\n", data.bus_voltage_mv, data.bus_voltage_mv / 1000.0f);
    FEB_Console_Printf("  Current:     %d mA (%.3f A)\r\n", data.current_ma, data.current_ma / 1000.0f);
    FEB_Console_Printf("  Shunt:       %ld uV\r\n", (long)data.shunt_voltage_uv);

    float power_mw = (float)data.bus_voltage_mv * data.current_ma / 1000.0f;
    FEB_Console_Printf("  Power:       %.1f mW (%.3f W)\r\n", power_mw, power_mw / 1000.0f);
  }
  else
  {
    FEB_Console_Printf("  Status: NO DATA (check I2C connection)\r\n");
  }
}

static void cmd_tps_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  DCU_TPS_Data_t data;
  DCU_TPS_GetData(&data);
  float power_mw = data.valid ? (float)data.bus_voltage_mv * data.current_ma / 1000.0f : 0.0f;
  /* Body: bus_mv,current_ma,shunt_uv,power_mw,valid */
  FEB_Console_CsvEmit("tps", "%u,%d,%ld,%.1f,%d", data.bus_voltage_mv, data.current_ma, (long)data.shunt_voltage_uv,
                      power_mw, data.valid ? 1 : 0);
}

/* ============================================================================
 * CAN Command (status / log / stream)
 * ============================================================================ */

static void cmd_can_log(void)
{
  FEB_Console_Printf("CAN CSV Logger:\r\n");
  FEB_Console_Printf("  Active:         %s\r\n", DCU_CAN_Log_IsActive() ? "Yes" : "No");
  FEB_Console_Printf("  Filename:       %s\r\n", DCU_CAN_Log_GetFilename());
  FEB_Console_Printf("  Frames written: %lu\r\n", (unsigned long)DCU_CAN_Log_GetWrittenCount());
  FEB_Console_Printf("  Drops:          %lu\r\n", (unsigned long)DCU_CAN_Log_GetDropCount());
  FEB_Console_Printf("  Queue depth:    %lu\r\n", (unsigned long)DCU_CAN_Log_GetQueueDepth());
}

static void cmd_can_stream(int argc, char *argv[])
{
  /* argv = ["can", "stream", <on|off|status>]
   *   can|stream         -> status read-back (idempotent, matches CSV form)
   *   can|stream|on/off  -> toggle
   * Shares state with the CSV-form handler via DCU_CAN_Log_SetStream /
   * DCU_CAN_Log_IsStreaming. */
  const char *sub = (argc >= 3) ? argv[2] : NULL;

  if (sub != NULL && FEB_strcasecmp(sub, "on") == 0)
  {
    /* Pipe form has no transaction id — use a fixed marker so the website
     * (if it's sniffing the same UART) can still tell pipe-driven streams
     * apart from CSV-driven ones. */
    DCU_CAN_Log_SetStream(true, "pipe");
    FEB_Console_Printf("CAN stream: on\r\n");
    return;
  }
  if (sub != NULL && FEB_strcasecmp(sub, "off") == 0)
  {
    DCU_CAN_Log_SetStream(false, NULL);
    FEB_Console_Printf("CAN stream: off\r\n");
    return;
  }

  /* default / "status" */
  if (DCU_CAN_Log_IsStreaming())
  {
    FEB_Console_Printf("CAN stream: on (tx_id=%s)\r\n", DCU_CAN_Log_GetStreamTxId());
  }
  else
  {
    FEB_Console_Printf("CAN stream: off\r\n");
  }
}

static void sub_can(int argc, char *argv[])
{
  if (argc >= 2 && FEB_strcasecmp(argv[1], "log") == 0)
  {
    cmd_can_log();
    return;
  }
  if (argc >= 2 && FEB_strcasecmp(argv[1], "stream") == 0)
  {
    cmd_can_stream(argc, argv);
    return;
  }

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

static void cmd_can_csv(int argc, char *argv[])
{
  if (argc >= 2 && FEB_strcasecmp(argv[1], "log") == 0)
  {
    /* Body: active,filename,written,drops,queue_depth */
    FEB_Console_CsvEmit("can-log", "%d,%s,%lu,%lu,%lu", DCU_CAN_Log_IsActive() ? 1 : 0, DCU_CAN_Log_GetFilename(),
                        (unsigned long)DCU_CAN_Log_GetWrittenCount(), (unsigned long)DCU_CAN_Log_GetDropCount(),
                        (unsigned long)DCU_CAN_Log_GetQueueDepth());
    return;
  }
  if (argc >= 2 && FEB_strcasecmp(argv[1], "stream") == 0)
  {
    const char *sub = (argc >= 3) ? argv[2] : NULL;
    if (sub != NULL && FEB_strcasecmp(sub, "on") == 0)
    {
      /* Tag the stream with this transaction's tx_id so the host can correlate
       * the async frame rows that follow. */
      char tx[FEB_CSV_TX_ID_MAX_LEN + 1];
      DCU_CAN_Log_SetStream(true, FEB_Console_CsvCurrentTxId(tx, sizeof(tx)) ? tx : "csv");
      FEB_Console_CsvEmit("can-stream", "on");
      return;
    }
    if (sub != NULL && FEB_strcasecmp(sub, "off") == 0)
    {
      DCU_CAN_Log_SetStream(false, NULL);
      FEB_Console_CsvEmit("can-stream", "off");
      return;
    }
    if (DCU_CAN_Log_IsStreaming())
      FEB_Console_CsvEmit("can-stream", "on,%s", DCU_CAN_Log_GetStreamTxId());
    else
      FEB_Console_CsvEmit("can-stream", "off");
    return;
  }

  /* Body: initialized,tx_reg,rx_reg,hal_err,tx_timeout,txq_overflow,rxq_overflow */
  FEB_Console_CsvEmit("can", "%d,%lu,%lu,%lu,%lu,%lu,%lu", DCU_CAN_IsInitialized() ? 1 : 0,
                      (unsigned long)FEB_CAN_TX_GetRegisteredCount(), (unsigned long)FEB_CAN_RX_GetRegisteredCount(),
                      (unsigned long)FEB_CAN_GetHalErrorCount(), (unsigned long)FEB_CAN_GetTxTimeoutCount(),
                      (unsigned long)FEB_CAN_GetTxQueueOverflowCount(),
                      (unsigned long)FEB_CAN_GetRxQueueOverflowCount());
}

/* ============================================================================
 * Radio Commands
 * ============================================================================ */

/* Static handle for low-level debug commands - initialized on first use */
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
  FEB_Console_Printf("  dcu|radio|stream [on|off]       - Toggle CAN-over-radio forwarding\r\n");
  FEB_Console_Printf("  dcu|radio|config <p> <value>    - p in {freq,power,sf,bw}\r\n");
  FEB_Console_Printf("  dcu|radio|reset                 - Hardware reset of RFM95\r\n");
  FEB_Console_Printf("  dcu|radio|spi [sep|raw]         - Low-level SPI test (text only)\r\n");
  FEB_Console_Printf("  dcu|radio|en                    - Toggle EN pin test (text only)\r\n");
}

/* Read the radio's current frequency from FRF registers (Hz). */
static uint32_t radio_read_freq_hz(void)
{
  uint8_t frf_msb = rfm95_read_register(&s_debug_handle, RFM95_REG_FRF_MSB);
  uint8_t frf_mid = rfm95_read_register(&s_debug_handle, RFM95_REG_FRF_MID);
  uint8_t frf_lsb = rfm95_read_register(&s_debug_handle, RFM95_REG_FRF_LSB);
  uint32_t frf = ((uint32_t)frf_msb << 16) | ((uint32_t)frf_mid << 8) | frf_lsb;
  /* Frequency = FRF * F_XOSC / 2^19, F_XOSC = 32 MHz */
  return (uint32_t)(((uint64_t)frf * 32000000ULL) >> 19);
}

static void cmd_radio_status(void)
{
  init_debug_handle();

  FEB_RFM95_Stats_t stats;
  FEB_RFM95_GetStats(&stats);

  uint8_t op_mode = rfm95_read_register(&s_debug_handle, RFM95_REG_OP_MODE);
  uint8_t mc1 = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_1);
  uint8_t mc2 = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_2);
  uint32_t freq_hz = radio_read_freq_hz();

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
  if (argc >= 3 && FEB_strcasecmp(argv[2], "reset") == 0)
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

/* Join argv[start..argc-1] with single spaces into out (NUL-terminated). */
static void join_args(int start, int argc, char *argv[], char *out, size_t out_size)
{
  size_t pos = 0;
  for (int i = start; i < argc && pos + 1 < out_size; i++)
  {
    if (i > start && pos + 1 < out_size)
      out[pos++] = ' ';
    size_t alen = strlen(argv[i]);
    size_t cap = out_size - 1 - pos;
    size_t to_copy = (alen < cap) ? alen : cap;
    memcpy(&out[pos], argv[i], to_copy);
    pos += to_copy;
  }
  out[pos] = '\0';
}

static void cmd_radio_tx(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: dcu|radio|tx <message>\r\n");
    return;
  }

  char payload[128];
  join_args(2, argc, argv, payload, sizeof(payload));
  size_t pos = strlen(payload);

  FEB_RFM95_Status_t s = FEB_RFM95_Transmit((const uint8_t *)payload, (uint8_t)pos, 1000);
  if (s == FEB_RFM95_OK)
  {
    FEB_Console_Printf("TX OK: %u bytes\r\n", (unsigned int)pos);
  }
  else
  {
    FEB_Console_Printf("TX failed: status=%d\r\n", (int)s);
  }
}

static void cmd_radio_rx(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: dcu|radio|rx <timeout_ms>\r\n");
    return;
  }
  uint32_t timeout = (uint32_t)strtoul(argv[2], NULL, 10);
  if (timeout == 0)
  {
    FEB_Console_Printf("Invalid timeout\r\n");
    return;
  }

  uint8_t buf[255];
  uint8_t len = 0;
  FEB_RFM95_Status_t s = FEB_RFM95_Receive(buf, &len, timeout);
  if (s == FEB_RFM95_OK)
  {
    FEB_Console_Printf("RX %u bytes  RSSI=%d  SNR=%d\r\n", (unsigned int)len, (int)FEB_RFM95_GetRSSI(),
                       (int)FEB_RFM95_GetSNR());
    FEB_Console_Printf("ASCII: \"");
    for (uint8_t i = 0; i < len; i++)
    {
      char c = (char)buf[i];
      FEB_Console_Printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
    }
    FEB_Console_Printf("\"\r\nHEX:   ");
    for (uint8_t i = 0; i < len; i++)
    {
      FEB_Console_Printf("%02X ", buf[i]);
    }
    FEB_Console_Printf("\r\n");
  }
  else if (s == FEB_RFM95_ERR_RX_TIMEOUT)
  {
    FEB_Console_Printf("RX timeout\r\n");
  }
  else
  {
    FEB_Console_Printf("RX failed: status=%d\r\n", (int)s);
  }
}

static void cmd_radio_listen(int argc, char *argv[])
{
  bool target;
  if (argc >= 3)
  {
    if (FEB_strcasecmp(argv[2], "on") == 0)
      target = true;
    else if (FEB_strcasecmp(argv[2], "off") == 0)
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

/* dcu|radio|stream [on|off] — enable/disable CAN-over-radio forwarding. Which
 * CAN IDs are eligible is decided by DCU_CAN_Filter.c; this just gates the TX. */
static void cmd_radio_stream(int argc, char *argv[])
{
  const char *sub = (argc >= 3) ? argv[2] : NULL;

  if (sub != NULL && FEB_strcasecmp(sub, "on") == 0)
  {
    FEB_Task_Radio_SetStreamMode(true);
    FEB_Console_Printf("Radio stream: on\r\n");
    return;
  }
  if (sub != NULL && FEB_strcasecmp(sub, "off") == 0)
  {
    FEB_Task_Radio_SetStreamMode(false);
    FEB_Console_Printf("Radio stream: off\r\n");
    return;
  }

  /* default / "status" */
  FEB_Console_Printf("Radio stream: %s (fwd drops=%lu)\r\n", FEB_Task_Radio_GetStreamMode() ? "on" : "off",
                     (unsigned long)FEB_Task_Radio_GetForwardDropCount());
}

static void cmd_radio_config(int argc, char *argv[])
{
  if (argc < 4)
  {
    FEB_Console_Printf("Usage: dcu|radio|config <freq|power|sf|bw> <value>\r\n");
    return;
  }
  init_debug_handle();
  const char *param = argv[2];
  long value = strtol(argv[3], NULL, 0);

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
    /* MODEM_CONFIG_2 bits 7:4 are SF; preserve other bits. */
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

static void sub_radio(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_radio_help();
    return;
  }

  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    cmd_radio_status();
  }
  else if (FEB_strcasecmp(subcmd, "stats") == 0)
  {
    cmd_radio_stats(argc, argv);
  }
  else if (FEB_strcasecmp(subcmd, "tx") == 0)
  {
    cmd_radio_tx(argc, argv);
  }
  else if (FEB_strcasecmp(subcmd, "rx") == 0)
  {
    cmd_radio_rx(argc, argv);
  }
  else if (FEB_strcasecmp(subcmd, "listen") == 0)
  {
    cmd_radio_listen(argc, argv);
  }
  else if (FEB_strcasecmp(subcmd, "stream") == 0)
  {
    cmd_radio_stream(argc, argv);
  }
  else if (FEB_strcasecmp(subcmd, "config") == 0)
  {
    cmd_radio_config(argc, argv);
  }
  else if (FEB_strcasecmp(subcmd, "reset") == 0)
  {
    init_debug_handle();
    rfm95_debug_reset(&s_debug_handle);
  }
  else if (FEB_strcasecmp(subcmd, "spi") == 0)
  {
    init_debug_handle();
    if (argc >= 3 && FEB_strcasecmp(argv[2], "sep") == 0)
      rfm95_debug_spi_separate(&s_debug_handle);
    else if (argc >= 3 && FEB_strcasecmp(argv[2], "raw") == 0)
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

static void cmd_radio_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvError("info", "usage,radio|<status|stats|tx|rx|listen|config|reset>");
    return;
  }

  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    init_debug_handle();
    FEB_RFM95_Stats_t stats;
    FEB_RFM95_GetStats(&stats);
    uint8_t op_mode = rfm95_read_register(&s_debug_handle, RFM95_REG_OP_MODE);
    uint8_t mc1 = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_1);
    uint8_t mc2 = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_2);
    uint32_t freq_hz = radio_read_freq_hz();
    /* Body: last_rssi,last_snr,listen,op_mode,freq_hz,mc1,mc2 */
    FEB_Console_CsvEmit("radio-status", "%d,%d,%d,0x%02X,%lu,0x%02X,0x%02X", (int)stats.last_rssi, (int)stats.last_snr,
                        FEB_Task_Radio_GetListenMode() ? 1 : 0, op_mode, (unsigned long)freq_hz, mc1, mc2);
  }
  else if (FEB_strcasecmp(subcmd, "stats") == 0)
  {
    if (argc >= 3 && FEB_strcasecmp(argv[2], "reset") == 0)
    {
      FEB_RFM95_ResetStats();
      FEB_Console_CsvEmit("radio-stats", "reset");
      return;
    }
    FEB_RFM95_Stats_t stats;
    FEB_RFM95_GetStats(&stats);
    /* Body: tx_count,tx_errors,rx_count,rx_errors,rx_timeouts,last_rssi,last_snr */
    FEB_Console_CsvEmit("radio-stats", "%lu,%lu,%lu,%lu,%lu,%d,%d", (unsigned long)stats.tx_count,
                        (unsigned long)stats.tx_errors, (unsigned long)stats.rx_count, (unsigned long)stats.rx_errors,
                        (unsigned long)stats.rx_timeouts, (int)stats.last_rssi, (int)stats.last_snr);
  }
  else if (FEB_strcasecmp(subcmd, "tx") == 0)
  {
    if (argc < 3)
    {
      FEB_Console_CsvError("error", "usage,radio|tx|<message>");
      return;
    }
    char payload[128];
    join_args(2, argc, argv, payload, sizeof(payload));
    size_t pos = strlen(payload);
    FEB_RFM95_Status_t s = FEB_RFM95_Transmit((const uint8_t *)payload, (uint8_t)pos, 1000);
    /* Body: bytes,status (status 0 == OK) */
    FEB_Console_CsvEmit("radio-tx", "%u,%d", (unsigned int)pos, (int)s);
  }
  else if (FEB_strcasecmp(subcmd, "rx") == 0)
  {
    if (argc < 3)
    {
      FEB_Console_CsvError("error", "usage,radio|rx|<timeout_ms>");
      return;
    }
    uint32_t timeout = (uint32_t)strtoul(argv[2], NULL, 10);
    if (timeout == 0)
    {
      FEB_Console_CsvError("error", "invalid_timeout");
      return;
    }
    uint8_t buf[255];
    uint8_t len = 0;
    FEB_RFM95_Status_t s = FEB_RFM95_Receive(buf, &len, timeout);
    if (s == FEB_RFM95_OK)
    {
      char hex[2 * sizeof(buf) + 1];
      size_t hp = 0;
      for (uint8_t i = 0; i < len && hp + 2 < sizeof(hex); i++)
      {
        hp += (size_t)snprintf(hex + hp, sizeof(hex) - hp, "%02X", buf[i]);
      }
      hex[hp] = '\0';
      /* Body: len,rssi,snr,hex */
      FEB_Console_CsvEmit("radio-rx", "%u,%d,%d,%s", (unsigned int)len, (int)FEB_RFM95_GetRSSI(),
                          (int)FEB_RFM95_GetSNR(), hex);
    }
    else if (s == FEB_RFM95_ERR_RX_TIMEOUT)
    {
      FEB_Console_CsvError("warn", "rx_timeout");
    }
    else
    {
      FEB_Console_CsvError("error", "rx_failed,%d", (int)s);
    }
  }
  else if (FEB_strcasecmp(subcmd, "listen") == 0)
  {
    bool target;
    if (argc >= 3)
    {
      if (FEB_strcasecmp(argv[2], "on") == 0)
        target = true;
      else if (FEB_strcasecmp(argv[2], "off") == 0)
        target = false;
      else
      {
        FEB_Console_CsvError("error", "listen,%s", argv[2]);
        return;
      }
    }
    else
    {
      target = !FEB_Task_Radio_GetListenMode();
    }
    FEB_Task_Radio_SetListenMode(target);
    FEB_Console_CsvEmit("radio-listen", "%d", target ? 1 : 0);
  }
  else if (FEB_strcasecmp(subcmd, "stream") == 0)
  {
    bool target;
    if (argc >= 3)
    {
      if (FEB_strcasecmp(argv[2], "on") == 0)
        target = true;
      else if (FEB_strcasecmp(argv[2], "off") == 0)
        target = false;
      else
      {
        FEB_Console_CsvError("error", "stream,%s", argv[2]);
        return;
      }
    }
    else
    {
      target = !FEB_Task_Radio_GetStreamMode();
    }
    FEB_Task_Radio_SetStreamMode(target);
    /* Body: enabled,fwd_drops */
    FEB_Console_CsvEmit("radio-stream", "%d,%lu", target ? 1 : 0,
                        (unsigned long)FEB_Task_Radio_GetForwardDropCount());
  }
  else if (FEB_strcasecmp(subcmd, "config") == 0)
  {
    if (argc < 4)
    {
      FEB_Console_CsvError("error", "usage,radio|config|<freq|power|sf|bw>|<value>");
      return;
    }
    init_debug_handle();
    const char *param = argv[2];
    long value = strtol(argv[3], NULL, 0);
    bool ok = false;
    if (FEB_strcasecmp(param, "freq") == 0)
    {
      ok = rfm95_set_frequency(&s_debug_handle, (uint32_t)value);
    }
    else if (FEB_strcasecmp(param, "power") == 0)
    {
      ok = rfm95_set_power(&s_debug_handle, (int8_t)value);
    }
    else if (FEB_strcasecmp(param, "sf") == 0)
    {
      if (value < 6 || value > 12)
      {
        FEB_Console_CsvError("error", "sf_range,6-12");
        return;
      }
      uint8_t cur = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_2);
      uint8_t new_val = (cur & 0x0F) | ((uint8_t)(value & 0x0F) << 4);
      rfm95_write_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_2, new_val);
      ok = true;
    }
    else if (FEB_strcasecmp(param, "bw") == 0)
    {
      if (value < 0 || value > 9)
      {
        FEB_Console_CsvError("error", "bw_range,0-9");
        return;
      }
      uint8_t cur = rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_1);
      uint8_t new_val = (cur & 0x0F) | ((uint8_t)(value & 0x0F) << 4);
      rfm95_write_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_1, new_val);
      ok = true;
    }
    else
    {
      FEB_Console_CsvError("error", "param,%s", param);
      return;
    }
    /* Body: param,value,ok */
    FEB_Console_CsvEmit("radio-config", "%s,%ld,%d", param, value, ok ? 1 : 0);
  }
  else if (FEB_strcasecmp(subcmd, "reset") == 0)
  {
    init_debug_handle();
    rfm95_debug_reset(&s_debug_handle);
    FEB_Console_CsvEmit("radio-reset", "ok");
  }
  else if (FEB_strcasecmp(subcmd, "spi") == 0 || FEB_strcasecmp(subcmd, "en") == 0)
  {
    /* Low-level debug helpers print raw text; not representable as CSV rows. */
    FEB_Console_CsvError("info", "text_only,%s", subcmd);
  }
  else
  {
    FEB_Console_CsvError("error", "radio_subcommand,%s", subcmd);
  }
}

/* ============================================================================
 * SD Commands — all routed through sdTask via DCU_SD_* wrappers
 * ============================================================================ */

#define SD_CMD_TIMEOUT_MS 10000U

static const char *sd_fs_type_str(uint8_t fs_type)
{
  switch (fs_type)
  {
  case FS_FAT12:
    return "FAT12";
  case FS_FAT16:
    return "FAT16";
  case FS_FAT32:
    return "FAT32";
  case FS_EXFAT:
    return "EXFAT";
  default:
    return "?";
  }
}

static void print_sd_help(void)
{
  FEB_Console_Printf("SD Commands (all routed through sdTask):\r\n");
  FEB_Console_Printf("  dcu|sd                          - Show this help\r\n");
  FEB_Console_Printf("  dcu|sd|smoke                    - Mount/write/read smoke test\r\n");
  FEB_Console_Printf("  dcu|sd|mount                    - Mount the card\r\n");
  FEB_Console_Printf("  dcu|sd|unmount                  - Unmount the card\r\n");
  FEB_Console_Printf("  dcu|sd|info                     - Capacity / free space / FS type\r\n");
  FEB_Console_Printf("  dcu|sd|ls [path]                - List directory entries\r\n");
  FEB_Console_Printf("  dcu|sd|write <file> <text>      - Write text to file (truncates)\r\n");
  FEB_Console_Printf("  dcu|sd|append <file> <text>     - Append text to file\r\n");
  FEB_Console_Printf("  dcu|sd|read <file>              - Print file contents\r\n");
  FEB_Console_Printf("  dcu|sd|rm <file>                - Delete a file\r\n");
  FEB_Console_Printf("  dcu|sd|bench                    - 64 KB write+read throughput\r\n");
}

static void cmd_sd_mount(void)
{
  FRESULT r = DCU_SD_Mount(SD_CMD_TIMEOUT_MS);
  FEB_Console_Printf("Mount: %s (%d)\r\n", DCU_SD_FresultString(r), (int)r);
}

static void cmd_sd_unmount(void)
{
  FRESULT r = DCU_SD_Unmount(SD_CMD_TIMEOUT_MS);
  FEB_Console_Printf("Unmount: %s (%d)\r\n", DCU_SD_FresultString(r), (int)r);
}

static void cmd_sd_info(void)
{
  uint32_t total_kb = 0, free_kb = 0;
  uint8_t fs_type = 0;
  FRESULT r = DCU_SD_GetInfo(&total_kb, &free_kb, &fs_type, SD_CMD_TIMEOUT_MS);
  if (r != FR_OK)
  {
    FEB_Console_Printf("info: %s (%d)\r\n", DCU_SD_FresultString(r), (int)r);
    return;
  }
  FEB_Console_Printf("Filesystem: %s\r\n", sd_fs_type_str(fs_type));
  FEB_Console_Printf("Total: %lu KB\r\n", (unsigned long)total_kb);
  FEB_Console_Printf("Free:  %lu KB\r\n", (unsigned long)free_kb);
}

static void cmd_sd_ls(int argc, char *argv[])
{
  const char *path = (argc >= 3) ? argv[2] : "/";
  FRESULT r = DCU_SD_Ls(path, SD_CMD_TIMEOUT_MS);
  if (r != FR_OK)
    FEB_Console_Printf("ls(%s): %s (%d)\r\n", path, DCU_SD_FresultString(r), (int)r);
}

static void cmd_sd_write(int argc, char *argv[])
{
  if (argc < 4)
  {
    FEB_Console_Printf("Usage: dcu|sd|write <file> <text>\r\n");
    return;
  }
  char buf[256];
  join_args(3, argc, argv, buf, sizeof(buf));
  size_t len = strlen(buf);
  FRESULT r = DCU_SD_Write(argv[2], (const uint8_t *)buf, (uint32_t)len, SD_CMD_TIMEOUT_MS);
  if (r == FR_OK)
    FEB_Console_Printf("Wrote %u bytes to %s\r\n", (unsigned)len, argv[2]);
  else
    FEB_Console_Printf("write(%s): %s (%d)\r\n", argv[2], DCU_SD_FresultString(r), (int)r);
}

static void cmd_sd_append(int argc, char *argv[])
{
  if (argc < 4)
  {
    FEB_Console_Printf("Usage: dcu|sd|append <file> <text>\r\n");
    return;
  }
  char buf[256];
  join_args(3, argc, argv, buf, sizeof(buf));
  size_t len = strlen(buf);
  FRESULT r = DCU_SD_Append(argv[2], (const uint8_t *)buf, (uint32_t)len, SD_CMD_TIMEOUT_MS);
  if (r == FR_OK)
    FEB_Console_Printf("Appended %u bytes to %s\r\n", (unsigned)len, argv[2]);
  else
    FEB_Console_Printf("append(%s): %s (%d)\r\n", argv[2], DCU_SD_FresultString(r), (int)r);
}

static void cmd_sd_read(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: dcu|sd|read <file>\r\n");
    return;
  }
  uint8_t buf[256];
  uint32_t got = 0;
  FRESULT r = DCU_SD_Read(argv[2], buf, sizeof(buf) - 1U, &got, SD_CMD_TIMEOUT_MS);
  if (r != FR_OK)
  {
    FEB_Console_Printf("read(%s): %s (%d)\r\n", argv[2], DCU_SD_FresultString(r), (int)r);
    return;
  }
  buf[got] = '\0';
  FEB_Console_Printf("%s\r\n[%lu bytes%s]\r\n", (char *)buf, (unsigned long)got,
                     got == sizeof(buf) - 1U ? ", truncated" : "");
}

static void cmd_sd_rm(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: dcu|sd|rm <file>\r\n");
    return;
  }
  FRESULT r = DCU_SD_Delete(argv[2], SD_CMD_TIMEOUT_MS);
  FEB_Console_Printf("rm(%s): %s (%d)\r\n", argv[2], DCU_SD_FresultString(r), (int)r);
}

static void sub_sd(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_sd_help();
    return;
  }
  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "smoke") == 0)
    DCU_SD_RunSmokeTest();
  else if (FEB_strcasecmp(subcmd, "mount") == 0)
    cmd_sd_mount();
  else if (FEB_strcasecmp(subcmd, "unmount") == 0)
    cmd_sd_unmount();
  else if (FEB_strcasecmp(subcmd, "info") == 0)
    cmd_sd_info();
  else if (FEB_strcasecmp(subcmd, "ls") == 0)
    cmd_sd_ls(argc, argv);
  else if (FEB_strcasecmp(subcmd, "write") == 0)
    cmd_sd_write(argc, argv);
  else if (FEB_strcasecmp(subcmd, "append") == 0)
    cmd_sd_append(argc, argv);
  else if (FEB_strcasecmp(subcmd, "read") == 0)
    cmd_sd_read(argc, argv);
  else if (FEB_strcasecmp(subcmd, "rm") == 0)
    cmd_sd_rm(argc, argv);
  else if (FEB_strcasecmp(subcmd, "bench") == 0)
    DCU_SD_RunBenchmark();
  else
  {
    FEB_Console_Printf("Unknown sd subcommand: %s\r\n", subcmd);
    print_sd_help();
  }
}

static void cmd_sd_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvError("info", "usage,sd|<mount|unmount|info|write|append|read|rm>");
    return;
  }
  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "mount") == 0)
  {
    FRESULT r = DCU_SD_Mount(SD_CMD_TIMEOUT_MS);
    FEB_Console_CsvEmit("sd-mount", "%s,%d", DCU_SD_FresultString(r), (int)r);
  }
  else if (FEB_strcasecmp(subcmd, "unmount") == 0)
  {
    FRESULT r = DCU_SD_Unmount(SD_CMD_TIMEOUT_MS);
    FEB_Console_CsvEmit("sd-unmount", "%s,%d", DCU_SD_FresultString(r), (int)r);
  }
  else if (FEB_strcasecmp(subcmd, "info") == 0)
  {
    uint32_t total_kb = 0, free_kb = 0;
    uint8_t fs_type = 0;
    FRESULT r = DCU_SD_GetInfo(&total_kb, &free_kb, &fs_type, SD_CMD_TIMEOUT_MS);
    if (r != FR_OK)
    {
      FEB_Console_CsvError("error", "info,%s,%d", DCU_SD_FresultString(r), (int)r);
      return;
    }
    /* Body: fs_type,total_kb,free_kb */
    FEB_Console_CsvEmit("sd-info", "%s,%lu,%lu", sd_fs_type_str(fs_type), (unsigned long)total_kb,
                        (unsigned long)free_kb);
  }
  else if (FEB_strcasecmp(subcmd, "write") == 0 || FEB_strcasecmp(subcmd, "append") == 0)
  {
    if (argc < 4)
    {
      FEB_Console_CsvError("error", "usage,sd|%s|<file>|<text>", subcmd);
      return;
    }
    const bool append = FEB_strcasecmp(subcmd, "append") == 0;
    char buf[256];
    join_args(3, argc, argv, buf, sizeof(buf));
    size_t len = strlen(buf);
    FRESULT r = append ? DCU_SD_Append(argv[2], (const uint8_t *)buf, (uint32_t)len, SD_CMD_TIMEOUT_MS)
                       : DCU_SD_Write(argv[2], (const uint8_t *)buf, (uint32_t)len, SD_CMD_TIMEOUT_MS);
    if (r == FR_OK)
      FEB_Console_CsvEmit(append ? "sd-append" : "sd-write", "%s,%u", argv[2], (unsigned)len);
    else
      FEB_Console_CsvError("error", "%s,%s,%s,%d", subcmd, argv[2], DCU_SD_FresultString(r), (int)r);
  }
  else if (FEB_strcasecmp(subcmd, "read") == 0)
  {
    if (argc < 3)
    {
      FEB_Console_CsvError("error", "usage,sd|read|<file>");
      return;
    }
    uint8_t buf[256];
    uint32_t got = 0;
    FRESULT r = DCU_SD_Read(argv[2], buf, sizeof(buf) - 1U, &got, SD_CMD_TIMEOUT_MS);
    if (r != FR_OK)
    {
      FEB_Console_CsvError("error", "read,%s,%s,%d", argv[2], DCU_SD_FresultString(r), (int)r);
      return;
    }
    /* Body: file,bytes (file content stays in text mode — it may contain commas
     * or newlines that would break CSV framing). */
    FEB_Console_CsvEmit("sd-read", "%s,%lu", argv[2], (unsigned long)got);
  }
  else if (FEB_strcasecmp(subcmd, "rm") == 0)
  {
    if (argc < 3)
    {
      FEB_Console_CsvError("error", "usage,sd|rm|<file>");
      return;
    }
    FRESULT r = DCU_SD_Delete(argv[2], SD_CMD_TIMEOUT_MS);
    if (r == FR_OK)
      FEB_Console_CsvEmit("sd-rm", "%s,ok", argv[2]);
    else
      FEB_Console_CsvError("error", "rm,%s,%s,%d", argv[2], DCU_SD_FresultString(r), (int)r);
  }
  else if (FEB_strcasecmp(subcmd, "ls") == 0 || FEB_strcasecmp(subcmd, "smoke") == 0 ||
           FEB_strcasecmp(subcmd, "bench") == 0)
  {
    /* These print free-form text directly; not representable as CSV rows. */
    FEB_Console_CsvError("info", "text_only,%s", subcmd);
  }
  else
  {
    FEB_Console_CsvError("error", "sd_subcommand,%s", subcmd);
  }
}

/* ============================================================================
 * Help + parent dispatcher + registration
 * ============================================================================ */

static void print_dcu_help(void)
{
  FEB_Console_Printf("DCU Commands:\r\n");
  FEB_Console_Printf("  dcu              - Show this help\r\n");
  FEB_Console_Printf("  dcu|tps          - Show TPS power measurements\r\n");
  FEB_Console_Printf("  dcu|can          - CAN status (dcu|can|log, dcu|can|stream|[on|off])\r\n");
  FEB_Console_Printf("  dcu|radio        - Radio commands (see dcu|radio for help)\r\n");
  FEB_Console_Printf("  dcu|sd           - SD card commands (see dcu|sd for help)\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("CSV Protocol (machine-readable):\r\n");
  FEB_Console_Printf("  DCU|csv|<tx_id>|<sub>  - any subcommand above also works as CSV\r\n");
  FEB_Console_Printf("  *|csv|<tx_id>|hello    - Discover all boards (system command)\r\n");
  FEB_Console_Printf("Each request emits: ack -> [rows] -> done\r\n");
}

/* One descriptor per functional group: text handler + CSV handler. Each is
 * registered top-level (hidden) so CSV resolves `DCU|csv|<tx>|<group>`; the
 * `dcu` parent exposes them as `dcu|<group>` for humans. */
static const FEB_Console_Cmd_t dcu_tps_cmd = {
    .name = "tps", .help = "TPS power measurements", .handler = sub_tps, .csv_handler = cmd_tps_csv, .hidden = true};
static const FEB_Console_Cmd_t dcu_can_cmd = {.name = "can",
                                              .help = "CAN status / logger / stream (can|log, can|stream|[on|off])",
                                              .handler = sub_can,
                                              .csv_handler = cmd_can_csv,
                                              .hidden = true};
static const FEB_Console_Cmd_t dcu_radio_cmd = {.name = "radio",
                                                .help = "Radio commands (status, stats, tx, rx, listen, config, reset)",
                                                .handler = sub_radio,
                                                .csv_handler = cmd_radio_csv,
                                                .hidden = true};
static const FEB_Console_Cmd_t dcu_sd_cmd = {.name = "sd",
                                             .help = "SD card commands (mount, info, ls, write, read, rm, ...)",
                                             .handler = sub_sd,
                                             .csv_handler = cmd_sd_csv,
                                             .hidden = true};

static const FEB_Console_Cmd_t *const DCU_SUBCMDS[] = {
    &dcu_tps_cmd,
    &dcu_can_cmd,
    &dcu_radio_cmd,
    &dcu_sd_cmd,
};
#define DCU_SUBCMDS_COUNT (sizeof(DCU_SUBCMDS) / sizeof(DCU_SUBCMDS[0]))

/* Text-mode parent: `dcu|<group>|<args>` dispatches via the table. Bare
 * `<group>` (no dcu prefix) also works because each group is registered
 * top-level. */
static void cmd_dcu(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_dcu_help();
    return;
  }
  const char *subcmd = argv[1];
  for (size_t i = 0; i < DCU_SUBCMDS_COUNT; i++)
  {
    if (FEB_strcasecmp(DCU_SUBCMDS[i]->name, subcmd) == 0)
    {
      if (DCU_SUBCMDS[i]->handler != NULL)
        DCU_SUBCMDS[i]->handler(argc - 1, argv + 1);
      return;
    }
  }
  FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
  print_dcu_help();
}

static const FEB_Console_Cmd_t dcu_cmd = {
    .name = "dcu",
    .help = "DCU board commands (dcu|tps, dcu|can, dcu|radio, dcu|sd)",
    .handler = cmd_dcu,
    .csv_handler = NULL,
};

bool DCU_RegisterCommands(void)
{
  int rc = FEB_Console_Register(&dcu_cmd);
  if (rc != 0)
  {
    LOG_E(TAG_DCU, "Failed to register dcu command (rc=%d)", rc);
    return false;
  }
  for (size_t i = 0; i < DCU_SUBCMDS_COUNT; i++)
  {
    rc = FEB_Console_Register(DCU_SUBCMDS[i]);
    if (rc != 0)
    {
      LOG_E(TAG_DCU, "Failed to register '%s' (rc=%d)", DCU_SUBCMDS[i]->name, rc);
      return false;
    }
  }
  return true;
}
