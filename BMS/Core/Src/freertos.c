/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bms_tasks.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for ADBMSTask */
osThreadId_t ADBMSTaskHandle;
const osThreadAttr_t ADBMSTask_attributes = {
  .name = "ADBMSTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for CANTxTask */
osThreadId_t CANTxTaskHandle;
const osThreadAttr_t CANTxTask_attributes = {
  .name = "CANTxTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for CANRxTask */
osThreadId_t CANRxTaskHandle;
const osThreadAttr_t CANRxTask_attributes = {
  .name = "CANRxTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal1,
};
/* Definitions for UARTTxTask */
osThreadId_t UARTTxTaskHandle;
const osThreadAttr_t UARTTxTask_attributes = {
  .name = "UARTTxTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for UARTRxTask */
osThreadId_t UARTRxTaskHandle;
const osThreadAttr_t UARTRxTask_attributes = {
  .name = "UARTRxTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow1,
};
/* Definitions for SMTask */
osThreadId_t SMTaskHandle;
const osThreadAttr_t SMTask_attributes = {
  .name = "SMTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for heartBeatTask */
osThreadId_t heartBeatTaskHandle;
const osThreadAttr_t heartBeatTask_attributes = {
  .name = "heartBeatTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for TPSTask */
osThreadId_t TPSTaskHandle;
const osThreadAttr_t TPSTask_attributes = {
  .name = "TPSTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for ADBMSMutex */
osMutexId_t ADBMSMutexHandle;
const osMutexAttr_t ADBMSMutex_attributes = {
  .name = "ADBMSMutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartADBMSTask(void *argument);
void StartCANTxTask(void *argument);
void StartCANRxTask(void *argument);
void StartUARTTxTask(void *argument);
void StartUARTRxTask(void *argument);
void StartSMTask(void *argument);
void StartHeartBeatTask(void *argument);
void StartTPSTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of ADBMSMutex */
  ADBMSMutexHandle = osMutexNew(&ADBMSMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of ADBMSTask */
  ADBMSTaskHandle = osThreadNew(StartADBMSTask, NULL, &ADBMSTask_attributes);

  /* creation of CANTxTask */
  CANTxTaskHandle = osThreadNew(StartCANTxTask, NULL, &CANTxTask_attributes);

  /* creation of CANRxTask */
  CANRxTaskHandle = osThreadNew(StartCANRxTask, NULL, &CANRxTask_attributes);

  /* creation of UARTTxTask */
  UARTTxTaskHandle = osThreadNew(StartUARTTxTask, NULL, &UARTTxTask_attributes);

  /* creation of UARTRxTask */
  UARTRxTaskHandle = osThreadNew(StartUARTRxTask, NULL, &UARTRxTask_attributes);

  /* creation of SMTask */
  SMTaskHandle = osThreadNew(StartSMTask, NULL, &SMTask_attributes);

  /* creation of heartBeatTask */
  heartBeatTaskHandle = osThreadNew(StartHeartBeatTask, NULL, &heartBeatTask_attributes);

  /* creation of TPSTask */
  TPSTaskHandle = osThreadNew(StartTPSTask, NULL, &TPSTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */

  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */

  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartADBMSTask */
/**
* @brief Function implementing the ADBMSTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartADBMSTask */
__weak void StartADBMSTask(void *argument)
{
  /* USER CODE BEGIN StartADBMSTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartADBMSTask */
}

/* USER CODE BEGIN Header_StartCANTxTask */
/**
* @brief Function implementing the CANTxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCANTxTask */
__weak void StartCANTxTask(void *argument)
{
  /* USER CODE BEGIN StartCANTxTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCANTxTask */
}

/* USER CODE BEGIN Header_StartCANRxTask */
/**
* @brief Function implementing the CANRxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCANRxTask */
__weak void StartCANRxTask(void *argument)
{
  /* USER CODE BEGIN StartCANRxTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCANRxTask */
}

/* USER CODE BEGIN Header_StartUARTTxTask */
/**
* @brief Function implementing the UARTTxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUARTTxTask */
__weak void StartUARTTxTask(void *argument)
{
  /* USER CODE BEGIN StartUARTTxTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartUARTTxTask */
}

/* USER CODE BEGIN Header_StartUARTRxTask */
/**
* @brief Function implementing the UARTRxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUARTRxTask */
__weak void StartUARTRxTask(void *argument)
{
  /* USER CODE BEGIN StartUARTRxTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartUARTRxTask */
}

/* USER CODE BEGIN Header_StartSMTask */
/**
* @brief Function implementing the SMTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSMTask */
__weak void StartSMTask(void *argument)
{
  /* USER CODE BEGIN StartSMTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartSMTask */
}

/* USER CODE BEGIN Header_StartHeartBeatTask */
/**
* @brief Function implementing the heartBeatTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartHeartBeatTask */
__weak void StartHeartBeatTask(void *argument)
{
  /* USER CODE BEGIN StartHeartBeatTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartHeartBeatTask */
}

/* USER CODE BEGIN Header_StartTPSTask */
/**
* @brief Function implementing the TPSTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTPSTask */
__weak void StartTPSTask(void *argument)
{
  /* USER CODE BEGIN StartTPSTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTPSTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

