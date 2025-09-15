#ifndef FREERTOS_MOCK_H
#define FREERTOS_MOCK_H

#ifdef UNIT_TEST

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// FreeRTOS Mock Types
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
typedef int BaseType_t;
typedef uint32_t TickType_t;

// FreeRTOS Constants
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFF

// FreeRTOS Mock Function Prototypes
SemaphoreHandle_t xSemaphoreCreateMutex(void);
QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize);
BaseType_t xTaskCreate(void (*pxTaskCode)(void *), const char * const pcName, uint16_t usStackDepth, void *pvParameters, uint32_t uxPriority, TaskHandle_t *pxCreatedTask);
BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, uint32_t xBlockTime);
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);
void vSemaphoreDelete(SemaphoreHandle_t xSemaphore);
void vQueueDelete(QueueHandle_t xQueue);
void vTaskDelete(TaskHandle_t xTask);
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, uint32_t xTicksToWait);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait);
uint32_t pdMS_TO_TICKS(uint32_t xTimeInMs);
void portYIELD_FROM_ISR(BaseType_t xHigherPriorityTaskWoken);

// Task notification type definition
typedef int eNotifyAction;

// Task notification functions
BaseType_t xTaskNotify(TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction);
BaseType_t xTaskNotifyFromISR(TaskHandle_t xTaskToNotify, uint32_t ulValue, int eAction, BaseType_t *pxHigherPriorityTaskWoken);

// FreeRTOS task notification actions
#define eNoAction 0
#define eSetBits 1
#define eIncrement 2
#define eSetValueWithOverwrite 3
#define eSetValueWithoutOverwrite 4

// CMSIS-OS2 task support
typedef uint32_t UBaseType_t;

// Critical section function prototypes (actual implementations)
UBaseType_t taskENTER_CRITICAL_FROM_ISR_FUNC(void);
void taskEXIT_CRITICAL_FROM_ISR_FUNC(UBaseType_t uxSavedInterruptState);

// Critical section macros for FreeRTOS
#define taskENTER_CRITICAL() Mock_FreeRTOS_EnterCritical()
#define taskEXIT_CRITICAL() Mock_FreeRTOS_ExitCritical()
#define taskENTER_CRITICAL_FROM_ISR() Mock_FreeRTOS_EnterCriticalFromISR()
#define taskEXIT_CRITICAL_FROM_ISR(x) Mock_FreeRTOS_ExitCriticalFromISR(x)

// osDelay mock function - declared in cmsis_os.h
void Mock_osDelay(uint32_t ms);
#ifndef osDelay
#define osDelay(ms) Mock_osDelay(ms)
#endif

// External task handles that need to be mocked
extern TaskHandle_t State_MachineHandle;

// Printf ISR handles (generated from STM32CubeMX .ioc file)
extern QueueHandle_t printfISRQueueHandle;
extern TaskHandle_t printfISRTaskHandle;

// Critical section functions (needed for FreeRTOS macros)
void Mock_FreeRTOS_EnterCritical(void);
void Mock_FreeRTOS_ExitCritical(void);
UBaseType_t Mock_FreeRTOS_EnterCriticalFromISR(void);
void Mock_FreeRTOS_ExitCriticalFromISR(UBaseType_t uxSavedInterruptState);

// Essential Mock_* functions for test setup only
void Mock_FreeRTOS_Reset(void);
void Mock_FreeRTOS_SetMutexCreateResult(int result);
void Mock_FreeRTOS_SetSemaphoreTakeResult(int result);
void Mock_FreeRTOS_SetSemaphoreGiveResult(int result);
void Mock_FreeRTOS_SetTaskNotifyResult(int result);
void Mock_FreeRTOS_SetTaskNotifyFromISRResult(int result);
int Mock_FreeRTOS_GetMutexCreated(void);
int Mock_FreeRTOS_GetSemaphoreTaken(void);
int Mock_FreeRTOS_GetSemaphoreGiven(void);
int Mock_FreeRTOS_GetTaskNotifyFromISRCount(void);
uint32_t Mock_FreeRTOS_GetLastNotifyValueFromISR(void);
bool Mock_FreeRTOS_GetCriticalSectionEntered(void);
bool Mock_FreeRTOS_GetCriticalFromISREntered(void);
void Mock_FreeRTOS_ResetCriticalTracking(void);
void Mock_FreeRTOS_ResetISRTracking(void);

