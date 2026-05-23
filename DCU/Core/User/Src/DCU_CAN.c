/**
 ******************************************************************************
 * @file           : DCU_CAN.c
 * @brief          : CAN initialization for DCU with accept-all filter
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_CAN.h"
#include "can.h"
#include "feb_can_lib.h"
#include "feb_log.h"
#include <stdbool.h>
#include "DCU_CAN_Logging.h"

static bool g_can_initialized = false;

/* ========================== External FreeRTOS handles (from CubeMX) ========================== */
/* NOTE: These are created in CubeMX .ioc and defined in freertos.c */
/* Names in .ioc are without "Handle" suffix; CubeMX adds it automatically */
extern osMessageQueueId_t canTxQueueHandle;
extern osMessageQueueId_t canRxQueueHandle;
extern osMutexId_t canTxMutexHandle;
extern osMutexId_t canRxMutexHandle;
extern osSemaphoreId_t canTxMailboxSemHandle;

bool DCU_CAN_Init(void)
{
  /* Reset flag in case of re-initialization */
  g_can_initialized = false;

  /* Initialize FEB CAN library */
  FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
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
 * FreeRTOS Tasks (override CubeMX weak stubs)
 * ============================================================================ */

/**
 * @brief DASH CAN RX task
 */
void StartCanTaskRx(void *argument)
{
  (void)argument;

  /* CAN init MUST occur after scheduler start */
  DCU_CAN_Init();

  /* Signal that CAN is ready for state publishing */
  // FEB_CAN_State_SetReady();
  FEB_CAN_Logging_Init();

  for (;;)
  {
    /* Dispatch RX queue and invoke callbacks */
    // LOG_D("[CAN]", "Handling Rx");
    FEB_CAN_RX_Process();
    osDelay(1);
  }
}

/**
 * @brief DASH CAN TX task
 */
void StartCanTaskTx(void *argument)
{
  (void)argument;

  for (;;)
  {
    /* Drain TX queue into CAN mailboxes */
    FEB_CAN_TX_Process();
    osDelay(1);
  }
}
