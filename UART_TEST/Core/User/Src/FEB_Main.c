/**
 ******************************************************************************
 * @file           : FEB_Main.c
 * @brief          : UART_TEST Application - Console Demo
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

/* Enable FreeRTOS support in UART library */
#define FEB_UART_USE_FREERTOS 1

#include "FEB_Main.h"
#include "main.h"
#include "feb_uart.h"
#include "feb_uart_log.h"
#include "feb_console.h"
#include "uart_test_commands.h"
#include "cmsis_os2.h"
#include "app_freertos.h"
#include <string.h>

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
  HAL_UART_Transmit(&huart1, (uint8_t *)"DBG:1-PreCfg\r\n", 14, 100);

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

  HAL_UART_Transmit(&huart1, (uint8_t *)"DBG:2-PreUARTInit\r\n", 19, 100);

  if (FEB_UART_Init(FEB_UART_INSTANCE_1, &cfg) != 0)
  {
    HAL_UART_Transmit(&huart1, (uint8_t *)"DBG:FAIL-UARTInit\r\n", 19, 100);
    while (1)
    {
    }
  }

  HAL_UART_Transmit(&huart1, (uint8_t *)"DBG:3-PostUARTInit\r\n", 20, 100);

  /* Initialize console (registers built-in commands) */
  FEB_Console_Init();

  HAL_UART_Transmit(&huart1, (uint8_t *)"DBG:4-PostConsole\r\n", 19, 100);

  /* Register UART_TEST custom commands */
  UART_TEST_RegisterCommands();

  /* Connect UART RX to console processor */
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

  HAL_UART_Transmit(&huart1, (uint8_t *)"DBG:5-PreLOG_I\r\n", 16, 100);

  /* Welcome message */
  LOG_I(TAG_MAIN, "========================================");

  HAL_UART_Transmit(&huart1, (uint8_t *)"DBG:6-PostLOG_I\r\n", 17, 100);

  LOG_I(TAG_MAIN, "UART_TEST Console Ready (FreeRTOS)");
  LOG_I(TAG_MAIN, "Use | as delimiter: echo|hello world");
  LOG_I(TAG_MAIN, "Type 'help' for available commands");
  LOG_I(TAG_MAIN, "========================================");

  HAL_UART_Transmit(&huart1, (uint8_t *)"DBG:7-AllDone\r\n", 15, 100);
}

void FEB_Main_Loop(void)
{
  /* UART processing moved to FreeRTOS tasks */
}

/* ============================================================================
 * FreeRTOS Task Implementations - Override weak stubs in app_freertos.c
 * ============================================================================ */

void StartUartRxTask(void *argument)
{
  (void)argument;

  /* Direct HAL test - bypass all library code */
  const char *test_msg = "HAL Direct Test\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t *)test_msg, strlen(test_msg), 1000);

  /* Initialize UART library (must happen after FreeRTOS kernel starts) */
  FEB_Main_Setup();

  for (;;)
  {
    FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);
    osDelay(10); /* 10ms polling interval */
  }
}

void StartUartTxTask(void *argument)
{
  (void)argument;
  for (;;)
  {
    osDelay(100); /* TX task placeholder - currently unused */
  }
}
