/**
 ******************************************************************************
 * @file           : DASH_Tasks.cpp
 * @brief          : Every FreeRTOS task body on DASH
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "PCU_CAN.h"
#include "PCU_Commands.h"

#include "cmsis_os2.h"
#include "feb_can_tasks.hpp"
#include "feb_uart.h"

namespace fc = feb::can;

extern "C"
{
  void StartPCUTask(void *argument)
  {
    (void)argument;

    for (;;)
    {
      uint32_t now = HAL_GetTick(); // gets the time in ms since boot
      if ((now - last_sample_ms) < PCU_SAMPLE_PERIOD_MS)
      {
        return;
      }
      last_sample_ms = now;

      bool apps_plausible = validate_torque(apps1, apps2) && apps1.in_range() && apps2.in_range();
      if (apps_plausible)
      {
        last_plausible_ms = now;
      }
      apps_fault = (now - last_plausible_ms) > APPS_IMPLAUSIBILITY_TIME_MS;

      torque_command = 0;
      if (PCU_CAN_IsR2D() && !apps_fault)
      {
        torque_command = apps1.get_torque();
      }

      osDelay(1);
    }
  }

  void StartUartRxTask(void *argument)
  {
    (void)argument;

    char line_buf[FEB_UART_QUEUE_LINE_SIZE];
    size_t line_len;

    for (;;)
    {
      FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);

      if (FEB_UART_QueueReceiveLine(FEB_UART_INSTANCE_1, line_buf, sizeof(line_buf), &line_len, 10))
      {
        DASH_Console_ProcessLine(line_buf, line_len);
      }
    }
  }

  void StartCanRxTask(void *argument)
  {
    (void)argument;
    fc::RunRxTask(&DASH_CAN_Init);
  }

  void StartCanPubTask(void *argument)
  {
    (void)argument;
    fc::RunPubTask(&DASH_CAN_IsReady);
  }

  void StartCanTxTask(void *argument)
  {
    (void)argument;
    fc::RunTxTask();
  }
}
