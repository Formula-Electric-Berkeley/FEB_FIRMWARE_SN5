/**
 ******************************************************************************
 * @file           : FEB_Main.c
 * @brief          : DASH Application - Console and Communication
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DASH_Main.h"
#include "DASH_CAN.h"
#include "DASH_PingPong.h"
#include "main.h"
#include "feb_uart.h"
#include "feb_log.h"
#include "feb_time.h"
#include "cmsis_os2.h"

/* External HAL handles from CubeMX-generated code */
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart3_tx;

/* UART buffers */
static uint8_t uart_tx_buf[512];
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

/* ============================================================================
 * Application Entry Points
 * ============================================================================ */

void DASH_Init(void)
{
  /* Initialize UART library with DMA */
  FEB_UART_Config_t cfg = {
      .huart = &huart3,
      .hdma_tx = &hdma_usart3_tx,
      .hdma_rx = &hdma_usart3_rx,
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

  if (FEB_UART_Init(FEB_UART_INSTANCE_1, &cfg) != 0)
  {
    HAL_UART_Transmit(&huart3, (uint8_t *)"UART Init Failed\r\n", 18, 100);
    while (1)
    {
    }
  }

  /* Initialize logging system */
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

  FEB_Time_Init();

  /* Startup banner */
  static const char banner[] = "\r\n"
                               "========================================\r\n"
                               "           DASH Console Ready\r\n"
                               "========================================\r\n"
                               "Type 'help' for available commands\r\n"
                               "\r\n";
  FEB_UART_Write(FEB_UART_INSTANCE_1, (const uint8_t *)banner, sizeof(banner) - 1);
}
