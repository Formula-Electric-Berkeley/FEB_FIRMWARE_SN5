/**
 ******************************************************************************
 * @file    FEB_CAN.c
 * @brief   BMS CAN application layer (FreeRTOS)
 * @author  Formula Electric @ Berkeley
 ******************************************************************************
 *
 * This file:
 *  - Initializes the FEB CAN library
 *  - Registers RX callbacks
 *  - Implements FreeRTOS RX/TX tasks
 *  - Owns all BMS CAN behavior
 *
 * CubeMX FreeRTOS tasks are expected to call:
 *   - StartBMSTaskRx()
 *   - StartBMSTaskTx()
 *
 ******************************************************************************
 */

#include "feb_can_lib.h"
#include "cmsis_os2.h"
#include "main.h"
#include "feb_log.h"
#include "FEB_CAN_State.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_CAN_DASH.h"
#include "FEB_CAN_IVT.h"
#include "FEB_CAN_Charger.h"
#include "FEB_CAN_Heartbeat.h"

/* ========================== External HAL handles ========================== */
extern CAN_HandleTypeDef hcan1;

/* ========================== External FreeRTOS handles (from CubeMX) ========================== */
/* NOTE: These are created in CubeMX .ioc and defined in freertos.c */
/* Names in .ioc are without "Handle" suffix; CubeMX adds it automatically */
extern osMessageQueueId_t canTxQueueHandle;
extern osMessageQueueId_t canRxQueueHandle;
extern osMutexId_t canTxMutexHandle;
extern osMutexId_t canRxMutexHandle;
extern osSemaphoreId_t canTxMailboxSemHandle;

/* ========================== Local Prototypes ========================== */
static void BMS_CAN_Init(void);

/* ========================== CAN Initialization ========================== */

static void BMS_CAN_Init(void)
{
  FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
      .hcan2 = NULL,
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
    /* CAN is critical for BMS operation */
    while (1)
    {
    }
  }

  /* RX registrations live in the per-module *_Init() functions called from
   * StartBMSTaskRx(). CAN1 has only 14 hardware filter banks (one per unique
   * registered ID/mask) — keep registrations consolidated (mask filters for
   * ranges) so dynamic ones (PingPong) always fit. */
}

/* ============================================================================
 * FreeRTOS Tasks (override CubeMX weak stubs)
 * ============================================================================ */

/**
 * @brief BMS CAN RX task
 */
void StartBMSTaskRx(void *argument)
{
  (void)argument;

  /* CAN init MUST occur after scheduler start */
  BMS_CAN_Init();

  /* Initialize pingpong module */
  FEB_CAN_PingPong_Init();

  /* Initialize DASH CAN reception (R2D signal) */
  FEB_CAN_DASH_Init();

  /* Initialize IVT CAN reception (voltage/current for precharge monitoring) */
  FEB_CAN_IVT_Init();

  /* Initialize charger CAN (extended-ID CCS protocol) */
  FEB_CAN_Charger_Init();

  /* Initialize heartbeat-presence tracking (BATTERY_FREE <-> LV_POWER) */
  FEB_CAN_Heartbeat_Init();

  /* Update filters after all registrations */
  FEB_CAN_Filter_UpdateFromRegistry(FEB_CAN_INSTANCE_1);

  /* Signal that CAN is ready for state publishing */
  FEB_CAN_State_SetReady();

  for (;;)
  {
    /* Dispatch RX queue and invoke callbacks */
    FEB_CAN_RX_Process();
    osDelay(1);
  }
}

/**
 * @brief BMS CAN TX task
 */
void StartBMSTaskTx(void *argument)
{
  (void)argument;

  for (;;)
  {
    /* Drain TX queue into CAN mailboxes */
    FEB_CAN_TX_Process();
    osDelay(1);
  }
}
