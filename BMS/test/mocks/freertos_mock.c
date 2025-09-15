#ifdef UNIT_TEST

#include "freertos_mock.h"
#include "cmsis_os.h"
#include <string.h>

// External task handles that are referenced by the BMS code  
TaskHandle_t State_MachineHandle = (TaskHandle_t)0xABCDEF12;
SemaphoreHandle_t AccumulatorSemaphoreHandle = (SemaphoreHandle_t)0x12345679;
TaskHandle_t Balance_ControlHandle = (TaskHandle_t)0x87654322;

// Printf ISR handles (generated from STM32CubeMX .ioc file)
QueueHandle_t printfISRQueueHandle = NULL;
TaskHandle_t printfISRTaskHandle = NULL;

// Mock state variables
static bool mock_mutex_create_result = true;
static int mock_mutex_created = 0;
static int mock_semaphore_taken = 0;
static BaseType_t mock_semaphore_take_result = pdTRUE;
static int mock_semaphore_given = 0;
static BaseType_t mock_semaphore_give_result = pdTRUE;
static int mock_task_notify_from_isr_count = 0;
static uint32_t mock_last_notify_value_from_isr = 0;
static BaseType_t mock_task_notify_from_isr_result = pdTRUE;  
TaskHandle_t Charging_ControlHandle = (TaskHandle_t)0xABCDEF13;

// Mock handles for testing
static QueueHandle_t mock_printf_isr_queue_handle = (QueueHandle_t)0x12345678;
static TaskHandle_t mock_printf_isr_task_handle = (TaskHandle_t)0x87654321;

// FreeRTOS Mock Function Implementations
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    if (mock_mutex_create_result) {
        mock_mutex_created = 1;
        return (SemaphoreHandle_t)0x12345678;
    } else {
        mock_mutex_created = 0;
        return NULL;
    }
}

QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize) {
    (void)uxQueueLength;
    (void)uxItemSize;
    return mock_printf_isr_queue_handle;
}

BaseType_t xTaskCreate(void (*pxTaskCode)(void *), const char * const pcName, uint16_t usStackDepth, void *pvParameters, uint32_t uxPriority, TaskHandle_t *pxCreatedTask) {
    (void)pxTaskCode;
    (void)pcName;
    (void)usStackDepth;
    (void)pvParameters;
    (void)uxPriority;
    
    if (pxCreatedTask != NULL) {
        *pxCreatedTask = mock_printf_isr_task_handle;
    }
    return pdTRUE;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, uint32_t xBlockTime) {
    (void)xSemaphore;
    (void)xBlockTime;
    mock_semaphore_taken++;
    return mock_semaphore_take_result;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    (void)xSemaphore;
    mock_semaphore_given++;
    return mock_semaphore_give_result;
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {
    (void)xSemaphore;
}

void vQueueDelete(QueueHandle_t xQueue) {
    (void)xQueue;
}

void vTaskDelete(TaskHandle_t xTask) {
    (void)xTask;
}

BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xQueue;
    (void)pvItemToQueue;
    (void)pxHigherPriorityTaskWoken;
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, uint32_t xTicksToWait) {
    (void)xQueue;
    (void)xTicksToWait;
    (void)pvBuffer;
    return pdTRUE;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait) {
    (void)xQueue;
    (void)pvItemToQueue;
    (void)xTicksToWait;
    return pdTRUE;
}

uint32_t pdMS_TO_TICKS(uint32_t xTimeInMs) {
    return xTimeInMs;
}

void portYIELD_FROM_ISR(BaseType_t xHigherPriorityTaskWoken) {
    (void)xHigherPriorityTaskWoken;
}

// Task notification functions
BaseType_t xTaskNotify(TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction) {
    (void)xTaskToNotify;
    (void)ulValue;
    (void)eAction;
    return pdTRUE;
}

BaseType_t xTaskNotifyFromISR(TaskHandle_t xTaskToNotify, uint32_t ulValue, int eAction, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xTaskToNotify;
    (void)eAction;
    (void)pxHigherPriorityTaskWoken;
    mock_task_notify_from_isr_count++;
    mock_last_notify_value_from_isr = ulValue;
    return mock_task_notify_from_isr_result;
}

// Mock state variables for test setup
static bool mock_critical_section_entered = false;
static bool mock_critical_from_isr_entered = false;
static int mock_task_notify_result = 1;

