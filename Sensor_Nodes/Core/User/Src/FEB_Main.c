#include "FEB_IMU.h"
#include "FEB_Magnetometer.h"
#include "FEB_SN_Commands.h"
#include "main.h"
#include <string.h>
#include "FEB_Main.h"

// Common libraries
#include "feb_uart.h"
#include "feb_log.h"
#include "feb_console.h"

#define TAG_MAIN "[MAIN]"

extern UART_HandleTypeDef huart2;

static uint8_t uart_tx_buf[1024];
static uint8_t uart_rx_buf[256];

void FEB_Update()
{
  read_Acceleration();
  read_Angular_Rate();
  read_Magnetic_Field_Data();
}

void FEB_Init(void)
{
  // Initialize UART library first (before any LOG calls)
  // Note: No DMA on Sensor Node UART2
  FEB_UART_Config_t uart_cfg = {
      .huart = &huart2,
      .hdma_tx = NULL,
      .hdma_rx = NULL,
      .tx_buffer = uart_tx_buf,
      .tx_buffer_size = sizeof(uart_tx_buf),
      .rx_buffer = uart_rx_buf,
      .rx_buffer_size = sizeof(uart_rx_buf),
      .get_tick_ms = HAL_GetTick,
  };
  FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg);

  // Initialize logging system
  FEB_Log_Config_t log_cfg = {
      .uart_instance = FEB_UART_INSTANCE_1,
      .level = FEB_LOG_DEBUG,
      .colors = true,
      .timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_Log_Init(&log_cfg);

  // Initialize console with default commands
  FEB_Console_Init(true);
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

  // Register Sensor Node specific commands
  SN_RegisterCommands();

  LOG_I(TAG_MAIN, "Sensor Node Starting");

  // Initialize sensors
  lsm6dsox_init();
  LOG_I(TAG_MAIN, "IMU initialized");

  lis3mdl_init();
  LOG_I(TAG_MAIN, "Magnetometer initialized");

  LOG_I(TAG_MAIN, "Sensor Node Setup Complete");
}

void FEB_Main_Loop(void)
{
  FEB_Update();
  FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);
  HAL_Delay(100);
}
