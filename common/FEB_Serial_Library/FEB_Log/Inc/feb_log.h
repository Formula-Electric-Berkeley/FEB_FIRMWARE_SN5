/**
 ******************************************************************************
 * @file           : feb_log.h
 * @brief          : FEB Log Library - Standalone Logging with Configurable Output
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides logging macros with:
 *   - Multiple severity levels (ERROR, WARN, INFO, DEBUG, TRACE)
 *   - Compile-time elimination for zero overhead when disabled
 *   - ANSI color-coded output for serial terminals
 *   - Timestamp integration
 *   - Module tagging for easy identification
 *   - File/line information for errors and warnings
 *   - Configurable output backend (UART, USB, RTT, etc.)
 *
 * Usage:
 *   // Initialize with output function
 *   static int my_output(const char *data, size_t len) {
 *       return FEB_UART_Write(FEB_UART_INSTANCE_1, (const uint8_t *)data, len);
 *   }
 *   FEB_Log_Init(my_output, HAL_GetTick, FEB_LOG_INFO);
 *
 *   // Use logging macros
 *   LOG_I(TAG_MAIN, "System initialized");
 *   LOG_E(TAG_ADC, "Failed to read channel %d", channel);
 *   LOG_W(TAG_CAN, "Message queue 80%% full");
 *   LOG_D(TAG_MAIN, "APPS: %.1f%%", apps_percent);
 *   LOG_T(TAG_BMS, "Cell %d voltage: %dmV", i, mv);
 *
 * Compile-time configuration:
 *   - FEB_LOG_COMPILE_LEVEL: Messages above this are compiled out
 *   - Set to 0 (NONE) for production builds to eliminate all overhead
 *
 * Runtime configuration:
 *   - FEB_Log_SetLevel(): Filter messages at runtime
 *
 ******************************************************************************
 */

#ifndef FEB_LOG_H
#define FEB_LOG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_log_config.h"
#include "feb_uart.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

  /* ============================================================================
   * FreeRTOS Sync Primitive Types
   * ============================================================================ */
#if FEB_LOG_USE_FREERTOS
#include "cmsis_os2.h"
  typedef osMutexId_t FEB_Log_MutexHandle_t;