// Essential Mock_* functions for test setup
void Mock_FreeRTOS_Reset(void) {
    mock_mutex_created = 0;
    mock_semaphore_taken = 0;
    mock_semaphore_given = 0;
    mock_task_notify_from_isr_count = 0;
    mock_last_notify_value_from_isr = 0;
    mock_critical_section_entered = false;
    mock_critical_from_isr_entered = false;
    mock_mutex_create_result = 1;
    mock_semaphore_take_result = 1;
    mock_semaphore_give_result = 1;
    mock_task_notify_result = 1;
    mock_task_notify_from_isr_result = 1;
}

void Mock_FreeRTOS_SetMutexCreateResult(int result) {
    mock_mutex_create_result = result;
}

void Mock_FreeRTOS_SetSemaphoreTakeResult(int result) {
    mock_semaphore_take_result = result;
}

void Mock_FreeRTOS_SetSemaphoreGiveResult(int result) {
    mock_semaphore_give_result = result;
}

void Mock_FreeRTOS_SetTaskNotifyResult(int result) {
    mock_task_notify_result = result;
}

void Mock_FreeRTOS_SetTaskNotifyFromISRResult(int result) {
    mock_task_notify_from_isr_result = result;
}

int Mock_FreeRTOS_GetMutexCreated(void) {
    return mock_mutex_created;
}

int Mock_FreeRTOS_GetSemaphoreTaken(void) {
    return mock_semaphore_taken;
}

int Mock_FreeRTOS_GetSemaphoreGiven(void) {
    return mock_semaphore_given;
}

int Mock_FreeRTOS_GetTaskNotifyFromISRCount(void) {
    return mock_task_notify_from_isr_count;
}

uint32_t Mock_FreeRTOS_GetLastNotifyValueFromISR(void) {
    return mock_last_notify_value_from_isr;
}

bool Mock_FreeRTOS_GetCriticalSectionEntered(void) {
    return mock_critical_section_entered;
}

bool Mock_FreeRTOS_GetCriticalFromISREntered(void) {
    return mock_critical_from_isr_entered;
}

void Mock_FreeRTOS_ResetCriticalTracking(void) {
    mock_critical_section_entered = false;
    mock_critical_from_isr_entered = false;
}

void Mock_FreeRTOS_ResetISRTracking(void) {
    mock_critical_from_isr_entered = false;
}

// Critical section functions
void Mock_FreeRTOS_EnterCritical(void) {
    mock_critical_section_entered = true;
}

void Mock_FreeRTOS_ExitCritical(void) {
    mock_critical_section_entered = false;
}

UBaseType_t Mock_FreeRTOS_EnterCriticalFromISR(void) {
    mock_critical_from_isr_entered = true;
    return 0;
}

void Mock_FreeRTOS_ExitCriticalFromISR(UBaseType_t uxSavedInterruptState) {
    (void)uxSavedInterruptState;
    mock_critical_from_isr_entered = false;
}

// Task suspension functions
void vTaskSuspendAll(void) {
    // No-op for unit tests
}

BaseType_t xTaskResumeAll(void) {
    return pdTRUE;
}

// osDelay implementation
void Mock_osDelay(uint32_t ms) {
    (void)ms;
    // No-op for unit tests
}

// Additional FreeRTOS mock function implementations
static bool mock_queue_create_result = true;
static bool mock_task_create_result = true;
static bool mock_queue_send_result = true;
static uint32_t mock_queue_timeout = 100;
static bool mock_queue_send_from_isr_result = true;
static bool mock_queue_receive_result = true;
static bool mock_delay_result = true;
static bool mock_queue_created = false;
static bool mock_task_created = false;
static bool mock_task_deleted = false;
static bool mock_queue_deleted = false;
static bool mock_mutex_deleted = false;
static bool mock_queue_sent = false;
static bool mock_queue_sent_from_isr = false;
static bool mock_queue_received = false;
static bool mock_delay_was_called = false;
static uint32_t mock_last_timeout = 0;
static uint32_t mock_last_delay_value = 0;
static uint32_t mock_last_queue_data = 0;
static uint32_t mock_last_semaphore_timeout = 0;
static uint32_t mock_semaphore_take_count = 0;
static uint32_t mock_semaphore_give_count = 0;

// CAN mock state
static HAL_StatusTypeDef mock_can_start_result = HAL_OK;
static HAL_StatusTypeDef mock_can_notification_result = HAL_OK;
static HAL_StatusTypeDef mock_can_transmit_result = HAL_OK;
static uint32_t mock_can_free_mailboxes = 3;
static HAL_StatusTypeDef mock_can_config_filter_result = HAL_OK;
static HAL_StatusTypeDef mock_can_get_rx_message_result = HAL_OK;
static bool mock_can_start_called = false;
static bool mock_can_notification_activated = false;
static bool mock_can_message_sent = false;
static uint32_t mock_can_config_filter_call_count = 0;
static bool mock_can_ivt_filter_configured = false;
static bool mock_can_dash_filter_configured = false;
static bool mock_can_charger_filter_configured = false;
static bool mock_can_heartbeat_filter_configured = false;
static bool mock_can_ivt_message_processed = false;
static bool mock_can_dash_message_processed = false;
static bool mock_can_charger_message_processed = false;
static bool mock_can_heartbeat_message_processed = false;
static uint32_t mock_can_last_sent_id = 0x100;
static uint32_t mock_can_message_count = 0;
static uint8_t mock_can_last_sent_data[8] = {0};
static CAN_FilterTypeDef mock_can_last_filter_config;

