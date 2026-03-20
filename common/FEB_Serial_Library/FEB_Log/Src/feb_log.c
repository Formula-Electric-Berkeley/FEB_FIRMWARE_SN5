/**
 ******************************************************************************
 * @file           : feb_log.c
 * @brief          : FEB Log Library Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Implements:
 *   - Standalone logging with user-configurable output backend
 *   - Multiple severity levels with compile-time and runtime filtering
 *   - ANSI color codes and timestamps
 *   - Thread-safe output (via FreeRTOS mutex or critical sections)
 *
 ******************************************************************************
 */

#include "feb_log.h"
#include "feb_log_config.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ============================================================================
 * FreeRTOS Abstraction
 * ============================================================================ */

#if FEB_LOG_USE_FREERTOS

#include "FreeRTOS.h"
#include "cmsis_os2.h"

typedef osMutexId_t FEB_Log_Mutex_t;

/* FreeRTOS mode: Use user-provided mutex (NOT created internally) */
#define FEB_LOG_MUTEX_LOCK(m)                                                                                          \
  do                                                                                                                   \
  {                                                                                                                    \
    if ((m) != NULL)                                                                                                   \
      osMutexAcquire(m, osWaitForever);                                                                                \
  } while (0)
#define FEB_LOG_MUTEX_UNLOCK(m)                                                                                        \
  do                                                                                                                   \
  {                                                                                                                    \
    if ((m) != NULL)                                                                                                   \
      osMutexRelease(m);                                                                                               \
  } while (0)
#define FEB_LOG_IN_ISR() (xPortIsInsideInterrupt())

#else /* Bare-metal */

typedef uint32_t FEB_Log_Mutex_t;

/*
 * Bare-metal sync primitive behavior depends on FEB_LOG_FORCE_BARE_METAL:
 *
 * When FORCE_BARE_METAL == 0 (default):
 *   - Mutex operations are NO-OPs (safe for single-threaded use)
 *
 * When FORCE_BARE_METAL == 1 (explicit):
 *   - Uses __disable_irq() / __enable_irq() for critical sections
 */

#if FEB_LOG_FORCE_BARE_METAL && defined(__GNUC__) && (defined(__ARM_ARCH) || defined(__arm__))

static inline uint32_t __get_PRIMASK_local(void)
{
  uint32_t result;
  __asm volatile("MRS %0, primask" : "=r"(result));
  return result;
}
static inline void __set_PRIMASK_local(uint32_t primask)
{
  __asm volatile("MSR primask, %0" : : "r"(primask) : "memory");
}
static inline void __disable_irq_local(void)
{
  __asm volatile("cpsid i" : : : "memory");
}
static inline uint32_t __get_IPSR_local(void)
{
  uint32_t result;
  __asm volatile("MRS %0, ipsr" : "=r"(result));
  return result;
}

#define FEB_LOG_MUTEX_LOCK(m)                                                                                          \
  do                                                                                                                   \
  {                                                                                                                    \
    (m) = __get_PRIMASK_local();                                                                                       \
    __disable_irq_local();                                                                                             \
  } while (0)
#define FEB_LOG_MUTEX_UNLOCK(m) __set_PRIMASK_local(m)
#define FEB_LOG_IN_ISR() ((__get_IPSR_local() & 0xFF) != 0)

#else /* Safe no-op defaults */

#define FEB_LOG_MUTEX_LOCK(m) ((void)0)
#define FEB_LOG_MUTEX_UNLOCK(m) ((void)0)
#define FEB_LOG_IN_ISR() (0)

#endif /* FEB_LOG_FORCE_BARE_METAL */

#endif /* FEB_LOG_USE_FREERTOS */

/* ============================================================================
 * Private Variables
 * ============================================================================ */

static struct
{
  FEB_Log_OutputFunc_t output;
  FEB_UART_Instance_t uart_instance;
  bool use_uart;
  uint32_t (*get_tick_ms)(void);
  FEB_Log_Level_t level;
  bool colors_enabled;
  bool timestamps_enabled;
  bool initialized;
  FEB_Log_Mutex_t mutex;
} log_ctx = {0};

/* ============================================================================
 * Built-in UART Output
 * ============================================================================ */

/**
 * @brief Internal output function that routes to FEB_UART_Write
 */
static int log_uart_output(const char *data, size_t len)
{
  return FEB_UART_Write(log_ctx.uart_instance, (const uint8_t *)data, len);
}

