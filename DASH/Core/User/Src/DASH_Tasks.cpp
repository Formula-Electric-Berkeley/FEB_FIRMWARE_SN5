/**
 ******************************************************************************
 * @file           : DASH_Tasks.cpp
 * @brief          : Every FreeRTOS task body on DASH
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DASH_CAN.h"
#include "DASH_Commands.h"
#include "DASH_IO.h"
#include "DASH_RTD.h"
#include "DASH_UI.h"
#include "cmsis_os2.h"
#include "feb_can_tasks.hpp"
#include "feb_uart.h"

namespace fc = feb::can;

extern "C"
{
  void StartIoTask(void *argument)
  {
    (void)argument;

    for (;;)
    {
      FEB_IO_Update_GPIO();
      FEB_State_Update_RTD();
      osDelay(1);
    }
  }

  void StartDisplayTask(void *argument)
  {
    (void)argument;

    FEB_UI_Init();

    for (;;)
    {
      FEB_UI_Update();
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