// Additional FreeRTOS mock functions needed by tests
void Mock_FreeRTOS_SetQueueCreateResult(bool result);
void Mock_FreeRTOS_SetTaskCreateResult(bool result);
void Mock_FreeRTOS_SetQueueSendResult(bool result);
void Mock_FreeRTOS_SetQueueTimeout(uint32_t timeout);
void Mock_FreeRTOS_SetQueueSendFromISRResult(bool result);
void Mock_FreeRTOS_SetQueueReceiveResult(bool result);
void Mock_FreeRTOS_SetQueueReceiveData(const void* data, uint32_t size);
void Mock_FreeRTOS_SimulateTaskSwitch(void);
void Mock_FreeRTOS_SetDelayResult(bool result);

bool Mock_FreeRTOS_GetQueueCreated(void);
bool Mock_FreeRTOS_GetTaskCreated(void);
bool Mock_FreeRTOS_GetTaskNotifyCalled(void);
bool Mock_FreeRTOS_GetTaskDeleted(void);
bool Mock_FreeRTOS_GetQueueDeleted(void);
bool Mock_FreeRTOS_GetMutexDeleted(void);
bool Mock_FreeRTOS_GetQueueSent(void);
bool Mock_FreeRTOS_GetQueueSentFromISR(void);
bool Mock_FreeRTOS_GetQueueReceived(void);
bool Mock_FreeRTOS_GetDelayWasCalled(void);

uint32_t Mock_FreeRTOS_GetLastTimeout(void);
uint32_t Mock_FreeRTOS_GetLastDelayValue(void);
uint32_t Mock_FreeRTOS_GetLastQueueData(void);
uint32_t Mock_FreeRTOS_GetLastSemaphoreTimeout(void);
uint32_t Mock_FreeRTOS_GetSemaphoreTakeCount(void);
uint32_t Mock_FreeRTOS_GetSemaphoreGiveCount(void);

// CAN mock functions
void Mock_CAN_Reset(void);
void Mock_CAN_SetStartResult(HAL_StatusTypeDef result);
void Mock_CAN_SetNotificationResult(HAL_StatusTypeDef result);
void Mock_CAN_SetTransmitResult(HAL_StatusTypeDef result);
void Mock_CAN_SetFreeMailboxes(uint32_t count);
void Mock_CAN_SetMailboxFreeAfterDelay(uint32_t count, uint32_t delay_ms);
void Mock_CAN_SetConfigFilterResult(HAL_StatusTypeDef result);
void Mock_CAN_SetGetRxMessageResult(HAL_StatusTypeDef result);
void Mock_CAN_SetRxMessage(uint32_t id, const uint8_t* data, uint32_t size);
void Mock_CAN_ResetFilterConfig(void);

bool Mock_CAN_GetStartCalled(void);
bool Mock_CAN_GetNotificationActivated(void);
bool Mock_CAN_GetMessageSent(void);
bool Mock_CAN_GetConfigFilterCallCount(void);
bool Mock_CAN_IVT_GetFilterConfigured(void);
bool Mock_CAN_DASH_GetFilterConfigured(void);
bool Mock_CAN_Charger_GetFilterConfigured(void);
bool Mock_CAN_Heartbeat_GetFilterConfigured(void);
bool Mock_CAN_IVT_GetMessageProcessed(void);
bool Mock_CAN_DASH_GetMessageProcessed(void);
bool Mock_CAN_Charger_GetMessageProcessed(void);
bool Mock_CAN_Heartbeat_GetMessageProcessed(void);

uint32_t Mock_CAN_GetLastSentID(void);
uint32_t Mock_CAN_GetMessageCount(void);
uint8_t* Mock_CAN_GetLastSentData(void);
CAN_FilterTypeDef* Mock_CAN_GetLastFilterConfig(void);

// UART mock functions
bool Mock_UART_ContainsString(const char* str);

// Define missing constants for tests
#ifndef FEB_DEV_INDEX_DASH
#define FEB_DEV_INDEX_DASH 0
#endif

// Define FEB_HIGH and FEB_LOW constants for tests
#ifndef FEB_HIGH
#define FEB_HIGH 1
#endif
#ifndef FEB_LOW
#define FEB_LOW 0
#endif

// FreeRTOS task suspension functions
void vTaskSuspendAll(void);
BaseType_t xTaskResumeAll(void);

// FreeRTOS task notification functions

#ifdef __cplusplus
}
#endif

#endif // UNIT_TEST

#endif // FREERTOS_MOCK_H