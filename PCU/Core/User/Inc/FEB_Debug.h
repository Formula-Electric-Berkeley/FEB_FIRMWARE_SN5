/**
 ******************************************************************************
 * @file           : FEB_Debug.h
 * @brief          : Debug logging and diagnostic macros for PCU firmware
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides a comprehensive logging system with:
 * - Multiple severity levels (INFO, WARNING, ERROR, DEBUG)
 * - Conditional compilation for zero overhead when disabled
 * - ANSI color-coded output for serial terminals
 * - Timestamp integration with HAL_GetTick()
 * - Module tagging for easy identification
 * - File/line information for error tracking
 * - Per-module debug level configuration
 *
 * Usage:
 *   LOG_I(TAG_MAIN, "System initialized");
 *   LOG_E(TAG_ADC, "Failed to read channel %d", channel);
 *   LOG_W(TAG_CAN, "Message queue full");
 *   LOG_D(TAG_MAIN, "APPS: %.1f%%", apps_percent);
 *
 * To enable logging, define DEBUG_ENABLE in your build configuration.
 * When disabled, all macros compile to no-ops with zero overhead.
 *
 ******************************************************************************
 */

#ifndef FEB_DEBUG_H
#define FEB_DEBUG_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "stm32f4xx_hal.h"

  /* Exported defines ----------------------------------------------------------*/

  /**
   * @brief Debug level enumeration
   */
  typedef enum
  {
    DEBUG_LEVEL_NONE = 0,  /**< No debug output */
    DEBUG_LEVEL_ERROR = 1, /**< Only errors */
    DEBUG_LEVEL_WARN = 2,  /**< Warnings and errors */
    DEBUG_LEVEL_INFO = 3,  /**< Info, warnings, and errors */
    DEBUG_LEVEL_DEBUG = 4  /**< All debug output including verbose */
  } FEB_DebugLevel_t;

/**
 * @brief Global debug level - controls what gets printed
 * Set to DEBUG_LEVEL_DEBUG to see all LOG_D() messages
 */
#ifndef FEB_DEBUG_GLOBAL_LEVEL
#define FEB_DEBUG_GLOBAL_LEVEL DEBUG_LEVEL_DEBUG
#endif

/**
 * @brief Per-module debug levels
 * Override these to control verbosity per module
 */
#ifndef DEBUG_LEVEL_MAIN
#define DEBUG_LEVEL_MAIN DEBUG_LEVEL_DEBUG
#endif

#ifndef DEBUG_LEVEL_ADC
#define DEBUG_LEVEL_ADC DEBUG_LEVEL_DEBUG
#endif

#ifndef DEBUG_LEVEL_CAN
#define DEBUG_LEVEL_CAN DEBUG_LEVEL_DEBUG
#endif

#ifndef DEBUG_LEVEL_RMS
#define DEBUG_LEVEL_RMS DEBUG_LEVEL_DEBUG
#endif

#ifndef DEBUG_LEVEL_BMS
#define DEBUG_LEVEL_BMS DEBUG_LEVEL_NONE
#endif

#ifndef DEBUG_LEVEL_BSPD
#define DEBUG_LEVEL_BSPD DEBUG_LEVEL_DEBUG
#endif

#ifndef DEBUG_LEVEL_TPS
#define DEBUG_LEVEL_TPS DEBUG_LEVEL_DEBUG
#endif

/**
 * @brief Module tag definitions
 * Used to identify the source of log messages
 */
#define TAG_MAIN "[MAIN]"
#define TAG_ADC "[ADC]"
#define TAG_CAN "[CAN]"
#define TAG_RMS "[RMS]"
#define TAG_BMS "[BMS]"
#define TAG_BSPD "[BSPD]"
#define TAG_TPS "[TPS]"

/**
 * @brief ANSI color codes for terminal output
 */
#ifdef DEBUG_ENABLE_COLORS
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_BOLD "\x1b[1m"
#else
#define ANSI_COLOR_RED ""
#define ANSI_COLOR_GREEN ""
#define ANSI_COLOR_YELLOW ""
#define ANSI_COLOR_BLUE ""
#define ANSI_COLOR_MAGENTA ""
#define ANSI_COLOR_CYAN ""
#define ANSI_COLOR_RESET ""
#define ANSI_BOLD ""
#endif

  /* Logging Macros ------------------------------------------------------------*/