#else
typedef void *FEB_Log_MutexHandle_t;
#endif

  /* ============================================================================
   * Log Level Enumeration
   * ============================================================================ */

  /**
   * @brief Log verbosity levels
   *
   * Lower values = higher priority. Messages are filtered based on runtime
   * log level setting. Compile-time log level (FEB_LOG_COMPILE_LEVEL)
   * can eliminate code for higher levels entirely.
   */
  typedef enum
  {
    FEB_LOG_NONE = 0,  /**< No logging output */
    FEB_LOG_ERROR = 1, /**< Critical errors only */
    FEB_LOG_WARN = 2,  /**< Warnings and errors */
    FEB_LOG_INFO = 3,  /**< Informational messages */
    FEB_LOG_DEBUG = 4, /**< Debug-level output */
    FEB_LOG_TRACE = 5, /**< Verbose trace output */
  } FEB_Log_Level_t;

  /* ============================================================================
   * Output Function Type
   * ============================================================================ */

  /**
   * @brief User-provided output function signature (for custom backends)
   *
   * @param data  Pointer to data to output
   * @param len   Length of data in bytes
   * @return Number of bytes written, or negative on error
   *
   * @note Must be thread-safe if called from multiple contexts
   */
  typedef int (*FEB_Log_OutputFunc_t)(const char *data, size_t len);

  /* ============================================================================
   * Configuration Structure
   * ============================================================================ */

  /**
   * @brief Log configuration structure
   *
   * Provides a clean initialization pattern matching other FEB libraries.
   * Either use uart_instance for built-in UART routing, or provide a
   * custom_output callback for other backends (USB, RTT, file, etc.)
   *
   * FreeRTOS Mode (FEB_LOG_USE_FREERTOS == 1):
   *   - mutex is REQUIRED for thread-safe output (created in CubeMX .ioc)
   *   - Pass NULL mutex at your own risk (output corruption possible)
   *
   * Bare-Metal Mode (FEB_LOG_USE_FREERTOS == 0):
   *   - mutex field is ignored
   */
  typedef struct
  {
    FEB_UART_Instance_t uart_instance;  /**< UART instance for output (default: 0) */
    FEB_Log_Level_t level;              /**< Initial runtime log level */
    bool colors;                        /**< Enable ANSI colors */
    bool timestamps;                    /**< Enable timestamps */
    uint32_t (*get_tick_ms)(void);      /**< Timestamp function (optional, NULL = no timestamps) */
    FEB_Log_OutputFunc_t custom_output; /**< Custom output callback (optional, overrides uart_instance) */

#if FEB_LOG_USE_FREERTOS
    /** @brief Mutex handle - REQUIRED in FreeRTOS mode. Create in CubeMX .ioc. */
    FEB_Log_MutexHandle_t mutex;
#endif
  } FEB_Log_Config_t;

  /* ============================================================================
   * Initialization API
   * ============================================================================ */

  /**
   * @brief Initialize the logging system with configuration
   *
   * @param config Pointer to configuration structure
   * @return 0 on success, -1 on error
   *
   * @note If custom_output is NULL, uses built-in UART routing via uart_instance
   * @note If custom_output is provided, it overrides uart_instance
   *
   * Example (simple):
   * @code
   *   FEB_Log_Config_t cfg = {
   *       .uart_instance = FEB_UART_INSTANCE_1,
   *       .level = FEB_LOG_INFO,
   *       .colors = true,
   *       .timestamps = true,
   *       .get_tick_ms = HAL_GetTick,
   *   };
   *   FEB_Log_Init(&cfg);
   * @endcode
   */
  int FEB_Log_Init(const FEB_Log_Config_t *config);

  /**
   * @brief Check if logging system is initialized
   *
   * @return true if initialized, false otherwise
   */
  bool FEB_Log_IsInitialized(void);

  /* ============================================================================
   * Runtime Configuration API
   * ============================================================================ */

  /**
   * @brief Set runtime log verbosity level
   *
   * @param level New log level (FEB_LOG_NONE to FEB_LOG_TRACE)
   */
  void FEB_Log_SetLevel(FEB_Log_Level_t level);

  /**
   * @brief Get current runtime log level
   *
   * @return Current log level
   */
  FEB_Log_Level_t FEB_Log_GetLevel(void);

  /**
   * @brief Enable or disable ANSI color codes
   *
   * @param enable true to enable colors, false to disable
   */
  void FEB_Log_SetColors(bool enable);

  /**
   * @brief Check if colors are enabled
   *
   * @return true if colors enabled, false otherwise
   */
  bool FEB_Log_GetColors(void);

  /**
   * @brief Enable or disable timestamps
   *
   * @param enable true to enable timestamps, false to disable
   */
  void FEB_Log_SetTimestamps(bool enable);

  /**
   * @brief Check if timestamps are enabled
   *
   * @return true if timestamps enabled, false otherwise
   */
  bool FEB_Log_GetTimestamps(void);

  /* ============================================================================
   * Module Tags
   * ============================================================================
   *
   * Standard module tags for consistent log output.
   * Define additional tags in your application as needed.
   */

#define TAG_MAIN "[MAIN]"
#define TAG_ADC "[ADC]"
#define TAG_CAN "[CAN]"
#define TAG_RMS "[RMS]"
#define TAG_BMS "[BMS]"
#define TAG_BSPD "[BSPD]"
#define TAG_TPS "[TPS]"
#define TAG_UART "[UART]"
#define TAG_I2C "[I2C]"
#define TAG_SPI "[SPI]"
#define TAG_DMA "[DMA]"
#define TAG_PWM "[PWM]"
#define TAG_GPIO "[GPIO]"
#define TAG_LOG "[LOG]"

  /* ============================================================================
   * Internal Logging Function
   * ============================================================================ */

  /**
   * @brief Internal logging function - do not call directly
   *
   * Use the LOG_E, LOG_W, LOG_I, LOG_D, LOG_T macros instead.
   *
   * @param level   Log level
   * @param tag     Module tag (e.g., "[MAIN]")
   * @param file    Source file name (for ERROR/WARN, NULL otherwise)
   * @param line    Source line number (for ERROR/WARN, 0 otherwise)
   * @param format  Printf format string
   * @param ...     Variable arguments
   */
  void FEB_Log_Output(FEB_Log_Level_t level, const char *tag, const char *file, int line, const char *format, ...)
      __attribute__((format(printf, 5, 6)));

  /**
   * @brief Raw output without formatting
   *
   * Outputs directly without timestamp, tag, or color.
   * Useful for banners, tables, or custom formatting.
   *
   * @param format  Printf format string
   * @param ...     Variable arguments
   * @return Number of bytes written, or negative on error
   */
  int FEB_Log_Raw(const char *format, ...) __attribute__((format(printf, 1, 2)));

  /**
   * @brief Hexdump of data
   *
   * Outputs data as hex bytes.
   *
   * @param tag    Module tag
   * @param data   Pointer to data
   * @param len    Length in bytes
   */
  void FEB_Log_Hexdump(const char *tag, const uint8_t *data, size_t len);

  /* ============================================================================
   * Logging Macros
   * ============================================================================
   *
   * These macros provide compile-time elimination when the log level is
   * above FEB_LOG_COMPILE_LEVEL. At runtime, messages are filtered
   * against the level set by FEB_Log_SetLevel().
   */

