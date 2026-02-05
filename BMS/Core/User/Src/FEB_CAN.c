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

#include "feb_can.h"
#include "cmsis_os2.h"
#include "main.h"

#define FEB_UART_USE_FREERTOS 1

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
  (void)can_id;
  (void)id_type;
  (void)data;
  (void)length;
  (void)user_data;

  printf("hello can\r\n");
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
