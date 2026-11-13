/**
 ******************************************************************************
 * @file           : feb_can_tasks.hpp
 * @brief          : Shared FreeRTOS task bodies for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_TASKS_HPP
#define FEB_CAN_TASKS_HPP

#include "cmsis_os2.h"
#include "feb_board_config.hpp"
#include "feb_can_lib.h"
#include "feb_can_scheduler.hpp"

#include <cstdint>

namespace feb::can
{
[[noreturn]] inline void RunTxTask()
{
  for (;;)
  {
    FEB_CAN_TX_Process();
    osDelay(1);
  }
}

[[noreturn]] inline void RunRxTask(void (*bring_up)())
{
  if (bring_up != nullptr)
  {
    bring_up();
  }
  for (;;)
  {
    FEB_CAN_RX_Process();
    osDelay(1);
  }
}

[[noreturn]] inline void RunPubTask(bool (*ready)(), std::uint32_t tick_ms = 1)
{
  Scheduler::set_gate(ready);
  Scheduler::restart(osKernelGetTickCount());

  std::uint32_t wake = osKernelGetTickCount();
  for (;;)
  {
    Scheduler::tick(osKernelGetTickCount());

    wake += tick_ms;
    const std::uint32_t now = osKernelGetTickCount();
    if ((std::int32_t)(wake - now) <= 0)
    {
      wake = now + tick_ms;
    }
    osDelayUntil(wake);
  }
}

}  // namespace feb::can

#endif /* FEB_CAN_TASKS_HPP */
