/**
 ******************************************************************************
 * @file           : LVPDB_Tasks.cpp
 * @brief          : Every FreeRTOS task body on LVPDB
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * These override the __weak stubs CubeMX emits in freertos.c, which means each
 * one MUST keep C linkage. Drop the extern "C" and the definition mangles, the
 * linker silently picks CubeMX's empty stub, and the task does nothing - with
 * no warning at build time. Verify with:
 *   arm-none-eabi-nm LVPDB.elf | grep -E " Start[A-Za-z]+$"
 *
 * Priorities and stack sizes are set in LVPDB.ioc, not here:
 *   canTxTask  AboveNormal  drains the TX queue
 *   tpsTask    Normal1      polls the TPS2482s
 *   canRxTask  Normal       CAN bring-up, then RX dispatch, rails and brake light
 *   canPubTask Normal       ticks the publisher scheduler and CAN ping/pong
 *   uartRxTask BelowNormal  console
 */

#include "LVPDB_CAN.h"
#include "LVPDB_Main.h"
#include "LVPDB_PingPong.h"
#include "LVPDB_TPS.h"
#include "cmsis_os2.h"
#include "feb_can_lib.h"
#include "feb_can_scheduler.hpp"
#include "feb_can_tasks.hpp"
#include "feb_console.h"
#include "feb_uart.h"

namespace fc = feb::can;

#define MAIN_LOOP_POLL_INTERVAL_MS 50

namespace
{
void wait_for_setup(void)
{
  while (!LVPDB_IsSetupComplete())
  {
    osDelay(5);
  }
}
} // namespace

extern "C"
{
  void StartUartRxTask(void *argument)
  {
    (void)argument;

    wait_for_setup();

    char line_buf[FEB_UART_QUEUE_LINE_SIZE];
    size_t line_len;

    for (;;)
    {
      FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);

      if (FEB_UART_QueueReceiveLine(FEB_UART_INSTANCE_1, line_buf, sizeof(line_buf), &line_len, 10))
      {
        FEB_Console_ProcessLine(line_buf, line_len);
      }
    }
  }

  void StartTpsTask(void *argument)
  {
    (void)argument;

    wait_for_setup();

    for (;;)
    {
      LVPDB_TPS_Poll();
      osDelay(MAIN_LOOP_POLL_INTERVAL_MS);
    }
  }

  void StartCanRxTask(void *argument)
  {
    (void)argument;

    wait_for_setup();

    LVPDB_CAN_Init();

    for (;;)
    {
      FEB_CAN_RX_Process();
      LVPDB_CAN_ApplyRxState();
      osDelay(1);
    }
  }

  void StartCanPubTask(void *argument)
  {
    (void)argument;

    wait_for_setup();

    fc::Scheduler::set_gate(&LVPDB_CAN_IsReady);
    fc::Scheduler::restart(osKernelGetTickCount());

    static uint16_t ping_divider = 0;

    for (;;)
    {
      fc::Scheduler::tick(osKernelGetTickCount());

      // Process CAN ping/pong every 100ms
      if (++ping_divider >= 100)
      {
        ping_divider = 0;
        FEB_CAN_PingPong_Tick();
      }

      osDelay(1);
    }
  }

  void StartCanTxTask(void *argument)
  {
    (void)argument;

    wait_for_setup();

    fc::RunTxTask();
  }
}
