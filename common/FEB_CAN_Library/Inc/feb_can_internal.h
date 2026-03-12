/**
 ******************************************************************************
 * @file           : feb_can_internal.h
 * @brief          : Internal types and helpers for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Internal implementation details. Do not include directly in user code.
 * Contains:
 *   - FreeRTOS/bare-metal abstraction macros
 *   - Internal data structures
 *   - Queue message types
 *
 ******************************************************************************
 */

#ifndef FEB_CAN_INTERNAL_H
#define FEB_CAN_INTERNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_can_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

  /* ============================================================================
   * FreeRTOS Abstraction Layer
   * ============================================================================ */

#if FEB_CAN_USE_FREERTOS

#include "cmsis_os2.h"

  typedef osMessageQueueId_t FEB_CAN_Queue_t;
  typedef osMutexId_t FEB_CAN_Mutex_t;
  typedef osSemaphoreId_t FEB_CAN_Semaphore_t;

#define FEB_CAN_QUEUE_CREATE(depth, item_size) osMessageQueueNew(depth, item_size, NULL)
#define FEB_CAN_QUEUE_DELETE(q) osMessageQueueDelete(q)
#define FEB_CAN_QUEUE_SEND(q, item, timeout) (osMessageQueuePut(q, item, 0, timeout) == osOK)
#define FEB_CAN_QUEUE_SEND_ISR(q, item) (osMessageQueuePut(q, item, 0, 0) == osOK)
#define FEB_CAN_QUEUE_RECEIVE(q, item, timeout) (osMessageQueueGet(q, item, NULL, timeout) == osOK)
#define FEB_CAN_QUEUE_RECEIVE_ISR(q, item) (osMessageQueueGet(q, item, NULL, 0) == osOK)
#define FEB_CAN_QUEUE_COUNT(q) osMessageQueueGetCount(q)
#define FEB_CAN_QUEUE_SPACE(q) osMessageQueueGetSpace(q)
#define FEB_CAN_QUEUE_RESET(q) osMessageQueueReset(q)

#define FEB_CAN_MUTEX_CREATE() osMutexNew(NULL)
#define FEB_CAN_MUTEX_DELETE(m) osMutexDelete(m)
#define FEB_CAN_MUTEX_LOCK(m) osMutexAcquire(m, osWaitForever)
#define FEB_CAN_MUTEX_UNLOCK(m) osMutexRelease(m)
#define FEB_CAN_MUTEX_TRYLOCK(m) (osMutexAcquire(m, 0) == osOK)

#define FEB_CAN_SEM_CREATE(max, init) osSemaphoreNew(max, init, NULL)
#define FEB_CAN_SEM_DELETE(s) osSemaphoreDelete(s)
#define FEB_CAN_SEM_GIVE(s) osSemaphoreRelease(s)
#define FEB_CAN_SEM_GIVE_ISR(s) osSemaphoreRelease(s)
#define FEB_CAN_SEM_TAKE(s, timeout) (osSemaphoreAcquire(s, timeout) == osOK)
#define FEB_CAN_SEM_TAKE_ISR(s) (osSemaphoreAcquire(s, 0) == osOK)

#define FEB_CAN_ENTER_CRITICAL() /* Use mutex instead in FreeRTOS */
#define FEB_CAN_EXIT_CRITICAL()

#define FEB_CAN_IN_ISR() (xPortIsInsideInterrupt())

#define FEB_CAN_DELAY(ms) osDelay(ms)

#else /* Bare-metal */

typedef void *FEB_CAN_Queue_t;
typedef uint8_t FEB_CAN_Mutex_t;
typedef volatile uint8_t FEB_CAN_Semaphore_t;

