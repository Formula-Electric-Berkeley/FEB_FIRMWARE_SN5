/**
 ******************************************************************************
 * @file           : LVPDB_Main.cpp
 * @brief          : LVPDB Application - Console and Communication
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "LVPDB_Main.h"
#include "LVPDB_Commands.h"
#include "LVPDB_TPS.h"
#include "cmsis_os2.h"
#include "feb_console.h"
#include "feb_log.h"
#include "feb_uart.h"
#include "main.h"

/* External HAL handles from CubeMX-generated code */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;

/* UART buffers */
static uint8_t uart_tx_buf[4096];
static uint8_t uart_rx_buf[256];

/* External FreeRTOS handles from .ioc-generated code */
#if FEB_LOG_USE_FREERTOS
extern osMutexId_t logMutexHandle;
#endif

#if FEB_UART_USE_FREERTOS
extern osMutexId_t uartTxMutexHandle;
extern osSemaphoreId_t uartTxSemHandle;
extern osMessageQueueId_t uartRxQueueHandle;
#endif

namespace
{
volatile bool setup_complete = false;
} // namespace

/* ============================================================================
 * Application Entry Points
 * ============================================================================ */

bool LVPDB_IsSetupComplete(void)
{
  return setup_complete;
}

/**
 * Perform board startup sequence and configure main subsystems.
 *
 * Initializes UART and console, scans the I2C bus for attached devices,
 * initializes and configures TPS2482 power-management devices (disabling all
 * non-LV rails, then enabling the shutdown (SH) rail and verifying power-good
 * signals), initializes
 * the CAN subsystem and its ping/pong module, configures the brake-light GPIO
 * to off, and starts the 1 kHz timer interrupt for regular system ticks.
 */

void LVPDB_Init(void)
{
  // Initialize UART library first (before any LOG calls)
  FEB_UART_Config_t uart_cfg = {
      .huart = &huart2,
      .hdma_tx = &hdma_usart2_tx,
      .hdma_rx = &hdma_usart2_rx,
      .tx_buffer = uart_tx_buf,
      .tx_buffer_size = sizeof(uart_tx_buf),
      .rx_buffer = uart_rx_buf,
      .rx_buffer_size = sizeof(uart_rx_buf),
      .get_tick_ms = HAL_GetTick,
#if FEB_UART_USE_FREERTOS
      .tx_mutex = uartTxMutexHandle,
      .tx_complete_sem = uartTxSemHandle,
      .enable_rx_queue = true,
      .rx_queue = uartRxQueueHandle,
#endif
  };
  FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg);

  // Initialize logging system
  FEB_Log_Config_t log_cfg = {
      .uart_instance = FEB_UART_INSTANCE_1,
      .level = FEB_LOG_DEBUG,
      .colors = true,
      .timestamps = true,
      .get_tick_ms = HAL_GetTick,
#if FEB_LOG_USE_FREERTOS
      .mutex = logMutexHandle,
#endif
  };
  FEB_Log_Init(&log_cfg);

  // Initialize console
  FEB_Console_Init(true);
  LVPDB_RegisterCommands();

  LOG_I(TAG_MAIN, "Beginning Setup");

  // I2C scan for debugging
  FEB_Console_Printf("Starting I2C Scanning: \r\n");
  for (uint8_t i = 1; i < 128; i++)
  {
    HAL_StatusTypeDef ret = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(i << 1), 3, 5);
    if (ret == HAL_OK)
    {
      FEB_Console_Printf("0x%X ", i);
    }
    else
    {
      FEB_Console_Printf("- ");
    }
  }
  FEB_Console_Printf("Done! \r\n\r\n");

  LVPDB_TPS_Setup();

  // Initialize brake light to be off
  HAL_GPIO_WritePin(BL_Switch_GPIO_Port, BL_Switch_Pin, GPIO_PIN_RESET);

  LOG_I(TAG_MAIN, "LVPDB Setup Complete");
  LOG_I(TAG_MAIN, "Type 'help' for available commands");

  setup_complete = true;
}
