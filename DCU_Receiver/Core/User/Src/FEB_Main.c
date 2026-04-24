/**
 ******************************************************************************
 * @file           : FEB_Main.c
 * @brief          : DCU Application - Console and Communication
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Main.h"
#include "DCU_Commands.h"
#include "main.h"
#include "feb_uart.h"
#include "feb_uart_config.h"
#include "feb_log.h"
#include "feb_console.h"
#include "cmsis_os2.h"

/* External HAL handles from CubeMX-generated code */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern UART_HandleTypeDef huart4;
extern DMA_HandleTypeDef hdma_uart4_rx;
extern DMA_HandleTypeDef hdma_uart4_tx;

/* External FreeRTOS handles from CubeMX-generated code */
extern osMutexId_t logMutexHandle;
extern osMutexId_t uartTxMutexHandle;
extern osSemaphoreId_t uartTxSemHandle;
extern osMessageQueueId_t uartRxQueueHandle;
/* Second-console handles (defined in freertos.c USER CODE block) */
extern osMutexId_t uartTxMutex2Handle;
extern osSemaphoreId_t uartTxSem2Handle;
extern osMessageQueueId_t uartRxQueue2Handle;

/* UART buffers - per instance */
static uint8_t uart_tx_buf[512];
static uint8_t uart_rx_buf[256];
static uint8_t uart_tx_buf2[512];
static uint8_t uart_rx_buf2[256];

/* ============================================================================
 * Application Entry Points
 * ============================================================================ */

void FEB_Init(void)
{
  /* Initialize UART library with DMA */
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

  /* Initialize UART4 as a second independent console instance */
  FEB_UART_Config_t cfg2 = {
      .huart = &huart4,
      .hdma_tx = &hdma_uart4_tx,
      .hdma_rx = &hdma_uart4_rx,
      .tx_buffer = uart_tx_buf2,
      .tx_buffer_size = sizeof(uart_tx_buf2),
      .rx_buffer = uart_rx_buf2,
      .rx_buffer_size = sizeof(uart_rx_buf2),
      .get_tick_ms = HAL_GetTick,
      .tx_mutex = uartTxMutex2Handle,
      .tx_complete_sem = uartTxSem2Handle,
      .enable_rx_queue = true,
      .rx_queue = uartRxQueue2Handle,
  };
  if (FEB_UART_Init(FEB_UART_INSTANCE_2, &cfg2) != 0)
  {
    HAL_UART_Transmit(&huart4, (uint8_t *)"UART4 Init Failed\r\n", 19, 100);
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
      .mutex = logMutexHandle,
  };
  FEB_Log_Init(&log_cfg);

  LOG_I(TAG_MAIN, "DCU initializing...");

  /* Initialize console (registers built-in commands: echo, help, version, uptime, reboot, log) */
  FEB_Console_Init(true);

  /* Register DCU-specific commands */
  DCU_RegisterCommands();

  /* Startup banner on both consoles */
  static const char banner[] =
      "\r\n"
      "========================================\r\n"
      "      DCU_Receiver Console Ready\r\n"
      "========================================\r\n"
      "Use | as delimiter: dcu|radio, dcu|can\r\n"
      "Type 'help' for available commands\r\n"
      "\r\n";
  FEB_UART_Write(FEB_UART_INSTANCE_1, (const uint8_t *)banner, sizeof(banner) - 1);
  FEB_UART_Write(FEB_UART_INSTANCE_2, (const uint8_t *)banner, sizeof(banner) - 1);
}

/* ============================================================================
 * FreeRTOS Task Implementations - Override weak stubs in freertos.c
 * ============================================================================ */

void StartUartRxTask(void *argument)
{
  (void)argument;

  char line_buf[FEB_UART_QUEUE_LINE_SIZE];
  size_t line_len;

  for (;;)
  {
    /* Process RX data - extracts from DMA buffer, posts complete lines to queue */
    FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);

    /* Receive from queue with 10ms timeout */
    if (FEB_UART_QueueReceiveLine(FEB_UART_INSTANCE_1, line_buf, sizeof(line_buf), &line_len, 10))
    {
      FEB_Console_ProcessLineOnInstance(FEB_UART_INSTANCE_1, line_buf, line_len);
    }
  }
}

void StartUart4RxTask(void *argument)
{
  (void)argument;

  char line_buf[FEB_UART_QUEUE_LINE_SIZE];
  size_t line_len;

  for (;;)
  {
    FEB_UART_ProcessRx(FEB_UART_INSTANCE_2);
    if (FEB_UART_QueueReceiveLine(FEB_UART_INSTANCE_2, line_buf, sizeof(line_buf), &line_len, 10))
    {
      FEB_Console_ProcessLineOnInstance(FEB_UART_INSTANCE_2, line_buf, line_len);
    }
  }
}