#if FEB_LOG_COMPILE_LEVEL >= 1 /* ERROR */
/**
 * @brief LOG_E - Error level logging
 *
 * For critical errors that should always be visible.
 * Includes file and line information.
 * Color: Red + Bold
 *
 * @param tag Module tag (e.g., TAG_MAIN)
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
#define LOG_E(tag, fmt, ...) FEB_Log_Output(FEB_LOG_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_E(tag, fmt, ...) ((void)0)
#endif

#if FEB_LOG_COMPILE_LEVEL >= 2 /* WARN */
/**
 * @brief LOG_W - Warning level logging
 *
 * For recoverable issues that should be investigated.
 * Includes file and line information.
 * Color: Yellow + Bold
 *
 * @param tag Module tag (e.g., TAG_CAN)
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
#define LOG_W(tag, fmt, ...) FEB_Log_Output(FEB_LOG_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_W(tag, fmt, ...) ((void)0)
#endif

#if FEB_LOG_COMPILE_LEVEL >= 3 /* INFO */
/**
 * @brief LOG_I - Info level logging
 *
 * For important status updates and milestones.
 * Does not include file/line info.
 * Color: Cyan
 *
 * @param tag Module tag (e.g., TAG_ADC)
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
#define LOG_I(tag, fmt, ...) FEB_Log_Output(FEB_LOG_INFO, tag, NULL, 0, fmt, ##__VA_ARGS__)
#else
#define LOG_I(tag, fmt, ...) ((void)0)
#endif

#if FEB_LOG_COMPILE_LEVEL >= 4 /* DEBUG */
/**
 * @brief LOG_D - Debug level logging
 *
 * For development and debugging information.
 * Does not include file/line info.
 * Color: Magenta
 *
 * @param tag Module tag (e.g., TAG_RMS)
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
#define LOG_D(tag, fmt, ...) FEB_Log_Output(FEB_LOG_DEBUG, tag, NULL, 0, fmt, ##__VA_ARGS__)
#else
#define LOG_D(tag, fmt, ...) ((void)0)
#endif

#if FEB_LOG_COMPILE_LEVEL >= 5 /* TRACE */
/**
 * @brief LOG_T - Trace level logging
 *
 * For verbose output during detailed debugging.
 * Does not include file/line info.
 * Color: Dim/Gray
 *
 * @param tag Module tag (e.g., TAG_BMS)
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
#define LOG_T(tag, fmt, ...) FEB_Log_Output(FEB_LOG_TRACE, tag, NULL, 0, fmt, ##__VA_ARGS__)
#else
#define LOG_T(tag, fmt, ...) ((void)0)
#endif

/**
 * @brief LOG_RAW - Raw output without formatting
 *
 * Outputs directly without timestamp, tag, or color.
 * Useful for banners, tables, or custom formatting.
 * Always compiled in regardless of log level.
 *
 * @param fmt Printf-style format string
 * @param ... Variable arguments
 */
#define LOG_RAW(fmt, ...) FEB_Log_Raw(fmt, ##__VA_ARGS__)

/**
 * @brief Log a hexdump of data
 *
 * Outputs data as hex bytes.
 *
 * @param tag    Module tag
 * @param data   Pointer to data
 * @param len    Length in bytes
 */
#define LOG_HEXDUMP(tag, data, len) FEB_Log_Hexdump(tag, data, len)

/**
 * @brief Assert with logging
 *
 * If condition is false, logs an error.
 *
 * @param cond Condition to check
 * @param msg  Message to log if false
 */
#define LOG_ASSERT(cond, msg)                                                                                          \
  do                                                                                                                   \
  {                                                                                                                    \
    if (!(cond))                                                                                                       \
    {                                                                                                                  \
      LOG_E(TAG_MAIN, "ASSERT FAILED: %s (%s:%d)", msg, __FILE__, __LINE__);                                           \
    }                                                                                                                  \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif /* FEB_LOG_H */
