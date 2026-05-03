#include "FEB_main.h"

#include "FEB_DART_Commands.h"
#include "feb_console.h"
#include "feb_log.h"
#include "feb_uart.h"

extern CAN_HandleTypeDef hcan;
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern uint16_t frequency[NUM_FANS];

#define DART_TACH_TX_PERIOD_MS 100u // 10 Hz

// Keep these small — STM32F042K6 only has 6 KB SRAM.
static uint8_t uart_tx_buf[256];
static uint8_t uart_rx_buf[128];

static uint32_t last_tach_tx_ms = 0;

static void FEB_IO_Init(void)
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
  };
  FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg);

  FEB_Log_Config_t log_cfg = {
      .uart_instance = FEB_UART_INSTANCE_1,
      .level = FEB_LOG_INFO,
      .colors = true,
      .timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_Log_Init(&log_cfg);

  FEB_Console_Init(true);
  DART_RegisterCommands();
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);
}

void FEB_Init(void)
{
  FEB_IO_Init();
  FEB_CAN_Init();
  FEB_Fan_Init();
  LOG_I(TAG_MAIN, "DART boot: fans@100%%, CAN 500kbps, tach-TX @10Hz");
}

void FEB_Main_Loop(void)
{
  FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);

  uint32_t now = HAL_GetTick();

  if ((now - last_tach_tx_ms) >= DART_TACH_TX_PERIOD_MS)
  {
    last_tach_tx_ms = now;
    FEB_CAN_Transmit(&hcan, frequency);
  }

  FEB_Fan_Watchdog_Tick();
}
