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
#include "flash_benchmark.h"
#include "feb_uart_internal.h"
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
/* Definitions for flashTask */
osThreadId_t flashTaskHandle;
const osThreadAttr_t flashTask_attributes = {
  .name = "flashTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for uartRxTask */
osThreadId_t uartRxTaskHandle;
const osThreadAttr_t uartRxTask_attributes = {
  .name = "uartRxTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for uartRxQueue */
osMessageQueueId_t uartRxQueueHandle;
const osMessageQueueAttr_t uartRxQueue_attributes = {
  .name = "uartRxQueue"
};
/* Definitions for uartTxMutex */
osMutexId_t uartTxMutexHandle;
const osMutexAttr_t uartTxMutex_attributes = {
  .name = "uartTxMutex"
};
/* Definitions for uartTxSem */
osSemaphoreId_t uartTxSemHandle;
const osSemaphoreAttr_t uartTxSem_attributes = {
  .name = "uartTxSem"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartFlashTask(void *argument);
void StartUARTRxTask(void *argument);

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
  /* creation of uartTxMutex */
  uartTxMutexHandle = osMutexNew(&uartTxMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of uartTxSem */
  uartTxSemHandle = osSemaphoreNew(1, 0, &uartTxSem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of uartRxQueue */
  uartRxQueueHandle = osMessageQueueNew (8, sizeof(FEB_UART_RxQueueMsg_t), &uartRxQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of flashTask */
  flashTaskHandle = osThreadNew(StartFlashTask, NULL, &flashTask_attributes);

  /* creation of uartRxTask */
  uartRxTaskHandle = osThreadNew(StartUARTRxTask, NULL, &uartRxTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartFlashTask */
/**
  * @brief  Function implementing the flashTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartFlashTask */
__weak void StartFlashTask(void *argument)
{
  /* USER CODE BEGIN StartFlashTask */
  FlashBench_TaskEntry(argument);
  /* USER CODE END StartFlashTask */
}

/* USER CODE BEGIN Header_StartUARTRxTask */
/**
* @brief Function implementing the uartRxTask03 thread.
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

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

