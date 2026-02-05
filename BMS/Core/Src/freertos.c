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
#include "i2c.h"
#include "TPS2482.h"
#include <stdio.h>
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
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for TPSTask */
osThreadId_t TPSTaskHandle;
const osThreadAttr_t TPSTask_attributes = {
  .name = "TPSTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for BMSTaskRx */
osThreadId_t BMSTaskRxHandle;
const osThreadAttr_t BMSTaskRx_attributes = {
  .name = "BMSTaskRx",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for BMSTaskTx */
osThreadId_t BMSTaskTxHandle;
const osThreadAttr_t BMSTaskTx_attributes = {
  .name = "BMSTaskTx",
  .stack_size = 128 * 4,
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
void StartTPSTask(void *argument);
void StartBMSTaskRx(void *argument);
void StartBMSTaskTx(void *argument);

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

  /* creation of TPSTask */
  TPSTaskHandle = osThreadNew(StartTPSTask, NULL, &TPSTask_attributes);

  /* creation of BMSTaskRx */
  BMSTaskRxHandle = osThreadNew(StartBMSTaskRx, NULL, &BMSTaskRx_attributes);

  /* creation of BMSTaskTx */
  BMSTaskTxHandle = osThreadNew(StartBMSTaskTx, NULL, &BMSTaskTx_attributes);

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
  for (;;) {
    
  }
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
    osDelay(pdMS_TO_TICKS(1));
  }
  /* USER CODE END StartADBMSTask */
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

/* USER CODE BEGIN Header_StartBMSTaskRx */
/**
* @brief Function implementing the BMSTaskRx thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartBMSTaskRx */
__weak void StartBMSTaskRx(void *argument)
{
  /* USER CODE BEGIN StartBMSTaskRx */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartBMSTaskRx */
}

/* USER CODE BEGIN Header_StartBMSTaskTx */
/**
* @brief Function implementing the BMSTaskTx thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartBMSTaskTx */
__weak void StartBMSTaskTx(void *argument)
{
  /* USER CODE BEGIN StartBMSTaskTx */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartBMSTaskTx */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