#ifdef DEBUG_ENABLE

/**
 * @brief Internal log formatting macro with timestamp and color
 */
#define LOG_FORMAT(color, level, tag, fmt) color "[%lu] %s " level ": " fmt ANSI_COLOR_RESET "\r\n"

/**
 * @brief Internal log formatting macro with file/line info
 */
#define LOG_FORMAT_LOC(color, level, tag, fmt, file, line)                                                             \
  color "[%lu] %s " level " (%s:%d): " fmt ANSI_COLOR_RESET "\r\n"

/**
 * @brief LOG_I - Info level logging
 * @param tag Module tag (e.g., TAG_MAIN, TAG_ADC)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
#define LOG_I(tag, fmt, ...)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    if (FEB_DEBUG_GLOBAL_LEVEL >= DEBUG_LEVEL_INFO)                                                                    \
    {                                                                                                                  \
      printf(LOG_FORMAT(ANSI_COLOR_CYAN, "INFO", tag, fmt), HAL_GetTick(), tag, ##__VA_ARGS__);                        \
    }                                                                                                                  \
  } while (0)

/**
 * @brief LOG_W - Warning level logging with file/line info
 * @param tag Module tag (e.g., TAG_MAIN, TAG_ADC)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
#define LOG_W(tag, fmt, ...)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    if (FEB_DEBUG_GLOBAL_LEVEL >= DEBUG_LEVEL_WARN)                                                                    \
    {                                                                                                                  \
      printf(LOG_FORMAT_LOC(ANSI_COLOR_YELLOW ANSI_BOLD, "WARN", tag, fmt, __FILE__, __LINE__), HAL_GetTick(), tag,    \
             __FILE__, __LINE__, ##__VA_ARGS__);                                                                       \
    }                                                                                                                  \
  } while (0)

/**
 * @brief LOG_E - Error level logging with file/line info
 * @param tag Module tag (e.g., TAG_MAIN, TAG_ADC)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
#define LOG_E(tag, fmt, ...)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    if (FEB_DEBUG_GLOBAL_LEVEL >= DEBUG_LEVEL_ERROR)                                                                   \
    {                                                                                                                  \
      printf(LOG_FORMAT_LOC(ANSI_COLOR_RED ANSI_BOLD, "ERROR", tag, fmt, __FILE__, __LINE__), HAL_GetTick(), tag,      \
             __FILE__, __LINE__, ##__VA_ARGS__);                                                                       \
    }                                                                                                                  \
  } while (0)

/**
 * @brief LOG_D - Debug level logging
 * @param tag Module tag (e.g., TAG_MAIN, TAG_ADC)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
#define LOG_D(tag, fmt, ...)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    if (FEB_DEBUG_GLOBAL_LEVEL >= DEBUG_LEVEL_DEBUG)                                                                   \
    {                                                                                                                  \
      printf(LOG_FORMAT(ANSI_COLOR_MAGENTA, "DEBUG", tag, fmt), HAL_GetTick(), tag, ##__VA_ARGS__);                    \
    }                                                                                                                  \
  } while (0)

/**
 * @brief LOG_RAW - Raw printf without formatting (for banners, etc.)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
#define LOG_RAW(fmt, ...) printf(fmt, ##__VA_ARGS__)

#else /* DEBUG_ENABLE not defined */

/**
 * @brief When DEBUG_ENABLE is not defined, all macros become no-ops
 * This ensures zero overhead in production builds
 */
#define LOG_I(tag, fmt, ...) ((void)0)
#define LOG_W(tag, fmt, ...) ((void)0)
#define LOG_E(tag, fmt, ...) ((void)0)
#define LOG_D(tag, fmt, ...) ((void)0)
#define LOG_RAW(fmt, ...) ((void)0)

#endif /* DEBUG_ENABLE */

  /* Exported functions --------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* FEB_DEBUG_H */
