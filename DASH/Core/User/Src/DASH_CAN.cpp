/**
 ******************************************************************************
 * @file           : DASH_CAN.cpp
 * @brief          : DASH CAN bring-up and the RX / TX tasks
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DASH_CAN.h"
#include "DASH_PingPong.h"
#include "cmsis_os2.h"
#include "feb_can_subscriber.hpp"
#include "feb_console.h"
#include "feb_log.h"
#include "main.h"

extern CAN_HandleTypeDef hcan2;

extern osMessageQueueId_t canTxQueueHandle;
extern osMessageQueueId_t canRxQueueHandle;
extern osMutexId_t canTxMutexHandle;
extern osMutexId_t canRxMutexHandle;
extern osSemaphoreId_t canTxMailboxSemHandle;

namespace
{
volatile bool can_ready = false;
}

bool DASH_CAN_IsReady()
{
  return can_ready;
}

void DASH_CAN_Init()
{
  const FEB_CAN_Config_t cfg = {
      .hcan1 = nullptr,
      .hcan2 = &hcan2,
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
  FEB_CAN_PingPong_Init();

  can_ready = true;
}
