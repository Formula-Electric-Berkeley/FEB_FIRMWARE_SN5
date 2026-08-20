/**
 ******************************************************************************
 * @file           : DCU_Commands.cpp
 * @brief          : Console commands for DCU_Receiver (radio + CAN-state)
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_Commands.h"
#include "FEB_CAN_Stream.h"
#include "FEB_RFM95.h"
#include "FEB_Task_Radio.h"
#include "feb_can_latest.h"
#include "feb_commands_2.hpp"
#include "feb_console_2.hpp"
#include "main.h"
#include "rfm95.h"
#include "spi.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace feb::console;

namespace
{

/* ============================================================================
 * Radio
 * ============================================================================ */

rfm95_handle_t s_debug_handle;
bool s_debug_handle_init = false;

void init_debug_handle()
{
  if (s_debug_handle_init)
  {
    return;
  }
  s_debug_handle.spi_handle = &hspi3;
  s_debug_handle.nss_port = RD_CS_GPIO_Port;
  s_debug_handle.nss_pin = RD_CS_Pin;
  s_debug_handle.nrst_port = RD_RST_GPIO_Port;
  s_debug_handle.nrst_pin = RD_RST_Pin;
  s_debug_handle.en_port = RD_EN_GPIO_Port;
  s_debug_handle.en_pin = RD_EN_Pin;
  s_debug_handle_init = true;
}

/* Frequency = FRF * F_XOSC / 2^19, F_XOSC = 32 MHz */
std::uint32_t radio_read_freq_hz()
{
  const std::uint32_t frf =
      (static_cast<std::uint32_t>(rfm95_read_register(&s_debug_handle, RFM95_REG_FRF_MSB)) << 16) |
      (static_cast<std::uint32_t>(rfm95_read_register(&s_debug_handle, RFM95_REG_FRF_MID)) << 8) |
      rfm95_read_register(&s_debug_handle, RFM95_REG_FRF_LSB);
  return static_cast<std::uint32_t>((static_cast<std::uint64_t>(frf) * 32000000ULL) >> 19);
}

void join_args(std::span<char *const> args, std::size_t start, char *out, std::size_t cap)
{
  std::size_t pos = 0;
  for (std::size_t i = start; i < args.size() && pos + 1 < cap; i++)
  {
    if (i > start)
    {
      out[pos++] = ' ';
    }
    const std::size_t len = std::strlen(args[i]);
    const std::size_t take = (len < cap - 1 - pos) ? len : cap - 1 - pos;
    std::memcpy(out + pos, args[i], take);
    pos += take;
  }
  out[pos] = '\0';
}

void cmd_dcu_radio_status(Interaction &io, std::span<char *const>)
{
  init_debug_handle();

  FEB_RFM95_Stats_t stats;
  FEB_RFM95_GetStats(&stats);

  {
    KVTable t(io, 12, 14, "Radio Status");

    t.row("Last RSSI", "%d dBm", (int)stats.last_rssi);
    t.row("Last SNR", "%d dB", (int)stats.last_snr);
    t.row("Listen mode", "%s", FEB_Task_Radio_GetListenMode() ? "ON" : "off");
    t.row("OpMode", "0x%02X", rfm95_read_register(&s_debug_handle, RFM95_REG_OP_MODE));
    t.row("Frequency", "%lu Hz", (unsigned long)radio_read_freq_hz());
    t.row("ModemCfg1/2", "0x%02X 0x%02X", rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_1),
          rfm95_read_register(&s_debug_handle, RFM95_REG_MODEM_CONFIG_2));
  }

  rfm95_debug_gpio_status(&s_debug_handle); /* reports through FEB_Log, not this console */
}

void cmd_dcu_radio_stats(Interaction &io, std::span<char *const>)
{
  FEB_RFM95_Stats_t s;
  FEB_RFM95_GetStats(&s);

  KVTable t(io, 12, 14, "Radio Stats");

  t.row("TX count", "%lu", (unsigned long)s.tx_count);
  t.row("TX errors", "%lu", (unsigned long)s.tx_errors);
  t.row("RX count", "%lu", (unsigned long)s.rx_count);
  t.row("RX errors", "%lu", (unsigned long)s.rx_errors);
  t.row("RX timeouts", "%lu", (unsigned long)s.rx_timeouts);
  t.row("Last RSSI", "%d dBm", (int)s.last_rssi);
  t.row("Last SNR", "%d dB", (int)s.last_snr);
}

