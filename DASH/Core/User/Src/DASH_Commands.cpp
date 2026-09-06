/**
 ******************************************************************************
 * @file           : DASH_Commands.cpp
 * @brief          : Console commands for DASH (CAN telemetry, ping/pong, I2C)
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DASH_Commands.h"

#include "DASH_I2C.h"
#include "DASH_IO.h"
#include "DASH_PingPong.h"
#include "feb_can_lib.h"
#include "feb_can_subscriber.hpp"
#include "feb_commands_2.hpp"
#include "feb_console_2.hpp"
#include "i2c.h"

#include <cstdint>

namespace fc = feb::can;
namespace fm = feb::can::msg;

using namespace feb::console;

namespace
{

constexpr const char *kPingPongModeNames[] = {"OFF", "PING", "PONG", "PINGPONG"};
constexpr std::uint32_t kPingPongFrameIds[] = {0xE0, 0xE1, 0xE2, 0xE3};

void cmd_dash_can_status(Interaction &io, std::span<char *const>)
{
  {
    static constexpr Column kCols[] = {{"Ch", 4},       {"Frame", 6}, {"Mode", 8},    {"TX OK", 10},
                                       {"TX Fail", 10}, {"RX", 10},   {"Last RX", 10}};
    Table t(io, kCols, "CAN Ping/Pong");

    for (std::uint8_t ch = 1; ch <= FEB_PINGPONG_NUM_CHANNELS; ch++)
    {
      t.cell("%u", (unsigned)ch);
      t.cell("0x%02X", (unsigned)kPingPongFrameIds[ch - 1]);
      t.cell("%s", kPingPongModeNames[FEB_CAN_PingPong_GetMode(ch)]);
      t.cell("%lu", (unsigned long)FEB_CAN_PingPong_GetTxCount(ch));
      t.cell("%lu", (unsigned long)FEB_CAN_PingPong_GetTxFailCount(ch));
      t.cell("%lu", (unsigned long)FEB_CAN_PingPong_GetRxCount(ch));
      t.cell("%ld", (long)FEB_CAN_PingPong_GetLastCounter(ch));
      t.end_row();
    }
  }

  KVTable t(io, 18, 12, "CAN Library Errors");

  t.row("HAL errors", "%lu", (unsigned long)FEB_CAN_GetHalErrorCount());
  t.row("TX timeout", "%lu", (unsigned long)FEB_CAN_GetTxTimeoutCount());
  t.row("TX queue overflow", "%lu", (unsigned long)FEB_CAN_GetTxQueueOverflowCount());
  t.row("RX queue overflow", "%lu", (unsigned long)FEB_CAN_GetRxQueueOverflowCount());
}

void set_pingpong_mode(Interaction &io, long ch, FEB_PingPong_Mode_t mode)
{
  FEB_CAN_PingPong_SetMode((std::uint8_t)ch, mode);
  io.println("Channel %ld (0x%02X): %s mode started", ch, (unsigned)kPingPongFrameIds[ch - 1],
             kPingPongModeNames[mode]);
}

constexpr std::array kDashCanPingParams = {param_int("channel", 1, FEB_PINGPONG_NUM_CHANNELS)};

void cmd_dash_can_ping(Interaction &io, std::span<char *const>)
{
  set_pingpong_mode(io, io.param_int(0), PINGPONG_MODE_PING);
}

constexpr std::array kDashCanPongParams = {param_int("channel", 1, FEB_PINGPONG_NUM_CHANNELS)};

void cmd_dash_can_pong(Interaction &io, std::span<char *const>)
{
  const long ch = io.param_int(0);
  const FEB_PingPong_Mode_t mode =
      (FEB_CAN_PingPong_GetMode((std::uint8_t)ch) == PINGPONG_MODE_PING) ? PINGPONG_MODE_PINGPONG : PINGPONG_MODE_PONG;
  set_pingpong_mode(io, ch, mode);
}

constexpr std::array kDashCanStopParams = {param_str("channel")};

void cmd_dash_can_stop(Interaction &io, std::span<char *const> args)
{
  if (iequal(io.param_str(0), "all"))
  {
    FEB_CAN_PingPong_Reset();
    io.println("All channels stopped");
    return;
  }

  long ch = 0;
  if (!io.arg_int(args, 1, &ch, 1, FEB_PINGPONG_NUM_CHANNELS))
  {
    return;
  }
  FEB_CAN_PingPong_SetMode((std::uint8_t)ch, PINGPONG_MODE_OFF);
  io.println("Channel %ld stopped", ch);
}

constexpr std::array<Command, 4> kCanSubcommands = {{
    {.name = "status", .description = "Channel modes, counters, and library errors", .handler = cmd_dash_can_status},
    {.name = "ping",
     .description = "Transmit on a channel",
     .handler = cmd_dash_can_ping,
     .params = kDashCanPingParams},
    {.name = "pong",
     .description = "Answer a channel (PINGPONG if it already pings)",
     .handler = cmd_dash_can_pong,
     .params = kDashCanPongParams},
    {.name = "stop",
     .description = "Stop one channel or all of them",
     .handler = cmd_dash_can_stop,
     .params = kDashCanStopParams},
}};

void cmd_dash_lvpdb(Interaction &io, std::span<char *const>)
{
  const auto &v = fc::rx<fm::LvpdbLv24vBusAnd12vBusVoltages>.v();

  KVTable t(io, 10, 12, "LVPDB Voltages");

  t.row("24V bus", "%u raw", (unsigned)v.lv_24v_voltage);
  t.row("12V bus", "%u raw", (unsigned)v.lv_12v_voltage);
}

void cmd_dash_bms(Interaction &io, std::span<char *const>)
{
  KVTable t(io, 14, 12, "BMS Status");

  t.row("State", "%d raw", (int)fc::rx<fm::BmsState>.v().bms_state);
  t.row("Max cell temp", "%d raw", (int)fc::rx<fm::BmsAccumulatorTemperature>.v().max_cell_temperature);
  t.row("Pack voltage", "%u raw", (unsigned)fc::rx<fm::BmsAccumulatorVoltage>.v().total_pack_voltage);
}

void cmd_dash_pcu(Interaction &io, std::span<char *const>)
{
  const auto &inv = fc::rx<fm::M192CommandMessage>.v();

  KVTable t(io, 14, 12, "PCU / RMS Status");

  t.row("Torque", "%d raw", (int)inv.vcu_inv_torque_command);
  t.row("Direction", "%d raw", (int)inv.vcu_inv_direction_command);
  t.row("RMS enabled", "%d raw", (int)inv.vcu_inv_inverter_enable);
  t.row("Brake position", "%u raw", (unsigned)fc::rx<fm::Brake>.v().brake_position);
}

void cmd_dash_i2c_scan(Interaction &io, std::span<char *const>)
{
  int found = 0;

  {
    static constexpr Column kCols[] = {{"Address", 8}, {"Device", 14}};
    Table t(io, kCols, "I2C1 Scan");

    for (std::uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
      if (FEB_I2C_IsDeviceReady(&hi2c1, (std::uint16_t)(addr << 1), 1, 5) != HAL_OK)
      {
        continue;
      }
      t.cell("0x%02X", (unsigned)addr);
      t.cell("%s", (addr == IOEXP_ADDR) ? "IO expander" : "");
      t.end_row();
      found++;
    }
  }

  io.println("%d device(s) found", found);

  if (FEB_I2C_IsDeviceReady(&hi2c1, (std::uint16_t)(IOEXP_ADDR << 1), 2, 5) != HAL_OK)
  {
    io.error("warn", "ioexp_not_responding", "0x%02X", (unsigned)IOEXP_ADDR);
  }
}

constexpr std::array<Command, 5> kDashSubcommands = {{
    {.name = "lvpdb", .description = "LVPDB 24V and 12V bus voltages", .handler = cmd_dash_lvpdb},
    {.name = "bms", .description = "BMS state, max cell temperature, pack voltage", .handler = cmd_dash_bms},
    {.name = "pcu", .description = "Torque, direction, RMS enable, brake position", .handler = cmd_dash_pcu},
    {.name = "i2c-scan", .description = "Scan I2C1 and list responding addresses", .handler = cmd_dash_i2c_scan},
    {.name = "can", .description = "CAN ping/pong test channels", .subs = kCanSubcommands},
}};

constexpr std::array<Command, 1> kDashCommands = {{
    group("dash", "DASH board commands", kDashSubcommands),
}};

} // namespace

namespace dash
{
inline constexpr auto kAll = concat(kSystemCommands, kDashCommands);
static_assert(!has_duplicate_names(kAll), "duplicate console command name");
static_assert(params_fit(kAll), "a command declares more params than kMaxParams");

inline constexpr Console kConsole{FEB_UART_INSTANCE_1, kAll};
} // namespace dash

extern "C" void DASH_Console_ProcessLine(const char *line, size_t len)
{
  dash::kConsole.process_line(line, len);
}