// UART mock state
static char mock_uart_buffer[1024] = {0};
static uint32_t mock_uart_buffer_pos = 0;

void Mock_FreeRTOS_SetQueueCreateResult(bool result) {
    mock_queue_create_result = result;
}

void Mock_FreeRTOS_SetTaskCreateResult(bool result) {
    mock_task_create_result = result;
}

void Mock_FreeRTOS_SetQueueSendResult(bool result) {
    mock_queue_send_result = result;
}

void Mock_FreeRTOS_SetQueueTimeout(uint32_t timeout) {
    mock_queue_timeout = timeout;
}

void Mock_FreeRTOS_SetQueueSendFromISRResult(bool result) {
    mock_queue_send_from_isr_result = result;
}

void Mock_FreeRTOS_SetQueueReceiveResult(bool result) {
    mock_queue_receive_result = result;
}

void Mock_FreeRTOS_SetQueueReceiveData(const void* data, uint32_t size) {
    (void)data;
    (void)size;
    // Store data for verification
}

void Mock_FreeRTOS_SimulateTaskSwitch(void) {
    // Simulate task switch
}

void Mock_FreeRTOS_SetDelayResult(bool result) {
    mock_delay_result = result;
}

bool Mock_FreeRTOS_GetQueueCreated(void) {
    return mock_queue_created;
}

bool Mock_FreeRTOS_GetTaskCreated(void) {
    return mock_task_created;
}

bool Mock_FreeRTOS_GetTaskNotifyCalled(void) {
    return mock_task_notify_from_isr_count > 0;
}

bool Mock_FreeRTOS_GetTaskDeleted(void) {
    return mock_task_deleted;
}

bool Mock_FreeRTOS_GetQueueDeleted(void) {
    return mock_queue_deleted;
}

bool Mock_FreeRTOS_GetMutexDeleted(void) {
    return mock_mutex_deleted;
}

bool Mock_FreeRTOS_GetQueueSent(void) {
    return mock_queue_sent;
}

bool Mock_FreeRTOS_GetQueueSentFromISR(void) {
    return mock_queue_sent_from_isr;
}

bool Mock_FreeRTOS_GetQueueReceived(void) {
    return mock_queue_received;
}

bool Mock_FreeRTOS_GetDelayWasCalled(void) {
    return mock_delay_was_called;
}

uint32_t Mock_FreeRTOS_GetLastTimeout(void) {
    return mock_last_timeout;
}

uint32_t Mock_FreeRTOS_GetLastDelayValue(void) {
    return mock_last_delay_value;
}

uint32_t Mock_FreeRTOS_GetLastQueueData(void) {
    return mock_last_queue_data;
}

uint32_t Mock_FreeRTOS_GetLastSemaphoreTimeout(void) {
    return mock_last_semaphore_timeout;
}

uint32_t Mock_FreeRTOS_GetSemaphoreTakeCount(void) {
    return mock_semaphore_take_count;
}

uint32_t Mock_FreeRTOS_GetSemaphoreGiveCount(void) {
    return mock_semaphore_give_count;
}

// CAN mock functions
void Mock_CAN_Reset(void) {
    mock_can_start_result = HAL_OK;
    mock_can_notification_result = HAL_OK;
    mock_can_transmit_result = HAL_OK;
    mock_can_free_mailboxes = 3;
    mock_can_config_filter_result = HAL_OK;
    mock_can_get_rx_message_result = HAL_OK;
    mock_can_start_called = false;
    mock_can_notification_activated = false;
    mock_can_message_sent = false;
    mock_can_config_filter_call_count = 0;
    mock_can_ivt_filter_configured = false;
    mock_can_dash_filter_configured = false;
    mock_can_charger_filter_configured = false;
    mock_can_heartbeat_filter_configured = false;
    mock_can_ivt_message_processed = false;
    mock_can_dash_message_processed = false;
    mock_can_charger_message_processed = false;
    mock_can_heartbeat_message_processed = false;
    mock_can_last_sent_id = 0x100;
    mock_can_message_count = 0;
    memset(mock_can_last_sent_data, 0, sizeof(mock_can_last_sent_data));
    memset(&mock_can_last_filter_config, 0, sizeof(mock_can_last_filter_config));
}

