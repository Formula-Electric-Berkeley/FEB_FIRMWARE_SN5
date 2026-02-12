/**
 ******************************************************************************
 * @file           : feb_uart_internal.h
 * @brief          : Internal types and helpers for FEB UART Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Internal implementation details. Do not include directly in user code.
 * Contains:
 *   - Ring buffer data structure and operations
 *   - FreeRTOS/bare-metal abstraction macros
 *   - Internal state structures
 *
 ******************************************************************************
 */

#ifndef FEB_UART_INTERNAL_H
#define FEB_UART_INTERNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_uart_config.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

  /* ============================================================================
   * FreeRTOS Abstraction Layer
   * ============================================================================ */

#if FEB_UART_USE_FREERTOS

#include "cmsis_os2.h"

  typedef osMutexId_t FEB_UART_Mutex_t;
  typedef osSemaphoreId_t FEB_UART_Semaphore_t;

#define FEB_UART_MUTEX_CREATE() osMutexNew(NULL)
#define FEB_UART_MUTEX_DELETE(m) osMutexDelete(m)
#define FEB_UART_MUTEX_LOCK(m) osMutexAcquire(m, osWaitForever)
#define FEB_UART_MUTEX_UNLOCK(m) osMutexRelease(m)
#define FEB_UART_MUTEX_LOCK_ISR(m) ((void)0) /* Mutexes not safe from ISR */
#define FEB_UART_MUTEX_UNLOCK_ISR(m) ((void)0)

#define FEB_UART_SEM_CREATE(max, init) osSemaphoreNew(max, init, NULL)
#define FEB_UART_SEM_DELETE(s) osSemaphoreDelete(s)
#define FEB_UART_SEM_GIVE(s) osSemaphoreRelease(s)
#define FEB_UART_SEM_TAKE(s, timeout) (osSemaphoreAcquire(s, timeout) == osOK)

#define FEB_UART_ENTER_CRITICAL() /* Use mutex instead in FreeRTOS */
#define FEB_UART_EXIT_CRITICAL()

#define FEB_UART_IN_ISR() (xPortIsInsideInterrupt())

#else /* Bare-metal */

typedef uint8_t FEB_UART_Mutex_t;
typedef volatile uint8_t FEB_UART_Semaphore_t;

#define FEB_UART_MUTEX_CREATE() (0)
#define FEB_UART_MUTEX_DELETE(m) ((void)0)
#define FEB_UART_MUTEX_LOCK(m) __disable_irq()
#define FEB_UART_MUTEX_UNLOCK(m) __enable_irq()
#define FEB_UART_MUTEX_LOCK_ISR(m) ((void)0) /* Already in ISR context */
#define FEB_UART_MUTEX_UNLOCK_ISR(m) ((void)0)

#define FEB_UART_SEM_CREATE(max, init) (init)
#define FEB_UART_SEM_DELETE(s) ((void)0)
#define FEB_UART_SEM_GIVE(s) ((s) = 1)
#define FEB_UART_SEM_TAKE(s, timeout) ((s) ? ((s) = 0, 1) : 0)

#define FEB_UART_ENTER_CRITICAL() __disable_irq()
#define FEB_UART_EXIT_CRITICAL() __enable_irq()

#define FEB_UART_IN_ISR() ((__get_IPSR() & 0xFF) != 0)

