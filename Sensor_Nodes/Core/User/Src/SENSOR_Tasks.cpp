/**
 ******************************************************************************
 * @file           : SENSOR_Tasks.cpp
 * @brief          : FreeRTOS task bodies for the Sensor Node
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "SENSOR_CAN.h"
#include "SENSOR_Commands.h"
#include "SENSOR_Tasks.h"
#include "FEB_Main.h"
#include "cmsis_os2.h"
#include "feb_can_tasks.hpp"
#include "feb_log.h"
#include "feb_uart.h"

#define TAG_SENSOR "[SENSOR]"

namespace fc = feb::can;

extern "C"
{
  void StartSensorTask(void *argument)
  {
    (void)argument;
    LOG_I(TAG_SENSOR, "task up");

    uint32_t next_tick = osKernelGetTickCount();
    for (;;)
    {
      FEB_Main_Loop();
      next_tick += 1;
      osDelayUntil(next_tick);
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
        SENSOR_Console_ProcessLine(line_buf, line_len);
      }
    }
  }

  void StartCanRxTask(void *argument)
  {
    (void)argument;
    fc::RunRxTask(&SENSOR_CAN_Init);
  }

  void StartCanPubTask(void *argument)
  {
    (void)argument;
    fc::RunPubTask(&SENSOR_CAN_IsReady);
  }

  void StartCanTxTask(void *argument)
  {
    (void)argument;
    fc::RunTxTask();
  }
}
