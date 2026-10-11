#include "FEB_Main.h"
#include "main.h"
#include "feb_can_lib.h"

// TODO: Move this stuff to other files for conciseness
// FEB_Init should initialize CAN usage, TPS usage, Pressure Sensor Reading, FEET setup, etc
void FEB_Init(void)
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
    // Handle init failure
    while (1)
    {
    }
  }
}

// TODO: Define Task starting functions here
void START_K3_CONTROL(void const *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_START_FEET_CONTROL */
/**
 * @brief Function implementing the FEET_CONTROL thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_START_FEET_CONTROL */
void START_FEET_CONTROL(void const *argument)
{
  /* USER CODE BEGIN START_FEET_CONTROL */
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END START_FEET_CONTROL */
}

/* USER CODE BEGIN Header_START_P_SENSE */
/**
 * @brief Function implementing the P_SENSE thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_START_P_SENSE */
void START_P_SENSE(void const *argument)
{
  /* USER CODE BEGIN START_P_SENSE */
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END START_P_SENSE */
}

/* USER CODE BEGIN Header_START_STATE_READ */
/**
 * @brief Function implementing the STATE_READ thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_START_STATE_READ */
void START_STATE_READ(void const *argument)
{
  /* USER CODE BEGIN START_STATE_READ */
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END START_STATE_READ */
}