/**
 ******************************************************************************
 * @file           : FEB_Main.c
 * @brief          : UART_TEST Application - Console Demo
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Main.h"
#include "main.h"
#include "feb_uart.h"
#include "feb_uart_log.h"
#include "feb_console.h"
#include "uart_test_commands.h"

/* External HAL handles from CubeMX-generated code */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef handle_GPDMA1_Channel0; /* RX (circular) */
extern DMA_HandleTypeDef handle_GPDMA1_Channel1; /* TX */

/* UART buffers */
static uint8_t uart_tx_buf[512];
static uint8_t uart_rx_buf[256];

#undef TAG_MAIN
#define TAG_MAIN "MAIN"

/* ============================================================================
 * Application Entry Points
 * ============================================================================ */

void FEB_Main_Setup(void)
{
  /* Initialize UART library */
  FEB_UART_Config_t cfg = {
      .huart = &huart1,
      .hdma_tx = &handle_GPDMA1_Channel1,
      .hdma_rx = &handle_GPDMA1_Channel0,
      .tx_buffer = uart_tx_buf,
      .tx_buffer_size = sizeof(uart_tx_buf),
      .rx_buffer = uart_rx_buf,
      .rx_buffer_size = sizeof(uart_rx_buf),
      .log_level = FEB_UART_LOG_DEBUG,
      .enable_colors = true,
      .enable_timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };

  if (FEB_UART_Init(&cfg) != 0)
  {
    /* UART init failed - can't log, just spin */
    while (1)
    {
    }
  }

  /* Initialize console (registers built-in commands) */
  FEB_Console_Init();

  /* Register UART_TEST custom commands */
  UART_TEST_RegisterCommands();

  /* Connect UART RX to console processor */
  FEB_UART_SetRxLineCallback(FEB_Console_ProcessLine);

  /* Welcome message - immediate (should show [0] timestamp) */
  LOG_I(TAG_MAIN, "========================================");
  LOG_I(TAG_MAIN, "UART_TEST Console Ready (boot)");

  /* Wait 100ms to verify timestamp increments */
  HAL_Delay(100);

  /* Second message group - should show [~100] timestamp */
  LOG_I(TAG_MAIN, "UART_TEST Console Ready (after 100ms delay)");
  LOG_I(TAG_MAIN, "Use | as delimiter: echo|hello world");
  LOG_I(TAG_MAIN, "Type 'help' for available commands");
  LOG_I(TAG_MAIN, "========================================");
}

void FEB_Main_Loop(void)
{
  /* Process incoming UART data */
  FEB_UART_ProcessRx();
}