#define FEB_CAN_QUEUE_CREATE(depth, item_size) NULL
#define FEB_CAN_QUEUE_DELETE(q) ((void)0)
#define FEB_CAN_QUEUE_SEND(q, item, timeout) (true)
#define FEB_CAN_QUEUE_SEND_ISR(q, item) (true)
#define FEB_CAN_QUEUE_RECEIVE(q, item, timeout) (false)
#define FEB_CAN_QUEUE_RECEIVE_ISR(q, item) (false)
#define FEB_CAN_QUEUE_COUNT(q) (0)
#define FEB_CAN_QUEUE_SPACE(q) (1)
#define FEB_CAN_QUEUE_RESET(q) ((void)0)

#define FEB_CAN_MUTEX_CREATE() (0)
#define FEB_CAN_MUTEX_DELETE(m) ((void)0)
#define FEB_CAN_MUTEX_LOCK(m) __disable_irq()
#define FEB_CAN_MUTEX_UNLOCK(m) __enable_irq()
#define FEB_CAN_MUTEX_TRYLOCK(m) (__disable_irq(), true)

#define FEB_CAN_SEM_CREATE(max, init) (init)
#define FEB_CAN_SEM_DELETE(s) ((void)0)
#define FEB_CAN_SEM_GIVE(s) ((s) = 1)
#define FEB_CAN_SEM_GIVE_ISR(s) ((s) = 1)
#define FEB_CAN_SEM_TAKE(s, timeout) ((s) ? ((s) = 0, true) : false)
#define FEB_CAN_SEM_TAKE_ISR(s) ((s) ? ((s) = 0, true) : false)

#define FEB_CAN_ENTER_CRITICAL() __disable_irq()
#define FEB_CAN_EXIT_CRITICAL() __enable_irq()

#define FEB_CAN_IN_ISR() ((__get_IPSR() & 0xFF) != 0)

#define FEB_CAN_DELAY(ms) HAL_Delay(ms)

