/**
 ******************************************************************************
 * @file    FEB_CAN.c
 * @brief   DASH CAN application layer (FreeRTOS)
 * @author  Formula Electric @ Berkeley
 ******************************************************************************
 *
 * This file:
 *  - Initializes the FEB CAN library
 *  - Registers RX callbacks
 *  - Implements FreeRTOS RX/TX tasks
 *  - Owns all DASH CAN behavior
 *
 * CubeMX FreeRTOS tasks are expected to call:
 *   - StartDASHTaskRx()
 *   - StartDASHTaskTx()
 *
 ******************************************************************************
 */

#include "feb_can_lib.h"
#include "cmsis_os2.h"
#include "feb_console.h"
#include "main.h"
#include "feb_log.h"
#include "FEB_CAN_State.h"
#include "FEB_CAN_PingPong.h"
#include "feb_can.h"
#include "feb_can_lib.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_LVPDB.h"
#include "FEB_CAN_PCU.h"

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
static void DASH_CAN_Init(void);
static void DASH_CAN_RxCallback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                const uint8_t *data, uint8_t length, void *user_data);

/* ========================== RX Callback ========================== */

static void DASH_CAN_RxCallback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)data;
  (void)user_data;

  FEB_Console_Printf("[CAN] RX: ID=0x%lX len=%d", can_id, length);
}

/* ========================== CAN Initialization ========================== */

static void DASH_CAN_Init(void)
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
    /* CAN initialization failed - halt in debug */
    while (1)
    {
    }
  }

  /* ---------------- RX Registration (wildcard to receive all) ---------------- */
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = 0x00,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = DASH_CAN_RxCallback,
      .user_data = NULL,
  };

  FEB_CAN_RX_Register(&rx_params);
}

/* ============================================================================
 * FreeRTOS Tasks (override CubeMX weak stubs)
 * ============================================================================ */

/**
 * @brief DASH CAN RX task
 */
void StartDASHTaskRx(void *argument)
{
  (void)argument;

  FEB_Console_Printf("gurt0.5");

  /* CAN init MUST occur after scheduler start */
  DASH_CAN_Init();

  FEB_CAN_PingPong_Init();
  
  FEB_Console_Printf("gurt: yo");
  FEB_CAN_BMS_Init();
  FEB_CAN_PCU_Init();
  FEB_CAN_LVPDB_Init();

/* Ensure filters reflect registry */
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
 * @brief DASH CAN TX task
 */
void StartDASHTaskTx(void *argument)
{
  (void)argument;

  // /* Hardcoded */
  // FEB_CAN_TX_Params_t tx_params = {
  //     .instance = FEB_CAN_INSTANCE_1,
  //     .can_id = FEB_CAN_FEB_PING_PONG_COUNTER1_FRAME_ID, // FEB_CAN_FEB_PING_PONG_COUNTER1_FRAME_ID (0xe0u)
  //     .id_type = FEB_CAN_ID_STD,
  // };

  // uint8_t tx_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

  for (;;)
  {
    // FEB_CAN_TX_Send(tx_params.instance, tx_params.can_id, tx_params.id_type, tx_data, 8);
    FEB_CAN_TX_Process();
    osDelay(100);
  }
}