/* Staging buffer for message formatting */
static char staging_buffer[FEB_LOG_STAGING_BUFFER_SIZE];

/* ============================================================================
 * Initialization
 * ============================================================================ */

int FEB_Log_Init(const FEB_Log_Config_t *config)
{
  if (config == NULL)
  {
    return -1;
  }

  /* Determine output function: custom callback or built-in UART routing */
  if (config->custom_output != NULL)
  {
    log_ctx.output = config->custom_output;
    log_ctx.use_uart = false;
  }
  else
  {
    log_ctx.uart_instance = config->uart_instance;
    log_ctx.output = log_uart_output;
    log_ctx.use_uart = true;
  }

  log_ctx.get_tick_ms = config->get_tick_ms;
  log_ctx.level = config->level;
  log_ctx.colors_enabled = config->colors;
  log_ctx.timestamps_enabled = config->timestamps;

#if FEB_LOG_USE_FREERTOS
  /* Store user-provided mutex (NOT created internally) */
  log_ctx.mutex = config->mutex;
#else
  log_ctx.mutex = 0;
#endif

  log_ctx.initialized = true;

  return 0;
}

bool FEB_Log_IsInitialized(void)
{
  return log_ctx.initialized;
}

/* ============================================================================
 * Runtime Configuration
 * ============================================================================ */

void FEB_Log_SetLevel(FEB_Log_Level_t level)
{
  log_ctx.level = level;
}

FEB_Log_Level_t FEB_Log_GetLevel(void)
{
  return log_ctx.level;
}

void FEB_Log_SetColors(bool enable)
{
  log_ctx.colors_enabled = enable;
}

bool FEB_Log_GetColors(void)
{
  return log_ctx.colors_enabled;
}

void FEB_Log_SetTimestamps(bool enable)
{
  log_ctx.timestamps_enabled = enable;
}

bool FEB_Log_GetTimestamps(void)
{
  return log_ctx.timestamps_enabled;
}

/* ============================================================================
 * Logging Functions
 * ============================================================================ */

