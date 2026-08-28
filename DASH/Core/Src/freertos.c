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
#include "DASH_Main.h"
#include "feb_uart.h"
#include "feb_console.h"
#include "feb_can_lib.h"
#include "feb_rtos_utils.h"
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
/* DASHTaskRx / DASHTaskTx stacks: set in CubeMX (DASH.ioc, FREERTOS.Tasks01). Regenerating
 * from the .ioc overwrites osThreadAttr_t blocks above; keep those tasks at 1024 words. */
/* USER CODE END Variables */
/* Definitions for ioTask */
osThreadId_t ioTaskHandle;
const osThreadAttr_t ioTask_attributes = {
  .name = "ioTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh1,
};
/* Definitions for displayTask */
osThreadId_t displayTaskHandle;
const osThreadAttr_t displayTask_attributes = {
  .name = "displayTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for uartRxTask */
osThreadId_t uartRxTaskHandle;
const osThreadAttr_t uartRxTask_attributes = {
  .name = "uartRxTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal1,
};
/* Definitions for uartTxTask */
osThreadId_t uartTxTaskHandle;
const osThreadAttr_t uartTxTask_attributes = {
  .name = "uartTxTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for canRxTask */
osThreadId_t canRxTaskHandle;
const osThreadAttr_t canRxTask_attributes = {
  .name = "canRxTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh2,
};
/* Definitions for canTxTask */
osThreadId_t canTxTaskHandle;
const osThreadAttr_t canTxTask_attributes = {
  .name = "canTxTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh3,
};
/* Definitions for canPubTask */
osThreadId_t canPubTaskHandle;
const osThreadAttr_t canPubTask_attributes = {
  .name = "canPubTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh1,
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
/* Definitions for i2cMutex */
osMutexId_t i2cMutexHandle;
const osMutexAttr_t i2cMutex_attributes = {
  .name = "i2cMutex"
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

void StartIoTask(void *argument);
void StartDisplayTask(void *argument);
void StartUartRxTask(void *argument);
void StartUartTxTask(void *argument);
void StartCanRxTask(void *argument);
void StartCanTxTask(void *argument);
void StartCanPubTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 2 */
__weak void vApplicationIdleHook(void)
{
  /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
  to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
  task. It is essential that code added to this hook function never attempts
  to block in any way (for example, call xQueueReceive() with a block time
  specified, or call vTaskDelay()). If the application makes use of the
  vTaskDelete() API function (as this demo application does) then it is also
  important that vApplicationIdleHook() is permitted to return to its calling
  function, because it is the responsibility of the idle task to clean up
  memory allocated by the kernel to any task that has since been deleted. */
}
/* USER CODE END 2 */

/* USER CODE BEGIN 4 */
extern UART_HandleTypeDef huart3;

void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
  /* Polled-UART panic notice. Runs with the offending task's stack already
   * trashed, so do NOT touch any FreeRTOS APIs or rely on FEB_UART/FEB_Log
   * (their state may be corrupt). HAL_UART_Transmit pokes the peripheral
   * directly which is the most we can safely do here. */
  (void)xTask;
  char buf[96];
  int n = snprintf(buf, sizeof(buf), "\r\n!!! STACK OVERFLOW in task: %s !!!\r\n",
                   pcTaskName ? (const char *)pcTaskName : "<unknown>");
  if (n > 0)
  {
    HAL_UART_Transmit(&huart3, (uint8_t *)buf, (uint16_t)n, 100);
  }
  __asm volatile("bkpt #0");
  for (;;)
  {
  }
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
  static const char msg[] = "\r\n!!! pvPortMalloc FAILED — FreeRTOS heap exhausted !!!\r\n";
  HAL_UART_Transmit(&huart3, (uint8_t *)msg, sizeof(msg) - 1, 100);
  __asm volatile("bkpt #0");
  for (;;)
  {
  }
}
/* USER CODE END 5 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of i2cMutex */
  i2cMutexHandle = osMutexNew(&i2cMutex_attributes);

  /* creation of canTxMutex */
  canTxMutexHandle = osMutexNew(&canTxMutex_attributes);

  /* creation of canRxMutex */
  canRxMutexHandle = osMutexNew(&canRxMutex_attributes);

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
  /* Validate all RTOS objects - fail fast on low heap */
  REQUIRE_RTOS_HANDLE(i2cMutexHandle);
  REQUIRE_RTOS_HANDLE(canTxMutexHandle);
  REQUIRE_RTOS_HANDLE(canRxMutexHandle);
  REQUIRE_RTOS_HANDLE(logMutexHandle);
  REQUIRE_RTOS_HANDLE(uartTxMutexHandle);
  REQUIRE_RTOS_HANDLE(canTxMailboxSemHandle);
  REQUIRE_RTOS_HANDLE(uartTxSemHandle);
  REQUIRE_RTOS_HANDLE(canTxQueueHandle);
  REQUIRE_RTOS_HANDLE(canRxQueueHandle);
  REQUIRE_RTOS_HANDLE(uartRxQueueHandle);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of ioTask */
  ioTaskHandle = osThreadNew(StartIoTask, NULL, &ioTask_attributes);

  /* creation of displayTask */
  displayTaskHandle = osThreadNew(StartDisplayTask, NULL, &displayTask_attributes);

  /* creation of uartRxTask */
  uartRxTaskHandle = osThreadNew(StartUartRxTask, NULL, &uartRxTask_attributes);

  /* creation of uartTxTask */
  uartTxTaskHandle = osThreadNew(StartUartTxTask, NULL, &uartTxTask_attributes);

  /* creation of canRxTask */
  canRxTaskHandle = osThreadNew(StartCanRxTask, NULL, &canRxTask_attributes);

  /* creation of canTxTask */
  canTxTaskHandle = osThreadNew(StartCanTxTask, NULL, &canTxTask_attributes);

  /* creation of canPubTask */
  canPubTaskHandle = osThreadNew(StartCanPubTask, NULL, &canPubTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* Validate all thread creations - fail fast on low heap */
  REQUIRE_RTOS_HANDLE(ioTaskHandle);
  REQUIRE_RTOS_HANDLE(displayTaskHandle);
  REQUIRE_RTOS_HANDLE(uartRxTaskHandle);
  REQUIRE_RTOS_HANDLE(uartTxTaskHandle);
  REQUIRE_RTOS_HANDLE(canRxTaskHandle);
  REQUIRE_RTOS_HANDLE(canTxTaskHandle);
  REQUIRE_RTOS_HANDLE(canPubTaskHandle);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  DASH_Init();
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartIoTask */
/**
  * @brief  Function implementing the ioTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartIoTask */
__weak void StartIoTask(void *argument)
{
  /* USER CODE BEGIN StartIoTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartIoTask */
}

/* USER CODE BEGIN Header_StartDisplayTask */
/**
 * @brief Function implementing the displayTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDisplayTask */
__weak void StartDisplayTask(void *argument)
{
  /* USER CODE BEGIN StartDisplayTask */
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDisplayTask */
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
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartUartRxTask */
}

/* USER CODE BEGIN Header_StartUartTxTask */
/**
 * @brief Function implementing the uartTxTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartUartTxTask */
__weak void StartUartTxTask(void *argument)
{
  /* USER CODE BEGIN StartUartTxTask */
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartUartTxTask */
}

/* USER CODE BEGIN Header_StartCanRxTask */
/**
 * @brief Function implementing the canRxTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartCanRxTask */
__weak void StartCanRxTask(void *argument)
{
  /* USER CODE BEGIN StartCanRxTask */
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCanRxTask */
}

/* USER CODE BEGIN Header_StartCanTxTask */
/**
 * @brief Function implementing the canTxTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartCanTxTask */
__weak void StartCanTxTask(void *argument)
{
  /* USER CODE BEGIN StartCanTxTask */
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCanTxTask */
}

/* USER CODE BEGIN Header_StartCanPubTask */
/**
 * @brief Function implementing the canPubTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartCanPubTask */
__weak void StartCanPubTask(void *argument)
{
  /* USER CODE BEGIN StartCanPubTask */
  /* Infinite loop */
  for (;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartCanPubTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

