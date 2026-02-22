/**
 ******************************************************************************
 * @file           : FEB_Main.c
 * @brief          : DASH Application - Console and Communication
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Main.h"
#include "main.h"
#include "feb_uart.h"
#include "feb_uart_config.h"
#include "feb_console.h"
#include "FEB_CAN_State.h"
#include "cmsis_os2.h"

/* External HAL handles from CubeMX-generated code */
extern UART_HandleTypeDef huart3;

/* UART buffers - polling mode (no DMA) */
static uint8_t uart_tx_buf[512];
static uint8_t uart_rx_buf[256];

/* ============================================================================
 * Application Entry Points
 * ============================================================================ */

void FEB_Init(void)
{
  /* Initialize UART library - polling mode (no DMA handles) */
  FEB_UART_Config_t cfg = {
      .huart = &huart3,
      .hdma_tx = NULL, /* No DMA for TX */
      .hdma_rx = NULL, /* No DMA for RX */
      .tx_buffer = uart_tx_buf,
      .tx_buffer_size = sizeof(uart_tx_buf),
      .rx_buffer = uart_rx_buf,
      .rx_buffer_size = sizeof(uart_rx_buf),
      .log_level = FEB_UART_LOG_DEBUG,
      .enable_colors = true,
      .enable_timestamps = true,
      .get_tick_ms = HAL_GetTick,
      .enable_rx_queue = true,
  };

  if (FEB_UART_Init(FEB_UART_INSTANCE_1, &cfg) != 0)
  {
    HAL_UART_Transmit(&huart3, (uint8_t *)"UART Init Failed\r\n", 18, 100);
    while (1)
    {
    }
  }

  /* Initialize console (registers built-in commands: echo, help, version, uptime, reboot, log) */
  FEB_Console_Init();

  /* Initialize CAN state publisher */
  FEB_CAN_State_Init();

  /* Startup banner */
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("========================================\r\n");
  FEB_Console_Printf("        DASH Console Ready\r\n");
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

  char line_buf[FEB_UART_QUEUE_LINE_SIZE];
  size_t line_len;

  for (;;)
  {
    /* Process RX data - extracts from buffer, posts complete lines to queue */
    FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);

    /* Receive from queue with 10ms timeout */
    if (FEB_UART_QueueReceiveLine(FEB_UART_INSTANCE_1, line_buf, sizeof(line_buf), &line_len, 10))
    {
      FEB_Console_ProcessLine(line_buf, line_len);
    }
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
