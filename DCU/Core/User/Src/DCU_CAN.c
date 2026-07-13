/**
 ******************************************************************************
 * @file           : DCU_CAN.c
 * @brief          : CAN initialization for DCU with accept-all filter
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_CAN.h"
#include "can.h"
#include "cmsis_os.h"
#include "feb_can_lib.h"
#include "feb_log.h"
#include "feb_rtos_utils.h"
#include <stdbool.h>

/* FreeRTOS sync primitives created in freertos.c (see DCU.ioc FreeRTOS pane).
 * feb_can requires all five — it does not create them internally. */
extern osMessageQueueId_t canTxQueueHandle;
extern osMessageQueueId_t canRxQueueHandle;
extern osMutexId_t canTxMutexHandle;
extern osMutexId_t canRxMutexHandle;
extern osSemaphoreId_t canTxMailboxSemHandle;

static bool g_can_initialized = false;

bool DCU_CAN_Init(void)
{
  /* Reset flag in case of re-initialization */
  g_can_initialized = false;

  /* feb_can needs these to dispatch RX, queue TX, and flow-control mailboxes.
   * Fail fast if the .ioc and freertos.c got out of sync. */
  REQUIRE_RTOS_HANDLE(canTxQueueHandle);
  REQUIRE_RTOS_HANDLE(canRxQueueHandle);
  REQUIRE_RTOS_HANDLE(canTxMutexHandle);
  REQUIRE_RTOS_HANDLE(canRxMutexHandle);
  REQUIRE_RTOS_HANDLE(canTxMailboxSemHandle);

  /* Initialize FEB CAN library */
  FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
      .hcan2 = &hcan2,
      .get_tick_ms = HAL_GetTick,
      .tx_queue = canTxQueueHandle,
      .rx_queue = canRxQueueHandle,
      .tx_mutex = canTxMutexHandle,
      .rx_mutex = canRxMutexHandle,
      .tx_mailbox_sem = canTxMailboxSemHandle,
  };

  FEB_CAN_Status_t status = FEB_CAN_Init(&cfg);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "CAN init failed: %s", FEB_CAN_StatusToString(status));
    return false;
  }

  /* Configure accept-all filter on CAN1 FIFO0 (filter bank 0) */
  status = FEB_CAN_Filter_AcceptAll(FEB_CAN_INSTANCE_1, 0, FEB_CAN_FIFO_0);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "CAN1 filter failed: %s", FEB_CAN_StatusToString(status));
    return false;
  }

  /* Configure accept-all filter on CAN2 FIFO0 (filter bank 14) */
  status = FEB_CAN_Filter_AcceptAll(FEB_CAN_INSTANCE_2, 14, FEB_CAN_FIFO_0);
  if (status != FEB_CAN_OK)
  {
    LOG_W(TAG_CAN, "CAN2 filter failed: %s", FEB_CAN_StatusToString(status));
    /* Continue - CAN2 failure is not fatal */
  }

  g_can_initialized = true;
  LOG_I(TAG_CAN, "CAN initialized (accept-all mode)");
  return true;
}

bool DCU_CAN_IsInitialized(void)
{
  return g_can_initialized;
}

/* ============================================================================
 * canDispatchTask — drains feb_can's rx_queue and fires registered callbacks.
 *
 * In FreeRTOS mode the CAN RX ISR posts into feb_can's internal rx_queue and
 * returns immediately. Some task has to dequeue and invoke matching callbacks
 * (e.g. the wildcard handlers registered by DCU_CAN_Log). This is that task.
 *
 * No TX-side counterpart yet: DCU does not currently transmit CAN frames. If
 * that changes, mirror this pattern with `FEB_CAN_TX_Process()` after adding
 * the task in DCU.ioc.
 * ==========================================================================*/

void StartCanDispatchTask(void *argument)
{
  (void)argument;
  for (;;)
  {
    FEB_CAN_RX_Process();
    osDelay(1);
  }
}
