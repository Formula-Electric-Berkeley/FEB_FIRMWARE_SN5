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
#include "FEB_Task_ADBMS.h"
#include "FEB_Task_TPS.h"
#include "FEB_Main.h"
#include "i2c.h"
#include "feb_uart.h"
#include "feb_uart_internal.h"
#include "feb_console.h"
#include "feb_can_lib.h"
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
/* Definitions for uartRxTask */
osThreadId_t uartRxTaskHandle;
const osThreadAttr_t uartRxTask_attributes = {
  .name = "uartRxTask",
  .stack_size = 512 * 4,
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
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for BMSTaskTx */
osThreadId_t BMSTaskTxHandle;
const osThreadAttr_t BMSTaskTx_attributes = {
  .name = "BMSTaskTx",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for SMTask */
osThreadId_t SMTaskHandle;
const osThreadAttr_t SMTask_attributes = {
  .name = "SMTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for canTxQueue */
osMessageQueueId_t canTxQueueHandle;
const osMessageQueueAttr_t canTxQueue_attributes = {
  .name = "canTxQueue"
};
/* Definitions for canRxQueue */
osMessageQueueId_t canRxQueueHandle;
const osMessageQueueAttr_t canRxQueue_attributes = {
  .name = "canRxQueue"
};
/* Definitions for uartRxQueue */
osMessageQueueId_t uartRxQueueHandle;
const osMessageQueueAttr_t uartRxQueue_attributes = {
  .name = "uartRxQueue"
};
/* Definitions for ADBMSMutex */
osMutexId_t ADBMSMutexHandle;
const osMutexAttr_t ADBMSMutex_attributes = {
  .name = "ADBMSMutex"
};
/* Definitions for canTxMutex */
osMutexId_t canTxMutexHandle;
const osMutexAttr_t canTxMutex_attributes = {
  .name = "canTxMutex"
};
/* Definitions for canRxMutex */
osMutexId_t canRxMutexHandle;
const osMutexAttr_t canRxMutex_attributes = {
  .name = "canRxMutex"
};
/* Definitions for tpsDataMutex */
osMutexId_t tpsDataMutexHandle;
const osMutexAttr_t tpsDataMutex_attributes = {
  .name = "tpsDataMutex"
};
/* Definitions for tpsI2cMutex */
osMutexId_t tpsI2cMutexHandle;
const osMutexAttr_t tpsI2cMutex_attributes = {
  .name = "tpsI2cMutex"
};
/* Definitions for logMutex */
osMutexId_t logMutexHandle;
const osMutexAttr_t logMutex_attributes = {
  .name = "logMutex"
};
/* Definitions for uartTxMutex */
osMutexId_t uartTxMutexHandle;
const osMutexAttr_t uartTxMutex_attributes = {
  .name = "uartTxMutex"
};
/* Definitions for canTxMailboxSem */
osSemaphoreId_t canTxMailboxSemHandle;
const osSemaphoreAttr_t canTxMailboxSem_attributes = {
  .name = "canTxMailboxSem"
};
/* Definitions for uartTxSem */
osSemaphoreId_t uartTxSemHandle;
const osSemaphoreAttr_t uartTxSem_attributes = {
  .name = "uartTxSem"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartUartRxTask(void *argument);
void StartADBMSTask(void *argument);
void StartTPSTask(void *argument);
void StartBMSTaskRx(void *argument);
void StartBMSTaskTx(void *argument);
void StartSMTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

  /* creation of canTxMutex */
  canTxMutexHandle = osMutexNew(&canTxMutex_attributes);

  /* creation of canRxMutex */
  canRxMutexHandle = osMutexNew(&canRxMutex_attributes);

  /* creation of tpsDataMutex */
  tpsDataMutexHandle = osMutexNew(&tpsDataMutex_attributes);

  /* creation of tpsI2cMutex */
  tpsI2cMutexHandle = osMutexNew(&tpsI2cMutex_attributes);

  /* creation of logMutex */
  logMutexHandle = osMutexNew(&logMutex_attributes);

  /* creation of uartTxMutex */
  uartTxMutexHandle = osMutexNew(&uartTxMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of canTxMailboxSem */
  canTxMailboxSemHandle = osSemaphoreNew(3, 3, &canTxMailboxSem_attributes);

  /* creation of uartTxSem */
  uartTxSemHandle = osSemaphoreNew(1, 0, &uartTxSem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of canTxQueue */
  canTxQueueHandle = osMessageQueueNew (16, sizeof(FEB_CAN_Message_t), &canTxQueue_attributes);

  /* creation of canRxQueue */
  canRxQueueHandle = osMessageQueueNew (32, sizeof(FEB_CAN_Message_t), &canRxQueue_attributes);

  /* creation of uartRxQueue */
  uartRxQueueHandle = osMessageQueueNew (8, sizeof(FEB_UART_RxQueueMsg_t), &uartRxQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of uartRxTask */
  uartRxTaskHandle = osThreadNew(StartUartRxTask, NULL, &uartRxTask_attributes);

  /* creation of ADBMSTask */
  ADBMSTaskHandle = osThreadNew(StartADBMSTask, NULL, &ADBMSTask_attributes);

  /* creation of TPSTask */
  TPSTaskHandle = osThreadNew(StartTPSTask, NULL, &TPSTask_attributes);

  /* creation of BMSTaskRx */
  BMSTaskRxHandle = osThreadNew(StartBMSTaskRx, NULL, &BMSTaskRx_attributes);

  /* creation of BMSTaskTx */
  BMSTaskTxHandle = osThreadNew(StartBMSTaskTx, NULL, &BMSTaskTx_attributes);

  /* creation of SMTask */
  SMTaskHandle = osThreadNew(StartSMTask, NULL, &SMTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */

  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartUartRxTask */
/**
* @brief Function implementing the uartRxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUartRxTask */
__weak void StartUartRxTask(void *argument)
{
  /* USER CODE BEGIN StartUartRxTask */
  (void)argument;
  /* Weak stub - override in FEB_Main.c with queue-based implementation */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartUartRxTask */
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

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* Stack overflow hook - called when stack overflow is detected */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
  (void)xTask;
  printf("STACK OVERFLOW: %s\r\n", (char *)pcTaskName);
  /* Halt for debugging - allows inspection with debugger */
  __disable_irq();
  while(1);
}

/* USER CODE END Application */