#endif /* FEB_CAN_USE_FREERTOS */

  /* ============================================================================
   * CAN Message Structure for Queuing
   * ============================================================================ */

  /**
   * @brief Internal CAN message structure for queue operations
   */
  typedef struct
  {
    uint32_t can_id;    /**< CAN identifier */
    uint8_t data[8];    /**< Message data */
    uint8_t length;     /**< Data length (0-8) */
    uint8_t id_type;    /**< 0=Standard, 1=Extended */
    uint8_t instance;   /**< CAN1 or CAN2 */
    uint8_t reserved;   /**< Padding for alignment */
    uint32_t timestamp; /**< Reception/transmission timestamp */
  } FEB_CAN_Message_t;

  /* ============================================================================
   * Forward Declarations
   * ============================================================================ */

  typedef enum FEB_CAN_Instance FEB_CAN_Instance_t;
  typedef enum FEB_CAN_ID_Type FEB_CAN_ID_Type_t;
  typedef enum FEB_CAN_Filter_Type FEB_CAN_Filter_Type_t;
  typedef enum FEB_CAN_FIFO FEB_CAN_FIFO_t;

  /* ============================================================================
   * RX Handle Structure
   * ============================================================================ */

  /**
   * @brief RX callback handle internal structure
   */
  typedef struct
  {
    uint32_t can_id;        /**< CAN ID to match */
    uint32_t mask;          /**< ID mask (0xFFFFFFFF for exact match) */
    void *callback;         /**< Callback function pointer */
    void *user_data;        /**< User context data */
    uint8_t instance;       /**< CAN instance */
    uint8_t id_type;        /**< Standard or Extended */
    uint8_t filter_type;    /**< Exact, mask, or wildcard */
    uint8_t is_active;      /**< Handle in use flag */
    uint8_t is_extended_cb; /**< Using extended callback type */
    uint8_t filter_bank;    /**< Assigned filter bank number */
    uint8_t fifo;           /**< FIFO assignment (0 or 1) */
    uint8_t reserved;       /**< Padding */
  } FEB_CAN_RX_Handle_Internal_t;

  /* ============================================================================
   * TX Handle Structure
   * ============================================================================ */

  /**
   * @brief TX slot handle for registered transmitters
   */
  typedef struct
  {
    uint32_t can_id;                                   /**< CAN ID to transmit */
    void *data_ptr;                                    /**< Pointer to source data structure */
    size_t data_size;                                  /**< Size of data structure */
    uint32_t period_ms;                                /**< Periodic interval (0 = manual only) */
    uint32_t last_tx_time;                             /**< Last transmission timestamp */
    int (*pack_func)(uint8_t *, const void *, size_t); /**< Optional pack function */
    uint8_t instance;                                  /**< CAN instance */
    uint8_t id_type;                                   /**< Standard or Extended */
    uint8_t is_active;                                 /**< Slot in use flag */
    uint8_t reserved;                                  /**< Padding */
  } FEB_CAN_TX_Handle_Internal_t;

  /* ============================================================================
   * Filter Bank Tracking
   * ============================================================================ */

  /**
   * @brief Filter bank entry for tracking configured filters
   */
  typedef struct
  {
    uint32_t id;       /**< Filter ID */
    uint32_t mask;     /**< Filter mask */
    uint8_t id_type;   /**< Standard or Extended */
    uint8_t fifo;      /**< FIFO assignment */
    uint8_t is_active; /**< Filter in use flag */
    uint8_t mode;      /**< IDMASK or IDLIST */
  } FEB_CAN_Filter_Entry_t;

  /* ============================================================================
   * Global Context Structure
   * ============================================================================ */

  /**
   * @brief Internal library context (defined in feb_can.c)
   */
  typedef struct
  {
    /* HAL handles */
    void *hcan[FEB_CAN_NUM_INSTANCES]; /**< CAN_HandleTypeDef pointers */

    /* Queues (FreeRTOS mode) */
    FEB_CAN_Queue_t tx_queue;
    FEB_CAN_Queue_t rx_queue;

    /* Mutexes */
    FEB_CAN_Mutex_t tx_mutex;
    FEB_CAN_Mutex_t rx_mutex;

    /* TX complete semaphore for flow control */
    FEB_CAN_Semaphore_t tx_sem;
    volatile uint8_t tx_pending_count; /**< Messages pending in mailboxes */

    /* Error counters for diagnostics */
    volatile uint32_t rx_queue_overflow_count; /**< RX messages dropped due to queue full */
    volatile uint32_t tx_queue_overflow_count; /**< TX messages dropped due to queue full */
    volatile uint32_t tx_timeout_count;        /**< TX messages dropped due to mailbox timeout */
    volatile uint32_t hal_error_count;         /**< HAL errors encountered */

    /* RX handles */
    FEB_CAN_RX_Handle_Internal_t rx_handles[FEB_CAN_MAX_RX_HANDLES];
    uint32_t rx_handle_count;

    /* TX handles */
    FEB_CAN_TX_Handle_Internal_t tx_handles[FEB_CAN_MAX_TX_HANDLES];
    uint32_t tx_handle_count;

    /* Filter tracking */
    FEB_CAN_Filter_Entry_t filters[FEB_CAN_TOTAL_FILTER_BANKS];

    /* Timestamp function */
    uint32_t (*get_tick_ms)(void);

    /* State flags */
    bool initialized;

  } FEB_CAN_Context_t;

  /* ============================================================================
   * Internal Function Declarations
   * ============================================================================ */

  /**
   * @brief Get global library context (defined in feb_can.c)
   */
  FEB_CAN_Context_t *feb_can_get_context(void);

  /**
   * @brief Internal RX dispatch function
   */
  void feb_can_rx_dispatch(FEB_CAN_Instance_t instance, uint32_t can_id, uint8_t id_type, const uint8_t *data,
                           uint8_t length, uint32_t timestamp);

  /**
   * @brief Internal TX transmit via HAL
   */
  int feb_can_tx_hal_transmit(FEB_CAN_Instance_t instance, uint32_t can_id, uint8_t id_type, const uint8_t *data,
                              uint8_t length);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_INTERNAL_H */
