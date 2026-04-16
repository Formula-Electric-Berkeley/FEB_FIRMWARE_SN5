/**
 ******************************************************************************
 * @file           : FEB_Main.c
 * @brief          : DCU Application - Console and Communication
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Main.h"
#include "DCU_CAN.h"
#include "DCU_TPS.h"
#include "DCU_Commands.h"
#include "main.h"
#include "feb_uart.h"
#include "feb_uart_config.h"
#include "feb_log.h"
#include "feb_console.h"
#include "feb_can_lib.h"
#include "cmsis_os2.h"

/* External HAL handles from CubeMX-generated code */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

/* External FreeRTOS handles from CubeMX-generated code */
extern osMutexId_t logMutexHandle;
extern osMutexId_t uartTxMutexHandle;
extern osSemaphoreId_t uartTxSemHandle;
extern osMessageQueueId_t uartRxQueueHandle;

/* UART buffers */
static uint8_t uart_tx_buf[512];
static uint8_t uart_rx_buf[256];

/* CAN init status */
static bool can_init_success = false;

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

  /* Initialize CAN with accept-all filter */
  can_init_success = DCU_CAN_Init();

  /* Initialize TPS subsystem */
  DCU_TPS_Init();

  /* Startup banner */
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("========================================\r\n");
  FEB_Console_Printf("         DCU Console Ready\r\n");
  FEB_Console_Printf("========================================\r\n");
  FEB_Console_Printf("Use | as delimiter: dcu|tps\r\n");
  FEB_Console_Printf("Type 'help' for available commands\r\n");
  FEB_Console_Printf("\r\n");
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
      FEB_Console_ProcessLine(line_buf, line_len);
    }
  }
}
