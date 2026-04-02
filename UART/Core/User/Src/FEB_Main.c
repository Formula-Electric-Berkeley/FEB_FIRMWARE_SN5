/**
 ******************************************************************************
 * @file           : FEB_Main.c
 * @brief          : UART Application - Console Demo
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Main.h"
#include "main.h"
#include "feb_uart.h"
#include "feb_log.h"
#include "feb_console.h"
#include "uart_commands.h"
#include "feb_rtc.h"
#include "rtc_commands.h"
#include "cmsis_os2.h"

/* External HAL handles from CubeMX-generated code */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

/* FreeRTOS handles from CubeMX (freertos.c) */
extern osMutexId_t uartTxMutexHandle;
extern osSemaphoreId_t uartTxSemHandle;
extern osMessageQueueId_t uartRxQueueHandle;

/* UART buffers */
static uint8_t uart_tx_buf[512];
static uint8_t uart_rx_buf[256];

/* External FreeRTOS handles from .ioc-generated code */
#if FEB_LOG_USE_FREERTOS
extern osMutexId_t logMutexHandle;
#endif

/* ============================================================================
 * Application Entry Points
 * ============================================================================ */

void FEB_Main_Setup(void)
{
  /* Initialize UART library */
  FEB_UART_Config_t cfg = {
      .huart = &huart2,
      .hdma_tx = &hdma_usart2_tx,
      .hdma_rx = &hdma_usart2_rx,
      .tx_buffer = uart_tx_buf,
      .tx_buffer_size = sizeof(uart_tx_buf),
      .rx_buffer = uart_rx_buf,
      .rx_buffer_size = sizeof(uart_rx_buf),
      .get_tick_ms = HAL_GetTick,
      .tx_mutex = uartTxMutexHandle,
      .tx_complete_sem = uartTxSemHandle,
      .enable_rx_queue = true,
      .rx_queue = uartRxQueueHandle,
  };

  if (FEB_UART_Init(FEB_UART_INSTANCE_1, &cfg) != 0)
  {
    HAL_UART_Transmit(&huart2, (uint8_t *)"UART Init Failed\r\n", 18, 100);
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

  /* Initialize RTC helper (creates mutex) */
  FEB_RTC_Init();

  /* Initialize console (registers built-in commands) */
  FEB_Console_Init(true);

  /* Register UART custom commands */
  UART_RegisterCommands();

  /* Register RTC commands */
  RTC_RegisterCommands();

  /* Startup banner */
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("========================================\r\n");
  FEB_Console_Printf("        UART Console Ready\r\n");
  FEB_Console_Printf("========================================\r\n");
  FEB_Console_Printf("Use | as delimiter: echo|hello world\r\n");
  FEB_Console_Printf("Type 'help' for available commands\r\n");
  FEB_Console_Printf("\r\n");
}

/* ============================================================================
 * FreeRTOS Task Implementations - Override weak stubs in freertos.c
 * ============================================================================ */

void StartUartRxTask(void *argument)
{
  (void)argument;

  /* Initialize UART library (must happen after FreeRTOS kernel starts) */
  FEB_Main_Setup();

  char line_buf[FEB_UART_QUEUE_LINE_SIZE];
  size_t line_len;

  for (;;)
  {
    /* Process RX data - extracts from DMA buffer, posts complete lines to queue */
    FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);

    /* Receive from queue with 10ms timeout */
    if (FEB_UART_QueueReceiveLine(FEB_UART_INSTANCE_1, line_buf, sizeof(line_buf), &line_len, 10))
    {
      FEB_Console_ProcessLine(line_buf, line_len);
    }
  }
}
