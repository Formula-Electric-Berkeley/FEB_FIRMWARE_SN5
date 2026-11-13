/**
 ******************************************************************************
 * @file           : PCU_Commands.cpp
 * @brief          : PCU - Raw Value Retrievals
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "PCU_Commands.h"
#include "PCU_ADC.hpp"
#include "main.h"
#include "usart.h"
#include "feb_uart.h"
#include "feb_log.h"
#include "feb_time.h"
#include "feb_commands_2.hpp"
#include "feb_console_2.hpp"
#include <array>
#include <cstdint>

using namespace feb::console;

namespace
{

uint8_t uart_tx_buf[UART_TX_BUF_SIZE];
uint8_t uart_rx_buf[UART_RX_BUF_SIZE];

void print_channel(Interaction &io, const char *name, uint16_t raw)
{
  float volts = raw * ADC_VREF / ADC_MAX_COUNTS;
  io.println("%s: raw %u, %.3f V", name, (unsigned)raw, (double)volts);
}

void cmd_pcu_apps1(Interaction &io, std::span<char *const>)
{
  print_channel(io, "APPS1", apps1.getRaw());
}

void cmd_pcu_apps2(Interaction &io, std::span<char *const>)
{
  print_channel(io, "APPS2", apps2.getRaw());
}

void cmd_pcu_brake1(Interaction &io, std::span<char *const>)
{
  print_channel(io, "BRAKE1", brake1.getRaw());
}

void cmd_pcu_brake2(Interaction &io, std::span<char *const>)
{
  print_channel(io, "BRAKE2", brake2.getRaw());
}

constexpr std::array<Command, 4> kPcuSubcommands = {{
    {.name = "apps1", .description = "APPS1 raw ADC and pin voltage", .handler = cmd_pcu_apps1},
    {.name = "apps2", .description = "APPS2 raw ADC and pin voltage", .handler = cmd_pcu_apps2},
    {.name = "brake1", .description = "Brake 1 raw ADC and pin voltage", .handler = cmd_pcu_brake1},
    {.name = "brake2", .description = "Brake 2 raw ADC and pin voltage", .handler = cmd_pcu_brake2},
}};

constexpr std::array<Command, 1> kPcuCommands = {{
    group("pcu", "PCU board commands", kPcuSubcommands),
}};

} // namespace

namespace pcu
{
inline constexpr auto kAll = concat(kSystemCommands, kPcuCommands);
static_assert(!has_duplicate_names(kAll), "duplicate console command name");
static_assert(params_fit(kAll), "a command declares more params than kMaxParams");

inline constexpr Console kConsole{FEB_UART_INSTANCE_1, kAll};
} // namespace pcu

extern "C" void PCU_Console_ProcessLine(const char *line, size_t len)
{
  pcu::kConsole.process_line(line, len);
}

void PCU_UART_Init()
{
  FEB_UART_Config_t uart_cfg = {
      .huart = &huart2,
      .hdma_tx = &hdma_usart2_tx,
      .hdma_rx = &hdma_usart2_rx,
      .tx_buffer = uart_tx_buf,
      .tx_buffer_size = sizeof(uart_tx_buf),
      .rx_buffer = uart_rx_buf,
      .rx_buffer_size = sizeof(uart_rx_buf),
      .get_tick_ms = HAL_GetTick,
  };
  if (FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg) != 0)
  {
    Error_Handler();
  }

  FEB_Log_Config_t log_cfg = {
      .uart_instance = FEB_UART_INSTANCE_1,
      .level = FEB_LOG_INFO,
      .colors = true,
      .timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_Log_Init(&log_cfg);

  FEB_Time_Init();

  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, PCU_Console_ProcessLine);
}

void PCU_UART_Process()
{
  FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);
}