void Mock_CAN_SetStartResult(HAL_StatusTypeDef result) {
    mock_can_start_result = result;
}

void Mock_CAN_SetNotificationResult(HAL_StatusTypeDef result) {
    mock_can_notification_result = result;
}

void Mock_CAN_SetTransmitResult(HAL_StatusTypeDef result) {
    mock_can_transmit_result = result;
}

void Mock_CAN_SetFreeMailboxes(uint32_t count) {
    mock_can_free_mailboxes = count;
}

void Mock_CAN_SetMailboxFreeAfterDelay(uint32_t count, uint32_t delay_ms) {
    (void)delay_ms;
    mock_can_free_mailboxes = count;
}

void Mock_CAN_SetConfigFilterResult(HAL_StatusTypeDef result) {
    mock_can_config_filter_result = result;
}

void Mock_CAN_SetGetRxMessageResult(HAL_StatusTypeDef result) {
    mock_can_get_rx_message_result = result;
}

void Mock_CAN_SetRxMessage(uint32_t id, const uint8_t* data, uint32_t size) {
    (void)id;
    (void)data;
    (void)size;
    // Store message for processing
}

void Mock_CAN_ResetFilterConfig(void) {
    mock_can_config_filter_call_count = 0;
}

bool Mock_CAN_GetStartCalled(void) {
    return mock_can_start_called;
}

bool Mock_CAN_GetNotificationActivated(void) {
    return mock_can_notification_activated;
}

bool Mock_CAN_GetMessageSent(void) {
    return mock_can_message_sent;
}

bool Mock_CAN_GetConfigFilterCallCount(void) {
    return mock_can_config_filter_call_count;
}

bool Mock_CAN_IVT_GetFilterConfigured(void) {
    return mock_can_ivt_filter_configured;
}

bool Mock_CAN_DASH_GetFilterConfigured(void) {
    return mock_can_dash_filter_configured;
}

bool Mock_CAN_Charger_GetFilterConfigured(void) {
    return mock_can_charger_filter_configured;
}

bool Mock_CAN_Heartbeat_GetFilterConfigured(void) {
    return mock_can_heartbeat_filter_configured;
}

bool Mock_CAN_IVT_GetMessageProcessed(void) {
    return mock_can_ivt_message_processed;
}

bool Mock_CAN_DASH_GetMessageProcessed(void) {
    return mock_can_dash_message_processed;
}

bool Mock_CAN_Charger_GetMessageProcessed(void) {
    return mock_can_charger_message_processed;
}

bool Mock_CAN_Heartbeat_GetMessageProcessed(void) {
    return mock_can_heartbeat_message_processed;
}

uint32_t Mock_CAN_GetLastSentID(void) {
    return mock_can_last_sent_id;
}

uint32_t Mock_CAN_GetMessageCount(void) {
    return mock_can_message_count;
}

uint8_t* Mock_CAN_GetLastSentData(void) {
    return mock_can_last_sent_data;
}

CAN_FilterTypeDef* Mock_CAN_GetLastFilterConfig(void) {
    return &mock_can_last_filter_config;
}

// UART mock functions
bool Mock_UART_ContainsString(const char* str) {
    return strstr(mock_uart_buffer, str) != NULL;
}

// CMSIS-OS2 function implementations
osSemaphoreId_t osSemaphoreNew(uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr) {
    (void)max_count;
    (void)initial_count;
    (void)attr;
    return (osSemaphoreId_t)0x12345678;
}

osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id, uint32_t timeout) {
    (void)semaphore_id;
    (void)timeout;
    return osOK;
}

osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id) {
    (void)semaphore_id;
    return osOK;
}

osStatus_t osSemaphoreDelete(osSemaphoreId_t semaphore_id) {
    (void)semaphore_id;
    return osOK;
}

osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size, const osMessageQueueAttr_t *attr) {
    (void)msg_count;
    (void)msg_size;
    (void)attr;
    return (osMessageQueueId_t)0x87654321;
}

osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout) {
    (void)mq_id;
    (void)msg_ptr;
    (void)msg_prio;
    (void)timeout;
    return osOK;
}

osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout) {
    (void)mq_id;
    (void)msg_ptr;
    (void)msg_prio;
    (void)timeout;
    return osOK;
}

osStatus_t osMessageQueueDelete(osMessageQueueId_t mq_id) {
    (void)mq_id;
    return osOK;
}

#endif // UNIT_TEST