void FEB_Log_Output(FEB_Log_Level_t level, const char *tag, const char *file, int line, const char *format, ...)
{
  if (!log_ctx.initialized || log_ctx.output == NULL)
  {
    return;
  }

  /* Runtime level filter */
  if (level > log_ctx.level || level == FEB_LOG_NONE)
  {
    return;
  }

  bool in_isr = FEB_LOG_IN_ISR();

  /*
   * Buffer selection: Use stack-local buffer for ISR context to avoid
   * race conditions with staging_buffer. ISR buffer is smaller due to
   * stack constraints.
   */
  char isr_buffer[128];
  char *buf;
  size_t buf_size;

  if (in_isr)
  {
    buf = isr_buffer;
    buf_size = sizeof(isr_buffer);
  }
  else
  {
    /* Acquire lock BEFORE formatting to protect staging_buffer */
    FEB_LOG_MUTEX_LOCK(log_ctx.mutex);
    buf = staging_buffer;
    buf_size = sizeof(staging_buffer);
  }

  int offset = 0;
  int ret;

  /*
   * Helper macro: Add snprintf result to offset and clamp to prevent overflow.
   * snprintf returns the number of bytes that WOULD be written (excluding null),
   * which can exceed remaining space. We must clamp after each call.
   */
#define SAFE_OFFSET_ADD(r)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    if ((r) > 0)                                                                                                       \
      offset += (r);                                                                                                   \
    if ((size_t)offset >= buf_size)                                                                                    \
      offset = (int)(buf_size - 1);                                                                                    \
  } while (0)

  /* Add color prefix if enabled */
  if (log_ctx.colors_enabled)
  {
    const char *color = "";
    switch (level)
    {
    case FEB_LOG_ERROR:
      color = FEB_LOG_COLOR_ERROR;
      break;
    case FEB_LOG_WARN:
      color = FEB_LOG_COLOR_WARN;
      break;
    case FEB_LOG_INFO:
      color = FEB_LOG_COLOR_INFO;
      break;
    case FEB_LOG_DEBUG:
      color = FEB_LOG_COLOR_DEBUG;
      break;
    case FEB_LOG_TRACE:
      color = FEB_LOG_COLOR_TRACE;
      break;
    default:
      break;
    }
    ret = snprintf(buf + offset, buf_size - (size_t)offset, "%s", color);
    SAFE_OFFSET_ADD(ret);
  }

  /* Add timestamp if enabled */
  if (log_ctx.timestamps_enabled && log_ctx.get_tick_ms != NULL)
  {
    uint32_t tick = log_ctx.get_tick_ms();
    ret = snprintf(buf + offset, buf_size - (size_t)offset, "[%lu] ", (unsigned long)tick);
    SAFE_OFFSET_ADD(ret);
  }

  /* Add level prefix */
  const char *level_str = "";
  switch (level)
  {
  case FEB_LOG_ERROR:
    level_str = "E";
    break;
  case FEB_LOG_WARN:
    level_str = "W";
    break;
  case FEB_LOG_INFO:
    level_str = "I";
    break;
  case FEB_LOG_DEBUG:
    level_str = "D";
    break;
  case FEB_LOG_TRACE:
    level_str = "T";
    break;
  default:
    break;
  }
  ret = snprintf(buf + offset, buf_size - (size_t)offset, "%s ", level_str);
  SAFE_OFFSET_ADD(ret);

  /* Add tag */
  if (tag != NULL)
  {
    ret = snprintf(buf + offset, buf_size - (size_t)offset, "%s ", tag);
    SAFE_OFFSET_ADD(ret);
  }

  /* Add user message */
  va_list args;
  va_start(args, format);
  ret = vsnprintf(buf + offset, buf_size - (size_t)offset, format, args);
  va_end(args);
  SAFE_OFFSET_ADD(ret);

  /* Add file/line for ERROR and WARN */
  if (file != NULL && (level == FEB_LOG_ERROR || level == FEB_LOG_WARN))
  {
    /* Extract just filename from full path */
    const char *filename = file;
    for (const char *p = file; *p; p++)
    {
      if (*p == '/' || *p == '\\')
      {
        filename = p + 1;
      }
    }
    ret = snprintf(buf + offset, buf_size - (size_t)offset, " (%s:%d)", filename, line);
    SAFE_OFFSET_ADD(ret);
  }

  /* Add color reset and newline */
  if (log_ctx.colors_enabled)
  {
    ret = snprintf(buf + offset, buf_size - (size_t)offset, "%s\r\n", FEB_LOG_ANSI_RESET);
    SAFE_OFFSET_ADD(ret);
  }
  else
  {
    ret = snprintf(buf + offset, buf_size - (size_t)offset, "\r\n");
    SAFE_OFFSET_ADD(ret);
  }

#undef SAFE_OFFSET_ADD

  /* Output the formatted message */
  log_ctx.output(buf, (size_t)offset);

  /* Release lock */
  if (!in_isr)
  {
    FEB_LOG_MUTEX_UNLOCK(log_ctx.mutex);
  }
}

int FEB_Log_Raw(const char *format, ...)
{
  if (!log_ctx.initialized || log_ctx.output == NULL)
  {
    return -1;
  }

  bool in_isr = FEB_LOG_IN_ISR();

  /*
   * Buffer selection: Use stack-local buffer for ISR context to avoid
   * race conditions with staging_buffer.
   */
  char isr_buffer[128];
  char *buf;
  size_t buf_size;

  if (in_isr)
  {
    buf = isr_buffer;
    buf_size = sizeof(isr_buffer);
  }
  else
  {
    /* Acquire lock */
    FEB_LOG_MUTEX_LOCK(log_ctx.mutex);
    buf = staging_buffer;
    buf_size = sizeof(staging_buffer);
  }

  va_list args;
  va_start(args, format);
  int len = vsnprintf(buf, buf_size, format, args);
  va_end(args);

  int result = -1;
  if (len > 0)
  {
    if ((size_t)len >= buf_size)
    {
      len = (int)(buf_size - 1);
    }
    result = log_ctx.output(buf, (size_t)len);
  }

  /* Release lock */
  if (!in_isr)
  {
    FEB_LOG_MUTEX_UNLOCK(log_ctx.mutex);
  }

  return result;
}

void FEB_Log_Hexdump(const char *tag, const uint8_t *data, size_t len)
{
  if (!log_ctx.initialized || log_ctx.output == NULL || data == NULL || len == 0)
  {
    return;
  }

  FEB_Log_Raw("%s HEX[%u]: ", tag ? tag : "", (unsigned)len);

  for (size_t i = 0; i < len; i++)
  {
    FEB_Log_Raw("%02X ", data[i]);
  }

  FEB_Log_Raw("\r\n");
}
