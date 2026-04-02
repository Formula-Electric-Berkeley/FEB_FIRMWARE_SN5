/**
 ******************************************************************************
 * @file           : feb_log_config.h
 * @brief          : Configuration defaults for FEB Log Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * User-configurable defaults for log formatting, buffer sizes, and ANSI colors.
 * Override these by defining the macros before including this file.
 *
 ******************************************************************************
 */

#ifndef FEB_LOG_CONFIG_H
#define FEB_LOG_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* ============================================================================
   * Buffer Size Defaults
   * ============================================================================ */

#ifndef FEB_LOG_STAGING_BUFFER_SIZE
#define FEB_LOG_STAGING_BUFFER_SIZE 512
#endif

  /* ============================================================================
   * FreeRTOS Detection
   * ============================================================================
   *
   * Auto-detect FreeRTOS by checking for common FreeRTOS config macros.
   * Can be overridden by defining FEB_LOG_USE_FREERTOS before including.
   */

#ifndef FEB_LOG_USE_FREERTOS
#if ((defined(INCLUDE_xSemaphoreGetMutexHolder) && (INCLUDE_xSemaphoreGetMutexHolder != 0)) ||                         \
     (defined(configUSE_MUTEXES) && (configUSE_MUTEXES != 0)) || (defined(USE_FREERTOS) && (USE_FREERTOS != 0)))
#define FEB_LOG_USE_FREERTOS 1
#else
#define FEB_LOG_USE_FREERTOS 0
#endif
#endif

  /* ============================================================================
   * Bare-Metal Safety Mode
   * ============================================================================
   *
   * When FreeRTOS is NOT detected and FORCE_BARE_METAL is NOT set:
   *   - Mutex operations are NO-OPs (safe default for single-threaded use)
   *
   * When FORCE_BARE_METAL is explicitly set to 1:
   *   - Mutex operations use __disable_irq() / __enable_irq() critical sections
   *   - Use this for bare-metal projects that need ISR protection
   */

#ifndef FEB_LOG_FORCE_BARE_METAL
#define FEB_LOG_FORCE_BARE_METAL 0
#endif

  /* ============================================================================
   * ANSI Color Codes
   * ============================================================================ */

#define FEB_LOG_ANSI_RED "\x1b[31m"
#define FEB_LOG_ANSI_GREEN "\x1b[32m"
#define FEB_LOG_ANSI_YELLOW "\x1b[33m"
#define FEB_LOG_ANSI_BLUE "\x1b[34m"
#define FEB_LOG_ANSI_MAGENTA "\x1b[35m"
#define FEB_LOG_ANSI_CYAN "\x1b[36m"
#define FEB_LOG_ANSI_WHITE "\x1b[37m"
#define FEB_LOG_ANSI_RESET "\x1b[0m"
#define FEB_LOG_ANSI_BOLD "\x1b[1m"
#define FEB_LOG_ANSI_DIM "\x1b[2m"

  /* ============================================================================
   * Log Level Colors
   * ============================================================================ */

#define FEB_LOG_COLOR_ERROR FEB_LOG_ANSI_RED FEB_LOG_ANSI_BOLD
#define FEB_LOG_COLOR_WARN FEB_LOG_ANSI_YELLOW FEB_LOG_ANSI_BOLD
#define FEB_LOG_COLOR_INFO FEB_LOG_ANSI_CYAN
#define FEB_LOG_COLOR_DEBUG FEB_LOG_ANSI_MAGENTA
#define FEB_LOG_COLOR_TRACE FEB_LOG_ANSI_DIM

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

#ifndef FEB_LOG_COMPILE_LEVEL
#ifdef DEBUG
#define FEB_LOG_COMPILE_LEVEL 4 /* DEBUG in debug builds */
#else
#define FEB_LOG_COMPILE_LEVEL 2 /* WARN in release builds */
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* FEB_LOG_CONFIG_H */
