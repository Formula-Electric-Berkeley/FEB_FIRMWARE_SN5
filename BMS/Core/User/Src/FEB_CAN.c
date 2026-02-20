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
#include "feb_uart_log.h"
#include "FEB_BMS_CAN_State.h"
#include "FEB_CAN_PingPong.h"

/* ========================== External HAL handles ========================== */
extern CAN_HandleTypeDef hcan1;

/* ========================== Local Prototypes ========================== */
static void BMS_CAN_Init(void);
static void BMS_CAN_RxCallback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                               const uint8_t *data, uint8_t length, void *user_data);

/* ========================== RX Callback ========================== */

static void BMS_CAN_RxCallback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                               const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)data;
  (void)user_data;

  LOG_D(TAG_CAN, "RX: ID=0x%lX len=%d", can_id, length);
}

/* ========================== CAN Initialization ========================== */

static void BMS_CAN_Init(void)
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
    /* CAN is critical for BMS operation */
    while (1)
    {
    }
  }

  /* ---------------- RX Registration ---------------- */
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = 0x00,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = BMS_CAN_RxCallback,
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
 * @brief BMS CAN RX task
 */
void StartBMSTaskRx(void *argument)
{
  (void)argument;

  /* CAN init MUST occur after scheduler start */
  BMS_CAN_Init();

  /* Initialize pingpong module */
  FEB_CAN_PingPong_Init();

  /* Signal that CAN is ready for state publishing */
  FEB_BMS_CAN_State_SetReady();

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
