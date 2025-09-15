#ifndef CMSIS_OS_H
#define CMSIS_OS_H

#ifdef UNIT_TEST

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CMSIS-OS v1 and v2 Unified Mock Header
// ============================================================================

// CMSIS-OS Status and Return Values
typedef enum {
    osOK                  = 0,         ///< Operation completed successfully.
    osError               = -1,        ///< Unspecified RTOS error: run-time error but no other error message fits.
    osErrorTimeout        = -2,        ///< Operation not completed within the timeout period.
    osErrorResource       = -3,        ///< Resource not available.
    osErrorParameter      = -4,        ///< Parameter error.
    osErrorNoMemory       = -5,        ///< System is out of memory: it was impossible to allocate or reserve memory for the operation.
    osErrorISR            = -6,        ///< Not allowed in ISR context: the function cannot be called from interrupt service routines.
    osStatusReserved      = 0x7FFFFFFF ///< Reserved
    
} osStatus_t;

// Priority levels
typedef enum {
    osPriorityIdle = -3,
    osPriorityLow = -2,
    osPriorityBelowNormal = -1,
    osPriorityNormal = 0,
    osPriorityAboveNormal = 1,
    osPriorityHigh = 2,
    osPriorityRealtime = 3
} osPriority_t;

// Timeout Values
#define osWaitForever         0xFFFFFFFFU ///< Wait forever timeout value.
#define osNoWait              0x0U         ///< Do not wait timeout value.

// General Wait Functions
typedef uint32_t osTime_t;

// ============================================================================
// Thread/Task Types and Functions
// ============================================================================

// Thread ID type
typedef void *osThreadId_t;

// External task handles that are referenced by the BMS code
extern osThreadId_t State_MachineHandle;

// Thread function type
typedef void (*osThreadFunc_t) (void *argument);

// Thread attributes structure
typedef struct {
    const char                   *name;   ///< name of the thread
    uint32_t                     attr_bits; ///< attribute bits
    void                        *cb_mem;  ///< memory for control block
    uint32_t                     cb_size; ///< size of provided memory for control block
    void                        *stack_mem; ///< memory for stack
    uint32_t                     stack_size; ///< size of stack
    uint32_t                     priority;   ///< initial thread priority (default: osPriorityNormal)
    uint32_t                     reserved;   ///< reserved (must be 0)
} osThreadAttr_t;

// ============================================================================
// Semaphore Types and Functions
// ============================================================================

typedef void *osSemaphoreId_t;

typedef struct {
    const char                   *name;   ///< name of the semaphore
    uint32_t                     attr_bits; ///< attribute bits
    void                        *cb_mem;  ///< memory for control block
    uint32_t                     cb_size; ///< size of provided memory for control block
} osSemaphoreAttr_t;

// ============================================================================
// Message Queue Types and Functions
// ============================================================================

typedef void *osMessageQueueId_t;

typedef struct {
    const char                   *name;   ///< name of the message queue
    uint32_t                     attr_bits; ///< attribute bits
    void                        *cb_mem;  ///< memory for control block
    uint32_t                     cb_size; ///< size of provided memory for control block
    void                        *mq_mem;  ///< memory for message queue buffer
    uint32_t                     mq_size; ///< size of provided memory for message queue buffer
} osMessageQueueAttr_t;

// ============================================================================
// Mutex and Timer Types (for compatibility)
// ============================================================================

typedef void *osMutexId_t;
typedef void *osTimerId_t;

// ============================================================================
// Mock Function Declarations
// ============================================================================

// Thread/Task functions
void Mock_osDelay(uint32_t ms);
#define osDelay(ms) Mock_osDelay(ms)
#define osThreadYield() do { } while(0)

// Semaphore functions (implemented in freertos_mock.c)
osSemaphoreId_t osSemaphoreNew(uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr);
osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id, uint32_t timeout);
osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id);
osStatus_t osSemaphoreDelete(osSemaphoreId_t semaphore_id);

// Message Queue functions (implemented in freertos_mock.c)
osMessageQueueId_t osMessageQueueNew(uint32_t msg_count, uint32_t msg_size, const osMessageQueueAttr_t *attr);
osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout);
osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout);
osStatus_t osMessageQueueDelete(osMessageQueueId_t mq_id);


// Include FreeRTOS mock definitions that are already implemented
#include "freertos_mock.h"

// Map CMSIS-OS calls to FreeRTOS mock calls where needed
#define vTaskDelay(ticks) do { } while(0)

#ifdef __cplusplus
}
#endif

#endif // UNIT_TEST

#endif // CMSIS_OS_H