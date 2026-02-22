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
#include "main.h"
#include "feb_uart_log.h"
#include "FEB_CAN_State.h"
#include "FEB_CAN_PingPong.h"

/* ========================== External HAL handles ========================== */
extern CAN_HandleTypeDef hcan1;

/* ========================== Module tag for logging ========================== */
#define TAG_CAN "[CAN]"

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

  LOG_D(TAG_CAN, "RX: ID=0x%lX len=%d", can_id, length);
}

/* ========================== CAN Initialization ========================== */

static void DASH_CAN_Init(void)
{
  FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
      .hcan2 = NULL,
      .tx_queue_size = 16,
      .rx_queue_size = 32,
      .get_tick_ms = HAL_GetTick,
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

  /* Ensure filters reflect registry */
  FEB_CAN_Filter_UpdateFromRegistry(FEB_CAN_INSTANCE_1);
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

  /* CAN init MUST occur after scheduler start */
  DASH_CAN_Init();
  FEB_CAN_PingPong_Init();

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

  for (;;)
  {
    /* Drain TX queue into CAN mailboxes */
    FEB_CAN_TX_Process();
    osDelay(1);
  }
}
