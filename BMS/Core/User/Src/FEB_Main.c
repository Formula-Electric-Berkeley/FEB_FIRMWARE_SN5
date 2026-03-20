/**
 ******************************************************************************
 * @file           : FEB_Main.c
 * @brief          : BMS Application - Console Demo
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Main.h"
#include "main.h"
#include "feb_uart.h"
#include "feb_uart_config.h"
#include "feb_log.h"
#include "feb_console.h"
#include "FEB_Commands.h"
#include "FEB_CAN_State.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_SM.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

/* Logging tag */
#define TAG_MAIN "[MAIN]"

/* External HAL handles from CubeMX-generated code */
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;

/* UART buffers */
static uint8_t uart_tx_buf[512];
static uint8_t uart_rx_buf[256];

/* ============================================================================
 * Application Entry Points
 * ============================================================================ */

void FEB_Init(void)
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
      .enable_rx_queue = true,
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
  };
  FEB_Log_Init(&log_cfg);

  /* Initialize console (registers built-in commands) */
  FEB_Console_Init(true);

  /* Register BMS custom commands */
  BMS_RegisterCommands();

  /* Initialize CAN state publisher */
  FEB_CAN_State_Init();

  /* Initialize state machine (sets relays to safe state, transitions to LV_POWER) */
  FEB_SM_Init();

  /* Log after all subsystems initialized */
  LOG_I(TAG_MAIN, "BMS initialization complete");

  /* Startup banner */
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("========================================\r\n");
  FEB_Console_Printf("        BMS Console Ready\r\n");
  FEB_Console_Printf("========================================\r\n");
  FEB_Console_Printf("Use | as delimiter: BMS|status\r\n");
  FEB_Console_Printf("Type 'help' for available commands\r\n");
  FEB_Console_Printf("\r\n");
}

/* ============================================================================
 * FreeRTOS Task Implementations - Override weak stubs in freertos.c
 * ============================================================================ */

void StartUartRxTask(void *argument)
{
  (void)argument;

  /* FEB_Init() is called from main() before kernel starts */

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

void StartSMTask(void *argument)
{
  (void)argument;
  static uint16_t pingpong_divider = 0;

  for (;;)
  {
    /* Wait for notification from ISR (1ms tick) */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    /* State machine processing */
    FEB_SM_Process();

    /* CAN state publishing (every 100ms via internal divider in function) */
    FEB_CAN_State_Tick();

    /* PingPong tick every 100ms */
    pingpong_divider++;
    if (pingpong_divider >= 100)
    {
      pingpong_divider = 0;
      FEB_CAN_PingPong_Tick();
    }
  }
}
