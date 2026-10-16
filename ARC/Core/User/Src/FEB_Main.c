#include "FEB_Main.h"
#include "main.h"
#include "feb_can_lib.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

// TODO: Move this stuff to other files for conciseness
// FEB_Init should initialize CAN usage, TPS usage, Pressure Sensor Reading, FEET setup, etc
void FEB_Init(void)
{
  // ARC_CAN_Init();
}

void ARC_CAN_Init(void)
{
  // Initialize can config
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

  if (FEB_CAN_Init(&cfg) != FEB_CAN_OK)
  {
    LOG_E(TAG_MAIN, "%s", "CAN Initialization failed")
    while (1)
    {
    }
  }
}
