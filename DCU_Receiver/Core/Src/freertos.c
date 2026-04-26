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
#include "FEB_Task_Radio.h"
#include "feb_rtos_utils.h"
#include "feb_uart.h"
#include "FEB_Main.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticSemaphore_t osStaticMutexDef_t;
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
/* Second UART (UART4) console: thread + sync primitives for FEB_UART_INSTANCE_2 */
osThreadId_t uart4RxTaskHandle;
const osThreadAttr_t uart4RxTask_attributes = {
  .name = "uart4RxTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
osMutexId_t uartTxMutex2Handle;
const osMutexAttr_t uartTxMutex2_attributes = {
  .name = "uartTxMutex2"
};
osSemaphoreId_t uartTxSem2Handle;
const osSemaphoreAttr_t uartTxSem2_attributes = {
  .name = "uartTxSem2"
};
osMessageQueueId_t uartRxQueue2Handle;
const osMessageQueueAttr_t uartRxQueue2_attributes = {
  .name = "uartRxQueue2"
};
/* USER CODE END Variables */
/* Definitions for uartRxTask */
osThreadId_t uartRxTaskHandle;
const osThreadAttr_t uartRxTask_attributes = {
  .name = "uartRxTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for radioTask */
osThreadId_t radioTaskHandle;
const osThreadAttr_t radioTask_attributes = {
  .name = "radioTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for rxDataQueue */
osMessageQueueId_t rxDataQueueHandle;
const osMessageQueueAttr_t rxDataQueue_attributes = {
  .name = "rxDataQueue"
};
/* Definitions for uartRxQueue */
osMessageQueueId_t uartRxQueueHandle;
const osMessageQueueAttr_t uartRxQueue_attributes = {
  .name = "uartRxQueue"
};
/* Definitions for rxTimeoutTimer */
osTimerId_t rxTimeoutTimerHandle;
const osTimerAttr_t rxTimeoutTimer_attributes = {
  .name = "rxTimeoutTimer"
};
/* Definitions for spiMutex */
osMutexId_t spiMutexHandle;
osStaticMutexDef_t spiMutexCtrlBlock;
const osMutexAttr_t spiMutex_attributes = {
  .name = "spiMutex",
  .cb_mem = &spiMutexCtrlBlock,
  .cb_size = sizeof(spiMutexCtrlBlock),
};
/* Definitions for uartTxMutex */
osMutexId_t uartTxMutexHandle;
const osMutexAttr_t uartTxMutex_attributes = {
  .name = "uartTxMutex"
};
/* Definitions for logMutex */
osMutexId_t logMutexHandle;
const osMutexAttr_t logMutex_attributes = {
  .name = "logMutex"
};
/* Definitions for uartTxSem */
osSemaphoreId_t uartTxSemHandle;
const osSemaphoreAttr_t uartTxSem_attributes = {
  .name = "uartTxSem"
};
/* Definitions for radioEvents */
osEventFlagsId_t radioEventsHandle;
const osEventFlagsAttr_t radioEvents_attributes = {
  .name = "radioEvents"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartUartRxTask(void *argument);
void StartUart4RxTask(void *argument);
void RadioTask(void *argument);
void rxTimeoutCallback(void *argument);

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
  /* creation of spiMutex */
  spiMutexHandle = osMutexNew(&spiMutex_attributes);

  /* creation of uartTxMutex */
  uartTxMutexHandle = osMutexNew(&uartTxMutex_attributes);

  /* creation of logMutex */
  logMutexHandle = osMutexNew(&logMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  uartTxMutex2Handle = osMutexNew(&uartTxMutex2_attributes);
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of uartTxSem */
  uartTxSemHandle = osSemaphoreNew(1, 0, &uartTxSem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  uartTxSem2Handle = osSemaphoreNew(1, 0, &uartTxSem2_attributes);
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* creation of rxTimeoutTimer */
  rxTimeoutTimerHandle = osTimerNew(rxTimeoutCallback, osTimerOnce, NULL, &rxTimeoutTimer_attributes);

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of rxDataQueue */
  rxDataQueueHandle = osMessageQueueNew (4, 64, &rxDataQueue_attributes);

  /* creation of uartRxQueue */
  uartRxQueueHandle = osMessageQueueNew (8, sizeof(FEB_UART_RxQueueMsg_t), &uartRxQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  uartRxQueue2Handle = osMessageQueueNew(8, sizeof(FEB_UART_RxQueueMsg_t), &uartRxQueue2_attributes);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of uartRxTask */
  uartRxTaskHandle = osThreadNew(StartUartRxTask, NULL, &uartRxTask_attributes);

  /* creation of radioTask */
  radioTaskHandle = osThreadNew(RadioTask, NULL, &radioTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  uart4RxTaskHandle = osThreadNew(StartUart4RxTask, NULL, &uart4RxTask_attributes);
  REQUIRE_RTOS_HANDLE(spiMutexHandle);
  REQUIRE_RTOS_HANDLE(uartTxMutexHandle);
  REQUIRE_RTOS_HANDLE(logMutexHandle);
  REQUIRE_RTOS_HANDLE(uartTxSemHandle);
  REQUIRE_RTOS_HANDLE(uartRxQueueHandle);
  REQUIRE_RTOS_HANDLE(rxTimeoutTimerHandle);
  REQUIRE_RTOS_HANDLE(rxDataQueueHandle);
  REQUIRE_RTOS_HANDLE(uartRxTaskHandle);
  REQUIRE_RTOS_HANDLE(radioTaskHandle);
  REQUIRE_RTOS_HANDLE(uartTxMutex2Handle);
  REQUIRE_RTOS_HANDLE(uartTxSem2Handle);
  REQUIRE_RTOS_HANDLE(uartRxQueue2Handle);
  REQUIRE_RTOS_HANDLE(uart4RxTaskHandle);
  /* USER CODE END RTOS_THREADS */

  /* creation of radioEvents */
  radioEventsHandle = osEventFlagsNew(&radioEvents_attributes);

  /* USER CODE BEGIN RTOS_EVENTS */
  REQUIRE_RTOS_HANDLE(radioEventsHandle);
  /* Initialize FEB libraries after FreeRTOS objects are created */
  FEB_Init();
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartUartRxTask */
/**
  * @brief  Function implementing the uartRxTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartUartRxTask */
__weak void StartUartRxTask(void *argument)
{
  /* USER CODE BEGIN StartUartRxTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartUartRxTask */
}

/* USER CODE BEGIN Header_RadioTask */
/**
* @brief Function implementing the radioTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_RadioTask */
void RadioTask(void *argument)
{
  /* USER CODE BEGIN RadioTask */
  StartRadioTask(argument);
  /* USER CODE END RadioTask */
}

/* rxTimeoutCallback function */
void rxTimeoutCallback(void *argument)
{
  /* USER CODE BEGIN rxTimeoutCallback */
  (void)argument;
  /* Timer not used - FEB_RFM95 handles timeouts internally via osEventFlagsWait */
  /* USER CODE END rxTimeoutCallback */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

