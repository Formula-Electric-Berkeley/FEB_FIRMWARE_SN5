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
#include "FEB_CAN_Charger.h"
#include "FEB_SM.h"
#include "FEB_Const.h"
#include "feb_time.h"
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
static uint8_t uart_tx_buf[1024];
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
#if FEB_UART_USE_FREERTOS
      .tx_mutex = uartTxMutexHandle,
      .tx_complete_sem = uartTxSemHandle,
      .enable_rx_queue = true,
      .rx_queue = uartRxQueueHandle,
#endif
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

  /* Initialize console (registers built-in commands) */
  FEB_Console_Init(true);

  /* Register BMS custom commands */
  BMS_RegisterCommands();

  /* Initialize CAN state publisher */
  FEB_CAN_State_Init();

  /* DWT microsecond clock — needed by the DRIVE shutdown-trip noise filter. */
  FEB_Time_Init();

  /* Initialize state machine (sets relays to safe state, transitions to LV_POWER) */
  FEB_SM_Init();

  /* Log after all subsystems initialized */
  LOG_I(TAG_MAIN, "BMS initialization complete");

  /* Startup banner. NOTE: FEB_Init() runs pre-scheduler (MX_FREERTOS_Init ->
   * before osKernelStart), where the DMA UART TX path blocks on a FreeRTOS
   * semaphore once the TX ring fills — which can never be serviced before the
   * scheduler runs, hanging the boot. Keep this pre-scheduler output small.
   * Bench-mode override warnings are emitted post-scheduler in StartUartRxTask. */
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("========================================\r\n");
  FEB_Console_Printf("        BMS Console Ready\r\n");
  FEB_Console_Printf("========================================\r\n");
  FEB_Console_Printf("Use | as delimiter: BMS|status\r\n");
  FEB_Console_Printf("Type 'help' for available commands\r\n");
  FEB_Console_Printf("\r\n");
}

/* Emit the bench-mode override warnings. Called once from a task AFTER the
 * scheduler is running — never from FEB_Init() (see banner note above). */
static void log_bench_overrides(void)
{
#if FEB_BMS_DISABLE_ADBMS_CHECKS
  LOG_W(TAG_MAIN, "ALL ADBMS CHECKS DISABLED (FEB_BMS_DISABLE_ADBMS_CHECKS=1)");
  LOG_W(TAG_MAIN, "Bench mode: voltage/temp enforcement AND the cell-monitor");
  LOG_W(TAG_MAIN, "data-timeout fault are BYPASSED. Do NOT run a real pack.");
  LOG_W(TAG_MAIN, "Pack voltage FORCED to %.1fV for bench precharge", (double)FEB_BMS_BENCH_PACK_VOLTAGE_V);
  LOG_W(TAG_MAIN, "Shutdown/AIR- backouts and contactor-feedback fault DISABLED");
#endif
#if FEB_BMS_DISABLE_TEMP_CHECKS
  LOG_W(TAG_MAIN, "TEMPERATURE ENFORCEMENT DISABLED (FEB_BMS_DISABLE_TEMP_CHECKS=1)");
  LOG_W(TAG_MAIN, "Bench mode: temp faults, charging temp limits, and balance");
  LOG_W(TAG_MAIN, "thermal gates are BYPASSED. Do NOT run a real pack.");
#endif
#if FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS
  LOG_W(TAG_MAIN, "PRIMARY (C-ADC) CELL-VOLTAGE ENFORCEMENT DISABLED "
                  "(FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS=1)");
  LOG_W(TAG_MAIN, "Charging pack/cell over-voltage limits are also BYPASSED.");
#endif
#if FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS
  LOG_W(TAG_MAIN, "SECONDARY (S-ADC) CELL-VOLTAGE ENFORCEMENT DISABLED "
                  "(FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS=1)");
#endif
#if FEB_BMS_DISABLE_PRIMARY_VOLT_CHECKS && FEB_BMS_DISABLE_SECONDARY_VOLT_CHECKS
  LOG_W(TAG_MAIN, "Bench mode: cell voltage faults will NEVER latch. Do NOT run a real pack.");
#endif
  (void)0; /* keep a statement when all blocks compile out */
}

/* ============================================================================
 * FreeRTOS Task Implementations - Override weak stubs in freertos.c
 * ============================================================================ */

void StartUartRxTask(void *argument)
{
  (void)argument;

  /* FEB_Init() is called from main() before kernel starts */

  /* Bench-mode warnings, emitted now that the scheduler is running (a full TX
   * ring here blocks-and-drains safely, unlike pre-scheduler in FEB_Init). */
  log_bench_overrides();

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

    /* PingPong + charger command tick every 100ms */
    pingpong_divider++;
    if (pingpong_divider >= 100)
    {
      pingpong_divider = 0;
      FEB_CAN_PingPong_Tick();
      /* Charger command TX + trickle logic (no-op unless in CHARGING) */
      FEB_CAN_Charger_Process();
    }
  }
}
