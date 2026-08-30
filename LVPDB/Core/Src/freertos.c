/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "FEB_Main.h"
#include "feb_uart.h"
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
/* Definitions for UartConsoleLog */
osThreadId_t UartConsoleLogHandle;
const osThreadAttr_t UartConsoleLog_attributes = {
  .name = "UartConsoleLog",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for TPSPowerManage */
osThreadId_t TPSPowerManageHandle;
const osThreadAttr_t TPSPowerManage_attributes = {
  .name = "TPSPowerManage",
  .stack_size = 192 * 4,
  .priority = (osPriority_t) osPriorityNormal1,
};
/* Definitions for LVPDBTaskRx */
osThreadId_t LVPDBTaskRxHandle;
const osThreadAttr_t LVPDBTaskRx_attributes = {
  .name = "LVPDBTaskRx",
  .stack_size = 160 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LVPDBTaskTx */
osThreadId_t LVPDBTaskTxHandle;
const osThreadAttr_t LVPDBTaskTx_attributes = {
  .name = "LVPDBTaskTx",
  .stack_size = 192 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
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
/* Definitions for FEB_I2C_mutex */
osMutexId_t FEB_I2C_mutexHandle;
const osMutexAttr_t FEB_I2C_mutex_attributes = {
  .name = "FEB_I2C_mutex"
};
/* Definitions for tpsDataMutex */
osMutexId_t tpsDataMutexHandle;
const osMutexAttr_t tpsDataMutex_attributes = {
  .name = "tpsDataMutex"
};
/* Definitions for uartTxSem */
osSemaphoreId_t uartTxSemHandle;
const osSemaphoreAttr_t uartTxSem_attributes = {
  .name = "uartTxSem"
};
/* Definitions for canTxMailboxSem */
osSemaphoreId_t canTxMailboxSemHandle;
const osSemaphoreAttr_t canTxMailboxSem_attributes = {
  .name = "canTxMailboxSem"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartUartConsoleLog(void *argument);
void StartTPSPowerManage(void *argument);
void StartLVPDBTaskRx(void *argument);
void StartLVPDBTaskTx(void *argument);

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
  /* creation of logMutex */
  logMutexHandle = osMutexNew(&logMutex_attributes);

  /* creation of uartTxMutex */
  uartTxMutexHandle = osMutexNew(&uartTxMutex_attributes);

  /* creation of canTxMutex */
  canTxMutexHandle = osMutexNew(&canTxMutex_attributes);

  /* creation of canRxMutex */
  canRxMutexHandle = osMutexNew(&canRxMutex_attributes);

  /* creation of FEB_I2C_mutex */
  FEB_I2C_mutexHandle = osMutexNew(&FEB_I2C_mutex_attributes);

  /* creation of tpsDataMutex */
  tpsDataMutexHandle = osMutexNew(&tpsDataMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of uartTxSem */
  uartTxSemHandle = osSemaphoreNew(1, 1, &uartTxSem_attributes);

  /* creation of canTxMailboxSem */
  canTxMailboxSemHandle = osSemaphoreNew(3, 3, &canTxMailboxSem_attributes);

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
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of UartConsoleLog */
  UartConsoleLogHandle = osThreadNew(StartUartConsoleLog, NULL, &UartConsoleLog_attributes);

  /* creation of TPSPowerManage */
  TPSPowerManageHandle = osThreadNew(StartTPSPowerManage, NULL, &TPSPowerManage_attributes);

  /* creation of LVPDBTaskRx */
  LVPDBTaskRxHandle = osThreadNew(StartLVPDBTaskRx, NULL, &LVPDBTaskRx_attributes);

  /* creation of LVPDBTaskTx */
  LVPDBTaskTxHandle = osThreadNew(StartLVPDBTaskTx, NULL, &LVPDBTaskTx_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  FEB_Main_Setup();
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartUartConsoleLog */
/**
* @brief Function implementing the UartConsoleLog thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUartConsoleLog */
__weak void StartUartConsoleLog(void *argument)
{
  /* USER CODE BEGIN StartUartConsoleLog */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartUartConsoleLog */
}

/* USER CODE BEGIN Header_StartTPSPowerManage */
/**
* @brief Function implementing the TPSPowerManage thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTPSPowerManage */
__weak void StartTPSPowerManage(void *argument)
{
  /* USER CODE BEGIN StartTPSPowerManage */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTPSPowerManage */
}

/* USER CODE BEGIN Header_StartLVPDBTaskRx */
/**
* @brief Function implementing the LVPDBTaskRx thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartLVPDBTaskRx */
__weak void StartLVPDBTaskRx(void *argument)
{
  /* USER CODE BEGIN StartLVPDBTaskRx */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartLVPDBTaskRx */
}

/* USER CODE BEGIN Header_StartLVPDBTaskTx */
/**
* @brief Function implementing the LVPDBTaskTx thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartLVPDBTaskTx */
__weak void StartLVPDBTaskTx(void *argument)
{
  /* USER CODE BEGIN StartLVPDBTaskTx */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartLVPDBTaskTx */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

