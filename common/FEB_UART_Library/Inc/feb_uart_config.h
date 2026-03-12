/**
 ******************************************************************************
 * @file           : feb_uart_config.h
 * @brief          : Configuration defaults for FEB UART Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * User-configurable defaults for buffer sizes, timeouts, and ANSI color codes.
 * Override these by defining the macros before including this file.
 *
 ******************************************************************************
 */

#ifndef FEB_UART_CONFIG_H
#define FEB_UART_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* ============================================================================
   * Buffer Size Defaults
   * ============================================================================ */

#ifndef FEB_UART_DEFAULT_TX_BUFFER_SIZE
#define FEB_UART_DEFAULT_TX_BUFFER_SIZE 512
#endif

#ifndef FEB_UART_DEFAULT_RX_BUFFER_SIZE
#define FEB_UART_DEFAULT_RX_BUFFER_SIZE 256
#endif

#ifndef FEB_UART_DEFAULT_LINE_BUFFER_SIZE
#define FEB_UART_DEFAULT_LINE_BUFFER_SIZE 128
#endif

#ifndef FEB_UART_STAGING_BUFFER_SIZE
#define FEB_UART_STAGING_BUFFER_SIZE 512
#endif

  /* ============================================================================
   * Timeout Defaults
   * ============================================================================ */

#ifndef FEB_UART_TX_TIMEOUT_MS
#define FEB_UART_TX_TIMEOUT_MS 100
#endif

#ifndef FEB_UART_FLUSH_TIMEOUT_MS
#define FEB_UART_FLUSH_TIMEOUT_MS 1000
#endif

  /* ============================================================================
   * FreeRTOS Detection
   * ============================================================================
   *
   * Auto-detect FreeRTOS by checking for common FreeRTOS config macros.
   * Can be overridden by defining FEB_UART_USE_FREERTOS before including.
   */

#ifndef FEB_UART_USE_FREERTOS
#if defined(INCLUDE_xSemaphoreGetMutexHolder) || defined(configUSE_MUTEXES) || defined(USE_FREERTOS)
#define FEB_UART_USE_FREERTOS 1
#else
#define FEB_UART_USE_FREERTOS 0
#endif
#endif

  /* ============================================================================
   * Queue Support (FreeRTOS only)
   * ============================================================================
   *
   * Enable FreeRTOS message queue support for RX/TX.
   * Auto-enabled when FreeRTOS is detected, can be overridden.
   */

#ifndef FEB_UART_ENABLE_QUEUES
#if FEB_UART_USE_FREERTOS
#define FEB_UART_ENABLE_QUEUES 1
#else
#define FEB_UART_ENABLE_QUEUES 0
#endif
#endif

  /* Queue size defaults */
#ifndef FEB_UART_RX_QUEUE_DEPTH
#define FEB_UART_RX_QUEUE_DEPTH 8
#endif

#ifndef FEB_UART_TX_QUEUE_DEPTH
#define FEB_UART_TX_QUEUE_DEPTH 4
#endif

#ifndef FEB_UART_QUEUE_LINE_SIZE
#define FEB_UART_QUEUE_LINE_SIZE FEB_UART_DEFAULT_LINE_BUFFER_SIZE
#endif

#ifndef FEB_UART_TX_QUEUE_MSG_SIZE
#define FEB_UART_TX_QUEUE_MSG_SIZE FEB_UART_STAGING_BUFFER_SIZE
#endif

  /* ============================================================================
   * Multi-Instance Support
   * ============================================================================
   *
   * Maximum number of UART instances that can be used simultaneously.
   * Each instance has independent TX/RX buffers, callbacks, and queues.
   */

#ifndef FEB_UART_MAX_INSTANCES
#define FEB_UART_MAX_INSTANCES 2
#endif

  /* ============================================================================
   * ANSI Color Codes
   * ============================================================================ */

#define FEB_UART_ANSI_RED "\x1b[31m"
#define FEB_UART_ANSI_GREEN "\x1b[32m"
#define FEB_UART_ANSI_YELLOW "\x1b[33m"
#define FEB_UART_ANSI_BLUE "\x1b[34m"
#define FEB_UART_ANSI_MAGENTA "\x1b[35m"
#define FEB_UART_ANSI_CYAN "\x1b[36m"
#define FEB_UART_ANSI_WHITE "\x1b[37m"
#define FEB_UART_ANSI_RESET "\x1b[0m"
#define FEB_UART_ANSI_BOLD "\x1b[1m"
#define FEB_UART_ANSI_DIM "\x1b[2m"

  /* ============================================================================
   * Log Level Colors
   * ============================================================================ */

#define FEB_UART_COLOR_ERROR FEB_UART_ANSI_RED FEB_UART_ANSI_BOLD
#define FEB_UART_COLOR_WARN FEB_UART_ANSI_YELLOW FEB_UART_ANSI_BOLD
#define FEB_UART_COLOR_INFO FEB_UART_ANSI_CYAN
#define FEB_UART_COLOR_DEBUG FEB_UART_ANSI_MAGENTA
#define FEB_UART_COLOR_TRACE FEB_UART_ANSI_DIM

  /* ============================================================================
   * Compile-Time Log Level
   * ============================================================================
   *
   * Messages above this level are compiled out completely.
   * Set to 0 (NONE) for production builds to eliminate all logging overhead.
   *
   * Levels:
   *   0 = NONE   - No output
   *   1 = ERROR  - Critical errors only
   *   2 = WARN   - Warnings and errors
   *   3 = INFO   - Informational messages
   *   4 = DEBUG  - Debug output
   *   5 = TRACE  - Verbose trace output
   */

#ifndef FEB_UART_COMPILE_LOG_LEVEL
#ifdef DEBUG
#define FEB_UART_COMPILE_LOG_LEVEL 4 /* DEBUG in debug builds */
#else
#define FEB_UART_COMPILE_LOG_LEVEL 2 /* WARN in release builds */
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* FEB_UART_CONFIG_H */
