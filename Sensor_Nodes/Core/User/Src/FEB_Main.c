#include "FEB_Main.h"
#include "FEB_SN_Config.h"
#include "main.h"

#include "cmsis_os2.h"
#include "feb_console.h"
#include "feb_log.h"
#include "feb_uart.h"

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;

extern osMutexId_t logMutexHandle;
extern osMutexId_t uartTxMutexHandle;
extern osMessageQueueId_t uartRxQueueHandle;

static uint8_t uart_tx_buf[1024];
static uint8_t uart_rx_buf[256];

void SN_Init(void)
{
  FEB_UART_Config_t uart_cfg = {
      .huart = &huart2,
      .hdma_tx = &hdma_usart2_tx,
      .hdma_rx = &hdma_usart2_rx,
      .tx_buffer = uart_tx_buf,
      .tx_buffer_size = sizeof(uart_tx_buf),
      .rx_buffer = uart_rx_buf,
      .rx_buffer_size = sizeof(uart_rx_buf),
      .get_tick_ms = HAL_GetTick,
      .tx_mutex = uartTxMutexHandle,
      .enable_rx_queue = true,
      .rx_queue = uartRxQueueHandle,
  };
  if (FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg) != FEB_UART_OK)
  {
    Error_Handler();
  }

  FEB_Log_Config_t log_cfg = {
      .uart_instance = FEB_UART_INSTANCE_1,
      .level = FEB_LOG_TRACE,
      .colors = true,
      .timestamps = true,
      .get_tick_ms = HAL_GetTick,
      .mutex = logMutexHandle,
  };
  FEB_Log_Init(&log_cfg);

  FEB_Console_Init(true);

  FEB_Console_Printf("Sensor Node (%s) Starting\r\n", FEB_SN_VARIANT_NAME);
}
