/**
 ******************************************************************************
 * @file           : SENSOR_CAN.cpp
 * @brief          : Sensor Node CAN bring-up
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "SENSOR_CAN.h"
#include "cmsis_os2.h"
#include "feb_can_lib.h"
#include "feb_can_subscriber.hpp" /* RxRegistry::attach_all() */
#include "feb_log.h"
#include "main.h"

extern CAN_HandleTypeDef hcan1;

extern osMessageQueueId_t canTxQueueHandle;
extern osMessageQueueId_t canRxQueueHandle;
extern osMutexId_t canTxMutexHandle;
extern osMutexId_t canRxMutexHandle;
extern osSemaphoreId_t canTxMailboxSemHandle;

namespace
{
volatile bool s_can_ready = false;
}

extern "C" bool SENSOR_CAN_IsReady(void)
{
  return s_can_ready;
}

extern "C" void SENSOR_CAN_Init(void)
{
  const FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
      .hcan2 = nullptr,
      .get_tick_ms = HAL_GetTick,
#if FEB_CAN_USE_FREERTOS
      .tx_queue = canTxQueueHandle,
      .rx_queue = canRxQueueHandle,
      .tx_mutex = canTxMutexHandle,
      .rx_mutex = canRxMutexHandle,
      .tx_mailbox_sem = canTxMailboxSemHandle,
#endif
  };

  if (FEB_CAN_Init(&cfg) != FEB_CAN_OK)
  {
    LOG_E("[CAN]", "init failed");
    for (;;)
    {
    }
  }

  feb::can::RxRegistry::attach_all();

  s_can_ready = true;
}