constexpr std::array kDcuRadioTxParams = {param_str("message")};

void cmd_dcu_radio_tx(Interaction &io, std::span<char *const> args)
{
  char payload[128];
  join_args(args, 1, payload, sizeof(payload));
  const std::size_t len = std::strlen(payload);
  const FEB_RFM95_Status_t s = FEB_RFM95_Transmit((const std::uint8_t *)payload, (std::uint8_t)len, 1000);

  if (s == FEB_RFM95_OK)
  {
    io.print("TX OK: %u bytes\r\n", (unsigned)len);
  }
  else
  {
    io.error("error", "tx_failed", "%d", (int)s);
  }
}

constexpr std::array kDcuRadioRxParams = {param_int("timeout_ms", 1, 60000)};

void cmd_dcu_radio_rx(Interaction &io, std::span<char *const>)
{
  const long timeout = io.param_int(0);

  std::uint8_t buf[255];
  std::uint8_t len = 0;
  const FEB_RFM95_Status_t s = FEB_RFM95_Receive(buf, &len, (std::uint32_t)timeout);

  if (s == FEB_RFM95_ERR_RX_TIMEOUT)
  {
    io.error("warn", "rx_timeout");
    return;
  }
  if (s != FEB_RFM95_OK)
  {
    io.error("error", "rx_failed", "%d", (int)s);
    return;
  }

  io.print("RX %u bytes  RSSI=%d  SNR=%d\r\n", (unsigned)len, (int)FEB_RFM95_GetRSSI(), (int)FEB_RFM95_GetSNR());
  io.print("ASCII: \"");
  for (std::uint8_t i = 0; i < len; i++)
  {
    const char c = (char)buf[i];
    io.print("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
  }
  io.print("\"\r\nHEX:   ");
  for (std::uint8_t i = 0; i < len; i++)
  {
    io.print("%02X ", buf[i]);
  }
  io.print("\r\n");
}

void cmd_dcu_radio_listen(Interaction &io, std::span<char *const>)
{
  io.print("Listen mode: %s\r\n", FEB_Task_Radio_GetListenMode() ? "ON" : "off");
}

void cmd_dcu_radio_listen_on(Interaction &io, std::span<char *const>)
{
  FEB_Task_Radio_SetListenMode(true);
  io.print("Listen mode: ON\r\n");
}

void cmd_dcu_radio_listen_off(Interaction &io, std::span<char *const>)
{
  FEB_Task_Radio_SetListenMode(false);
  io.print("Listen mode: off\r\n");
}

/* Rewrite the top nibble of a modem-config register, reporting before/after. */
void set_modem_nibble(Interaction &io, const char *label, std::uint8_t reg, long value)
{
  const std::uint8_t cur = rfm95_read_register(&s_debug_handle, reg);
  const std::uint8_t next = (std::uint8_t)((cur & 0x0F) | ((std::uint8_t)(value & 0x0F) << 4));
  rfm95_write_register(&s_debug_handle, reg, next);
  io.print("%s set to %ld (0x%02X -> 0x%02X)\r\n", label, value, cur, next);
}

constexpr std::array kDcuRadioConfigFreqParams = {param_int("hz", 902000000, 928000000)};

void cmd_dcu_radio_config_freq(Interaction &io, std::span<char *const>)
{
  const long hz = io.param_int(0);
  init_debug_handle();
  if (rfm95_set_frequency(&s_debug_handle, (std::uint32_t)hz))
  {
    io.println("Frequency set to %ld Hz", hz);
  }
  else
  {
    io.error("error", "set_frequency_failed");
  }
}

constexpr std::array kDcuRadioConfigPowerParams = {param_int("dbm", -128, 127)};

void cmd_dcu_radio_config_power(Interaction &io, std::span<char *const>)
{
  const long dbm = io.param_int(0);
  init_debug_handle();
  if (rfm95_set_power(&s_debug_handle, (std::int8_t)dbm))
  {
    io.println("TX power set to %ld dBm", dbm);
  }
  else
  {
    io.error("error", "set_power_failed");
  }
}

constexpr std::array kDcuRadioConfigSfParams = {param_int("sf", 6, 12)};

void cmd_dcu_radio_config_sf(Interaction &io, std::span<char *const>)
{
  init_debug_handle();
  set_modem_nibble(io, "SF", RFM95_REG_MODEM_CONFIG_2, io.param_int(0));
}

constexpr std::array kDcuRadioConfigBwParams = {param_int("code", 0, 9)};

void cmd_dcu_radio_config_bw(Interaction &io, std::span<char *const>)
{
  init_debug_handle();
  set_modem_nibble(io, "BW code", RFM95_REG_MODEM_CONFIG_1, io.param_int(0));
}

constexpr std::array<Command, 4> kRadioConfigSubcommands = {{
    {.name = "freq",
     .description = "Set frequency in Hz",
     .handler = cmd_dcu_radio_config_freq,
     .params = kDcuRadioConfigFreqParams},
    {.name = "power",
     .description = "Set TX power in dBm",
     .handler = cmd_dcu_radio_config_power,
     .params = kDcuRadioConfigPowerParams},
    {.name = "sf",
     .description = "Spreading factor 6-12",
     .handler = cmd_dcu_radio_config_sf,
     .params = kDcuRadioConfigSfParams},
    {.name = "bw",
     .description = "Bandwidth code 0-9",
     .handler = cmd_dcu_radio_config_bw,
     .params = kDcuRadioConfigBwParams},
}};

void cmd_dcu_radio_stats_reset(Interaction &io, std::span<char *const>)
{
  FEB_RFM95_ResetStats();
  io.print("Radio stats reset.\r\n");
}

void cmd_dcu_radio_reset(Interaction &io, std::span<char *const>)
{
  init_debug_handle();
  rfm95_debug_reset(&s_debug_handle);
  io.print("Radio reset.\r\n");
}

/* rfm95_debug_* report through FEB_Log, not this console. */
void cmd_dcu_radio_spi(Interaction &io, std::span<char *const> args)
{
  (void)io;
  init_debug_handle();
  if (args.size() >= 2 && iequal(args[1], "sep"))
  {
    rfm95_debug_spi_separate(&s_debug_handle);
  }
  else if (args.size() >= 2 && iequal(args[1], "raw"))
  {
    rfm95_debug_spi_raw(&s_debug_handle);
  }
  else
  {
    rfm95_debug_spi_poll(&s_debug_handle);
  }
}

void cmd_dcu_radio_en(Interaction &io, std::span<char *const>)
{
  (void)io;
  init_debug_handle();
  rfm95_debug_enable(&s_debug_handle);
}

constexpr std::array<Command, 1> kRadioStatsSubcommands = {{
    {.name = "reset", .description = "Clear the TX/RX counters", .handler = cmd_dcu_radio_stats_reset},
}};

constexpr std::array<Command, 2> kRadioListenSubcommands = {{
    {.name = "on", .description = "Enable listen-only mode", .handler = cmd_dcu_radio_listen_on},
    {.name = "off", .description = "Disable listen-only mode", .handler = cmd_dcu_radio_listen_off},
}};

constexpr std::array<Command, 9> kRadioSubcommands = {{
    {.name = "status", .description = "RSSI/SNR + GPIO/register state", .handler = cmd_dcu_radio_status},
    {.name = "stats", .description = "TX/RX counters", .handler = cmd_dcu_radio_stats, .subs = kRadioStatsSubcommands},
    {.name = "tx", .description = "Transmit a string", .handler = cmd_dcu_radio_tx, .params = kDcuRadioTxParams},
    {.name = "rx",
     .description = "Receive once with timeout",
     .handler = cmd_dcu_radio_rx,
     .params = kDcuRadioRxParams},
    {.name = "listen",
     .description = "Listen-only mode",
     .handler = cmd_dcu_radio_listen,
     .subs = kRadioListenSubcommands},
    {.name = "config", .description = "Radio configuration", .subs = kRadioConfigSubcommands},
    {.name = "reset", .description = "Hardware reset of RFM95", .handler = cmd_dcu_radio_reset},
    {.name = "spi", .description = "Low-level SPI test", .handler = cmd_dcu_radio_spi},
    {.name = "en", .description = "Toggle EN pin test", .handler = cmd_dcu_radio_en},
}};

Interaction *s_print_io = nullptr;

int print_thunk(const char *fmt, ...)
{
  char buf[256];
  std::va_list ap;
  va_start(ap, fmt);
  const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (s_print_io != nullptr)
  {
    s_print_io->print("%s", buf);
  }
  return n;
}

void cmd_dcu_can_stream(Interaction &io, std::span<char *const>)
{
  io.print("CAN stream: %s\r\n", FEB_CAN_Stream_IsStreaming() ? "on" : "off");
}

void cmd_dcu_can_stream_on(Interaction &io, std::span<char *const>)
{
  FEB_CAN_Stream_Start(io);
}

void cmd_dcu_can_stream_off(Interaction &io, std::span<char *const>)
{
  FEB_CAN_Stream_Stop();
  io.print("CAN stream: off\r\n");
}

void cmd_dcu_can_state(Interaction &io, std::span<char *const>)
{
  s_print_io = &io;
  FEB_CAN_State_Print(print_thunk);
  s_print_io = nullptr;
}

constexpr std::array kDcuCanMsgParams = {param_str("name")};

void cmd_dcu_can_msg(Interaction &io, std::span<char *const>)
{
  const char *name = io.param_str(0);

  s_print_io = &io;
  const int rc = FEB_CAN_State_PrintOne(name, print_thunk);
  s_print_io = nullptr;
  if (rc != 0)
  {
    io.error("error", "unknown_message", "%s", name);
  }
}

constexpr std::array<Command, 2> kCanStreamSubcommands = {{
    {.name = "on", .description = "Begin streaming CAN frames as can rows", .handler = cmd_dcu_can_stream_on},
    {.name = "off", .description = "Stop the active CAN-frame stream", .handler = cmd_dcu_can_stream_off},
}};

constexpr std::array<Command, 3> kCanSubcommands = {{
    {.name = "state", .description = "Latest value of each received CAN message", .handler = cmd_dcu_can_state},
    {.name = "msg",
     .description = "Show signals for one CAN message",
     .handler = cmd_dcu_can_msg,
     .params = kDcuCanMsgParams},
    {.name = "stream",
     .description = "Live CAN-frame stream",
     .handler = cmd_dcu_can_stream,
     .subs = kCanStreamSubcommands},
}};

/* ============================================================================
 * Parent dispatcher + tables
 * ============================================================================ */

constexpr std::array<Command, 2> kDcuSubcommands = {{
    group("radio", "Radio status, config, and raw TX/RX", kRadioSubcommands),
    group("can", "CAN state and the live frame stream", kCanSubcommands),
}};

constexpr std::array<Command, 1> kDcuCommands = {{
    group("dcu", "DCU_Receiver board commands", kDcuSubcommands),
}};

} // namespace

namespace dcu
{
inline constexpr auto kAll = concat(kSystemCommands, kDcuCommands);
static_assert(!has_duplicate_names(kAll), "duplicate console command name");
static_assert(params_fit(kAll), "a command declares more params than kMaxParams");

inline constexpr Console kConsole1{FEB_UART_INSTANCE_1, kAll};
inline constexpr Console kConsole2{FEB_UART_INSTANCE_2, kAll};
} // namespace dcu

extern "C" void DCU_Console_ProcessLine(int uart_instance, const char *line, size_t len)
{
  if (uart_instance == FEB_UART_INSTANCE_2)
  {
    dcu::kConsole2.process_line(line, len);
  }
  else
  {
    dcu::kConsole1.process_line(line, len);
  }
}
