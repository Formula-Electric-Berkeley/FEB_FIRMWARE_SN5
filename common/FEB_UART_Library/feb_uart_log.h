/**
 ******************************************************************************
 * @file           : feb_uart_log.h
 * @brief          : Logging macros for FEB UART Library
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
 *
 * Usage:
 *   LOG_I(TAG_MAIN, "System initialized");
 *   LOG_E(TAG_ADC, "Failed to read channel %d", channel);
 *   LOG_W(TAG_CAN, "Message queue 80%% full");
 *   LOG_D(TAG_MAIN, "APPS: %.1f%%", apps_percent);
 *   LOG_T(TAG_BMS, "Cell %d voltage: %dmV", i, mv);
 *
 * Compile-time configuration:
 *   - FEB_UART_COMPILE_LOG_LEVEL: Messages above this are compiled out
 *   - Set to 0 (NONE) for production builds to eliminate all overhead
 *
 * Runtime configuration:
 *   - FEB_UART_SetLogLevel(): Filter messages at runtime
 *
 ******************************************************************************
 */

#ifndef FEB_UART_LOG_H
#define FEB_UART_LOG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_uart.h"
#include "feb_uart_config.h"

/* ============================================================================
 * Module Tags
 * ============================================================================
 *
 * Standard module tags for consistent log output.
 * Define additional tags in your application as needed.
 */

#define TAG_MAIN "[MAIN]"
#define TAG_ADC  "[ADC]"
#define TAG_CAN  "[CAN]"
#define TAG_RMS  "[RMS]"
#define TAG_BMS  "[BMS]"
#define TAG_BSPD "[BSPD]"
#define TAG_TPS  "[TPS]"
#define TAG_UART "[UART]"
#define TAG_I2C  "[I2C]"
#define TAG_SPI  "[SPI]"
#define TAG_DMA  "[DMA]"
#define TAG_PWM  "[PWM]"
#define TAG_GPIO "[GPIO]"
#define TAG_PUMP "[PUMP]"
#define TAG_FAN  "[FAN]"

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
void FEB_UART_Log(FEB_UART_LogLevel_t level,
                  const char *tag,
                  const char *file,
                  int line,
                  const char *format,
                  ...) __attribute__((format(printf, 5, 6)));

/* ============================================================================
 * Logging Macros
 * ============================================================================
 *
 * These macros provide compile-time elimination when the log level is
 * above FEB_UART_COMPILE_LOG_LEVEL. At runtime, messages are filtered
 * against the level set by FEB_UART_SetLogLevel().
 */

#if FEB_UART_COMPILE_LOG_LEVEL >= 1 /* ERROR */
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
#define LOG_E(tag, fmt, ...) \
  FEB_UART_Log(FEB_UART_LOG_ERROR, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_E(tag, fmt, ...) ((void)0)
#endif

#if FEB_UART_COMPILE_LOG_LEVEL >= 2 /* WARN */
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
#define LOG_W(tag, fmt, ...) \
  FEB_UART_Log(FEB_UART_LOG_WARN, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_W(tag, fmt, ...) ((void)0)
#endif

#if FEB_UART_COMPILE_LOG_LEVEL >= 3 /* INFO */
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
#define LOG_I(tag, fmt, ...) \
  FEB_UART_Log(FEB_UART_LOG_INFO, tag, NULL, 0, fmt, ##__VA_ARGS__)
#else
#define LOG_I(tag, fmt, ...) ((void)0)
#endif

#if FEB_UART_COMPILE_LOG_LEVEL >= 4 /* DEBUG */
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
#define LOG_D(tag, fmt, ...) \
  FEB_UART_Log(FEB_UART_LOG_DEBUG, tag, NULL, 0, fmt, ##__VA_ARGS__)
#else
#define LOG_D(tag, fmt, ...) ((void)0)
#endif

#if FEB_UART_COMPILE_LOG_LEVEL >= 5 /* TRACE */
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
#define LOG_T(tag, fmt, ...) \
  FEB_UART_Log(FEB_UART_LOG_TRACE, tag, NULL, 0, fmt, ##__VA_ARGS__)
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
#define LOG_RAW(fmt, ...) FEB_UART_Printf(fmt, ##__VA_ARGS__)

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

/**
 * @brief Log a hexdump of data
 *
 * Outputs data as hex bytes with optional ASCII representation.
 *
 * @param tag    Module tag
 * @param data   Pointer to data
 * @param len    Length in bytes
 */
#define LOG_HEXDUMP(tag, data, len) FEB_UART_LogHexdump(tag, data, len)

/**
 * @brief Internal hexdump function
 */
void FEB_UART_LogHexdump(const char *tag, const uint8_t *data, size_t len);

/**
 * @brief Assert with logging
 *
 * If condition is false, logs an error and optionally halts.
 *
 * @param cond Condition to check
 * @param msg  Message to log if false
 */
#define LOG_ASSERT(cond, msg)                                                 \
  do                                                                          \
  {                                                                           \
    if (!(cond))                                                              \
    {                                                                         \
      LOG_E(TAG_MAIN, "ASSERT FAILED: %s (%s:%d)", msg, __FILE__, __LINE__);  \
    }                                                                         \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif /* FEB_UART_LOG_H */