#endif /* FEB_UART_USE_FREERTOS */

  /* ============================================================================
   * Ring Buffer Structure
   * ============================================================================ */

  /**
   * @brief Ring buffer for UART TX/RX
   *
   * Lock-free for single-producer single-consumer when:
   *   - Producer only modifies head
   *   - Consumer only modifies tail
   *
   * For multi-producer (e.g., multiple tasks calling printf), external locking
   * is required.
   */
  typedef struct
  {
    uint8_t *buffer;      /**< Pointer to user-provided buffer */
    size_t size;          /**< Total buffer size */
    volatile size_t head; /**< Write position (next byte to write) */
    volatile size_t tail; /**< Read position (next byte to read/DMA) */
  } FEB_UART_RingBuffer_t;

  /**
   * @brief Initialize ring buffer
   */
  static inline void feb_uart_ring_init(FEB_UART_RingBuffer_t *rb, uint8_t *buffer, size_t size)
  {
    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
  }

  /**
   * @brief Get number of bytes available to read
   */
  static inline size_t feb_uart_ring_count(const FEB_UART_RingBuffer_t *rb)
  {
    size_t head = rb->head;
    size_t tail = rb->tail;
    if (head >= tail)
    {
      return head - tail;
    }
    return rb->size - tail + head;
  }

  /**
   * @brief Get number of bytes available to write
   */
  static inline size_t feb_uart_ring_space(const FEB_UART_RingBuffer_t *rb)
  {
    return rb->size - feb_uart_ring_count(rb) - 1;
  }

  /**
   * @brief Check if ring buffer is empty
   */
  static inline bool feb_uart_ring_empty(const FEB_UART_RingBuffer_t *rb)
  {
    return rb->head == rb->tail;
  }

  /**
   * @brief Check if ring buffer is full
   */
  static inline bool feb_uart_ring_full(const FEB_UART_RingBuffer_t *rb)
  {
    return feb_uart_ring_space(rb) == 0;
  }

  /**
   * @brief Write bytes to ring buffer
   * @return Number of bytes actually written
   * @note Caller must hold lock if multi-producer
   */
  static inline size_t feb_uart_ring_write(FEB_UART_RingBuffer_t *rb, const uint8_t *data, size_t len)
  {
    size_t space = feb_uart_ring_space(rb);
    if (len > space)
    {
      len = space;
    }

    size_t written = 0;
    while (written < len)
    {
      rb->buffer[rb->head] = data[written];
      rb->head = (rb->head + 1) % rb->size;
      written++;
    }

    return written;
  }

  /**
   * @brief Read bytes from ring buffer
   * @return Number of bytes actually read
   */
  static inline size_t feb_uart_ring_read(FEB_UART_RingBuffer_t *rb, uint8_t *data, size_t len)
  {
    size_t count = feb_uart_ring_count(rb);
    if (len > count)
    {
      len = count;
    }

    size_t read = 0;
    while (read < len)
    {
      data[read] = rb->buffer[rb->tail];
      rb->tail = (rb->tail + 1) % rb->size;
      read++;
    }

    return read;
  }

  /**
   * @brief Peek at bytes without removing them
   * @return Number of bytes peeked
   */
  static inline size_t feb_uart_ring_peek(const FEB_UART_RingBuffer_t *rb, uint8_t *data, size_t len)
  {
    size_t count = feb_uart_ring_count(rb);
    if (len > count)
    {
      len = count;
    }

    size_t pos = rb->tail;
    for (size_t i = 0; i < len; i++)
    {
      data[i] = rb->buffer[pos];
      pos = (pos + 1) % rb->size;
    }

    return len;
  }

  /**
   * @brief Get contiguous read length from tail
   * @return Number of contiguous bytes available from tail position
   * @note Useful for DMA transfers that can't wrap
   */
  static inline size_t feb_uart_ring_contig_read_len(const FEB_UART_RingBuffer_t *rb)
  {
    size_t head = rb->head;
    size_t tail = rb->tail;

    if (head >= tail)
    {
      return head - tail;
    }
    else
    {
      return rb->size - tail; /* Data wraps, return to end of buffer */
    }
  }

  /**
   * @brief Advance tail by specified amount
   * @note Call after DMA transfer completes
   */
  static inline void feb_uart_ring_advance_tail(FEB_UART_RingBuffer_t *rb, size_t len)
  {
    rb->tail = (rb->tail + len) % rb->size;
  }

  /* ============================================================================
   * TX State Machine
   * ============================================================================ */

  typedef enum
  {
    FEB_UART_TX_IDLE = 0,   /**< No DMA transfer in progress */
    FEB_UART_TX_DMA_ACTIVE, /**< DMA transfer in progress */
  } FEB_UART_TxState_t;

  /* ============================================================================
   * Line Buffer for RX Parsing
   * ============================================================================ */

  typedef struct
  {
    char buffer[FEB_UART_DEFAULT_LINE_BUFFER_SIZE];
    size_t len;
  } FEB_UART_LineBuffer_t;

#ifdef __cplusplus
}
#endif

#endif /* FEB_UART_INTERNAL_H */
