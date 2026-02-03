/**
 ******************************************************************************
 * @file           : feb_uart.c
 * @brief          : FEB UART Library Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Implements:
 *   - DMA-based non-blocking TX with ring buffer
 *   - DMA-based circular RX with idle line detection
 *   - Printf/scanf redirection via _write/_read overrides
 *   - FreeRTOS-optional thread safety
 *   - Logging with verbosity levels, colors, timestamps
 *
 ******************************************************************************
 */

#include "feb_uart.h"
#include "feb_uart_config.h"
#include "feb_uart_internal.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* STM32 HAL includes - main.h is MCU-agnostic (CubeMX includes correct HAL) */
#include "main.h"

/* ============================================================================
 * Private Types
 * ============================================================================ */

/**
 * @brief Library context structure
 */
typedef struct
{
  /* Configuration */
  UART_HandleTypeDef *huart;
  DMA_HandleTypeDef *hdma_tx;
  DMA_HandleTypeDef *hdma_rx;
  uint32_t (*get_tick_ms)(void);

  /* Runtime settings */
  FEB_UART_LogLevel_t log_level;
  bool colors_enabled;
  bool timestamps_enabled;
  bool initialized;

  /* TX state */
  FEB_UART_RingBuffer_t tx_ring;
  FEB_UART_TxState_t tx_state;
  size_t tx_dma_len;
  FEB_UART_Mutex_t tx_mutex;

  /* RX state */
  uint8_t *rx_buffer;
  size_t rx_buffer_size;
  volatile size_t rx_head; /* Updated by DMA/ISR */
  size_t rx_tail;          /* Updated by ProcessRx */
  FEB_UART_RxLineCallback_t rx_line_callback;
  FEB_UART_LineBuffer_t line_buffer;
  bool last_was_line_ending; /* Track \r\n and \n\r sequences */

} FEB_UART_Context_t;

/* ============================================================================
 * Private Variables
 * ============================================================================ */

static FEB_UART_Context_t ctx = {0};

/* Staging buffer for printf formatting */
static char staging_buffer[FEB_UART_STAGING_BUFFER_SIZE];

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void start_dma_tx(void);
static int feb_uart_write_internal(const uint8_t *data, size_t len);
static int feb_uart_printf_internal(const char *format, ...);
static size_t get_rx_count(void);
static uint32_t default_get_tick(void);

/* ============================================================================
 * Initialization
 * ============================================================================ */

int FEB_UART_Init(const FEB_UART_Config_t *config)
{
  if (config == NULL || config->huart == NULL)
  {
    return -1;
  }

  if (config->tx_buffer == NULL || config->tx_buffer_size == 0)
  {
    return -1;
  }

  if (config->rx_buffer == NULL || config->rx_buffer_size == 0)
  {
    return -1;
  }

  /* Store configuration */
  ctx.huart = config->huart;
  ctx.hdma_tx = config->hdma_tx;
  ctx.hdma_rx = config->hdma_rx;
  ctx.log_level = config->log_level;
  ctx.colors_enabled = config->enable_colors;
  ctx.timestamps_enabled = config->enable_timestamps;
  ctx.get_tick_ms = config->get_tick_ms ? config->get_tick_ms : default_get_tick;

  /* Initialize TX ring buffer */
  feb_uart_ring_init(&ctx.tx_ring, config->tx_buffer, config->tx_buffer_size);
  ctx.tx_state = FEB_UART_TX_IDLE;
  ctx.tx_dma_len = 0;

#if FEB_UART_USE_FREERTOS
  ctx.tx_mutex = FEB_UART_MUTEX_CREATE();
#endif

  /* Initialize RX state */
  ctx.rx_buffer = config->rx_buffer;
  ctx.rx_buffer_size = config->rx_buffer_size;
  ctx.rx_head = 0;
  ctx.rx_tail = 0;
  ctx.rx_line_callback = NULL;
  ctx.line_buffer.len = 0;
  ctx.last_was_line_ending = false;

  /* Start circular DMA RX if DMA available */
  if (ctx.hdma_rx != NULL)
  {
    /* Enable IDLE line interrupt */
    __HAL_UART_ENABLE_IT(ctx.huart, UART_IT_IDLE);

    /* Start circular DMA reception */
    HAL_UARTEx_ReceiveToIdle_DMA(ctx.huart, ctx.rx_buffer, ctx.rx_buffer_size);

    /* Disable half-transfer interrupt (we only care about IDLE and complete) */
    __HAL_DMA_DISABLE_IT(ctx.hdma_rx, DMA_IT_HT);
  }

  ctx.initialized = true;

  return 0;
}

void FEB_UART_DeInit(void)
{
  if (!ctx.initialized)
  {
    return;
  }

  /* Stop DMA transfers */
  if (ctx.hdma_tx != NULL)
  {
    HAL_UART_DMAStop(ctx.huart);
  }

  /* Disable IDLE interrupt */
  __HAL_UART_DISABLE_IT(ctx.huart, UART_IT_IDLE);

#if FEB_UART_USE_FREERTOS
  FEB_UART_MUTEX_DELETE(ctx.tx_mutex);
#endif

  memset(&ctx, 0, sizeof(ctx));
}

bool FEB_UART_IsInitialized(void)
{
  return ctx.initialized;
}

/* ============================================================================
 * Runtime Configuration
 * ============================================================================ */

void FEB_UART_SetLogLevel(FEB_UART_LogLevel_t level)
{
  ctx.log_level = level;
}

FEB_UART_LogLevel_t FEB_UART_GetLogLevel(void)
{
  return ctx.log_level;
}

void FEB_UART_SetColorsEnabled(bool enable)
{
  ctx.colors_enabled = enable;
}

bool FEB_UART_GetColorsEnabled(void)
{
  return ctx.colors_enabled;
}

void FEB_UART_SetTimestampsEnabled(bool enable)
{
  ctx.timestamps_enabled = enable;
}

bool FEB_UART_GetTimestampsEnabled(void)
{
  return ctx.timestamps_enabled;
}

/* ============================================================================
 * Output Functions
 * ============================================================================ */

int FEB_UART_Printf(const char *format, ...)
{
  if (!ctx.initialized)
  {
    return -1;
  }

  int written;
  bool in_isr = FEB_UART_IN_ISR();

  /* Acquire lock BEFORE formatting to protect staging_buffer */
#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_LOCK(ctx.tx_mutex);
  }
#else
  (void)in_isr;
  FEB_UART_ENTER_CRITICAL();
#endif

  va_list args;
  va_start(args, format);
  int len = vsnprintf(staging_buffer, sizeof(staging_buffer), format, args);
  va_end(args);

  if (len < 0)
  {
    written = -1;
  }
  else
  {
    /* Clamp to buffer size */
    if ((size_t)len >= sizeof(staging_buffer))
    {
      len = sizeof(staging_buffer) - 1;
    }
    written = feb_uart_write_internal((const uint8_t *)staging_buffer, (size_t)len);
  }

#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_UNLOCK(ctx.tx_mutex);
  }
#else
  FEB_UART_EXIT_CRITICAL();
#endif

  return written;
}

int FEB_UART_Write(const uint8_t *data, size_t len)
{
  if (!ctx.initialized || data == NULL || len == 0)
  {
    return -1;
  }

  int written;
  bool in_isr = FEB_UART_IN_ISR();

  /* Lock for thread safety */
#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_LOCK(ctx.tx_mutex);
  }
#else
  (void)in_isr;
  FEB_UART_ENTER_CRITICAL();
#endif

  written = feb_uart_write_internal(data, len);

#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_UNLOCK(ctx.tx_mutex);
  }
#else
  FEB_UART_EXIT_CRITICAL();
#endif

  return written;
}

int FEB_UART_Flush(uint32_t timeout_ms)
{
  if (!ctx.initialized)
  {
    return -1;
  }

  uint32_t start = ctx.get_tick_ms();

  while (!feb_uart_ring_empty(&ctx.tx_ring) || ctx.tx_state == FEB_UART_TX_DMA_ACTIVE)
  {
    if (timeout_ms > 0 && (ctx.get_tick_ms() - start) >= timeout_ms)
    {
      return -1; /* Timeout */
    }

#if FEB_UART_USE_FREERTOS
    osDelay(1);
#endif
  }

  return 0;
}

size_t FEB_UART_TxPending(void)
{
  return feb_uart_ring_count(&ctx.tx_ring);
}

/* ============================================================================
 * Input Functions
 * ============================================================================ */

void FEB_UART_SetRxLineCallback(FEB_UART_RxLineCallback_t callback)
{
  ctx.rx_line_callback = callback;
}

void FEB_UART_ProcessRx(void)
{
  if (!ctx.initialized)
  {
    return;
  }

  size_t count = get_rx_count();

  while (count > 0)
  {
    uint8_t byte = ctx.rx_buffer[ctx.rx_tail];
    ctx.rx_tail = (ctx.rx_tail + 1) % ctx.rx_buffer_size;
    count--;

    /* Check if this is a line ending character (\r or \n) */
    bool is_line_ending = (byte == '\r' || byte == '\n');

    if (is_line_ending)
    {
      /* Skip if this is the second char of a \r\n or \n\r sequence */
      if (ctx.last_was_line_ending)
      {
        ctx.last_was_line_ending = false;
        continue;
      }

      /* Trigger callback for complete line */
      if (ctx.line_buffer.len > 0 && ctx.rx_line_callback != NULL)
      {
        ctx.line_buffer.buffer[ctx.line_buffer.len] = '\0';
        ctx.rx_line_callback(ctx.line_buffer.buffer, ctx.line_buffer.len);
      }
      ctx.line_buffer.len = 0;
      ctx.last_was_line_ending = true;
      continue;
    }

    /* Reset line ending tracking for non-line-ending characters */
    ctx.last_was_line_ending = false;

    /* Add to line buffer (with overflow protection) */
    if (ctx.line_buffer.len < sizeof(ctx.line_buffer.buffer) - 1)
    {
      ctx.line_buffer.buffer[ctx.line_buffer.len++] = (char)byte;
    }
  }
}

size_t FEB_UART_RxAvailable(void)
{
  return get_rx_count();
}

size_t FEB_UART_Read(uint8_t *data, size_t max_len)
{
  if (!ctx.initialized || data == NULL || max_len == 0)
  {
    return 0;
  }

  size_t count = get_rx_count();
  if (max_len > count)
  {
    max_len = count;
  }

  size_t read = 0;
  while (read < max_len)
  {
    data[read] = ctx.rx_buffer[ctx.rx_tail];
    ctx.rx_tail = (ctx.rx_tail + 1) % ctx.rx_buffer_size;
    read++;
  }

  return read;
}

/* ============================================================================
 * HAL Callback Functions
 * ============================================================================ */

void FEB_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (!ctx.initialized || huart != ctx.huart)
  {
    return;
  }

  /* Advance ring buffer tail by completed DMA length */
  feb_uart_ring_advance_tail(&ctx.tx_ring, ctx.tx_dma_len);
  ctx.tx_state = FEB_UART_TX_IDLE;
  ctx.tx_dma_len = 0;

  /* Start next transfer if data pending */
  if (!feb_uart_ring_empty(&ctx.tx_ring))
  {
    start_dma_tx();
  }
}

void FEB_UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (!ctx.initialized || huart != ctx.huart)
  {
    return;
  }

  /* Update head position */
  ctx.rx_head = size;
}

void FEB_UART_IDLE_Callback(UART_HandleTypeDef *huart)
{
  if (!ctx.initialized || huart != ctx.huart)
  {
    return;
  }

  /* Check and clear IDLE flag */
  if (__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE))
  {
    __HAL_UART_CLEAR_IDLEFLAG(huart);

    /* Update head position from DMA counter */
    if (ctx.hdma_rx != NULL)
    {
      size_t dma_remaining = __HAL_DMA_GET_COUNTER(ctx.hdma_rx);
      ctx.rx_head = ctx.rx_buffer_size - dma_remaining;
    }
  }
}

/* ============================================================================
 * Printf/Scanf Redirection
 * ============================================================================
 *
 * Override weak symbols from syscalls.c to redirect stdio.
 * These are called by printf(), scanf(), etc.
 */

/**
 * @brief Override _write for printf output (newlib)
 * @note Weak attribute allows user to override if needed
 */
__attribute__((weak)) int _write(int file, char *ptr, int len)
{
  /* Only handle stdout (1) and stderr (2) */
  if (file != 1 && file != 2)
  {
    return -1;
  }

  if (!ctx.initialized)
  {
    /* Fall back to blocking HAL transmit if library not initialized */
    if (ctx.huart != NULL)
    {
      HAL_UART_Transmit(ctx.huart, (uint8_t *)ptr, len, HAL_MAX_DELAY);
      return len;
    }
    return -1;
  }

  int written = FEB_UART_Write((const uint8_t *)ptr, (size_t)len);
  return (written >= 0) ? written : -1;
}

/**
 * @brief Override _read for scanf input (newlib)
 * @note Weak attribute allows user to override if needed
 */
__attribute__((weak)) int _read(int file, char *ptr, int len)
{
  if (file != 0) /* stdin only */
  {
    return -1;
  }

  if (!ctx.initialized)
  {
    return -1;
  }

  /* Block until data available */
  while (get_rx_count() == 0)
  {
#if FEB_UART_USE_FREERTOS
    osDelay(1);
#endif
  }

  return (int)FEB_UART_Read((uint8_t *)ptr, (size_t)len);
}

/**
 * @brief Override __io_putchar for single character output
 * @note Weak attribute - CubeMX main.c may already define this
 */
__attribute__((weak)) int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;

  if (!ctx.initialized)
  {
    /* Fall back to blocking HAL transmit */
    if (ctx.huart != NULL)
    {
      HAL_UART_Transmit(ctx.huart, &c, 1, HAL_MAX_DELAY);
    }
    return ch;
  }

  FEB_UART_Write(&c, 1);
  return ch;
}

/**
 * @brief Override __io_getchar for single character input
 * @note Weak attribute - CubeMX main.c may already define this
 */
__attribute__((weak)) int __io_getchar(void)
{
  uint8_t c;

  if (!ctx.initialized)
  {
    return -1;
  }

  /* Block until data available */
  while (FEB_UART_Read(&c, 1) == 0)
  {
#if FEB_UART_USE_FREERTOS
    osDelay(1);
#endif
  }

  return (int)c;
}

/* ============================================================================
 * Logging Implementation
 * ============================================================================ */

void FEB_UART_Log(FEB_UART_LogLevel_t level, const char *tag, const char *file, int line, const char *format, ...)
{
  if (!ctx.initialized)
  {
    return;
  }

  /* Runtime level filtering */
  if (level > ctx.log_level)
  {
    return;
  }

  /* Select color based on level */
  const char *color_start = "";
  const char *color_end = "";
  const char *level_str = "";

  if (ctx.colors_enabled)
  {
    color_end = FEB_UART_ANSI_RESET;

    switch (level)
    {
    case FEB_UART_LOG_ERROR:
      color_start = FEB_UART_COLOR_ERROR;
      break;
    case FEB_UART_LOG_WARN:
      color_start = FEB_UART_COLOR_WARN;
      break;
    case FEB_UART_LOG_INFO:
      color_start = FEB_UART_COLOR_INFO;
      break;
    case FEB_UART_LOG_DEBUG:
      color_start = FEB_UART_COLOR_DEBUG;
      break;
    case FEB_UART_LOG_TRACE:
      color_start = FEB_UART_COLOR_TRACE;
      break;
    default:
      break;
    }
  }

  switch (level)
  {
  case FEB_UART_LOG_ERROR:
    level_str = "ERROR";
    break;
  case FEB_UART_LOG_WARN:
    level_str = "WARN";
    break;
  case FEB_UART_LOG_INFO:
    level_str = "INFO";
    break;
  case FEB_UART_LOG_DEBUG:
    level_str = "DEBUG";
    break;
  case FEB_UART_LOG_TRACE:
    level_str = "TRACE";
    break;
  default:
    break;
  }

  bool in_isr = FEB_UART_IN_ISR();

  /* Acquire lock for entire log message to prevent interleaving */
#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_LOCK(ctx.tx_mutex);
  }
#else
  (void)in_isr;
  FEB_UART_ENTER_CRITICAL();
#endif

  /* Format: [timestamp] TAG LEVEL: message (file:line) */
  if (ctx.timestamps_enabled)
  {
    feb_uart_printf_internal("%s[%u] %s %s: ", color_start, (unsigned int)ctx.get_tick_ms(), tag, level_str);
  }
  else
  {
    feb_uart_printf_internal("%s%s %s: ", color_start, tag, level_str);
  }

  /* Format user message */
  va_list args;
  va_start(args, format);
  int len = vsnprintf(staging_buffer, sizeof(staging_buffer), format, args);
  va_end(args);

  if (len > 0)
  {
    if ((size_t)len >= sizeof(staging_buffer))
    {
      len = sizeof(staging_buffer) - 1;
    }
    feb_uart_write_internal((const uint8_t *)staging_buffer, (size_t)len);
  }

  /* Add file:line for ERROR and WARN */
  if (file != NULL && line > 0)
  {
    /* Extract just the filename from full path */
    const char *filename = file;
    const char *p = file;
    while (*p)
    {
      if (*p == '/' || *p == '\\')
      {
        filename = p + 1;
      }
      p++;
    }
    feb_uart_printf_internal(" (%s:%d)", filename, line);
  }

  feb_uart_printf_internal("%s\r\n", color_end);

#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_UNLOCK(ctx.tx_mutex);
  }
#else
  FEB_UART_EXIT_CRITICAL();
#endif
}

void FEB_UART_LogHexdump(const char *tag, const uint8_t *data, size_t len)
{
  if (!ctx.initialized || data == NULL || len == 0)
  {
    return;
  }

  FEB_UART_Printf("%s HEXDUMP (%zu bytes):\r\n", tag, len);

  for (size_t i = 0; i < len; i += 16)
  {
    /* Address */
    FEB_UART_Printf("  %04zX: ", i);

    /* Hex bytes */
    for (size_t j = 0; j < 16; j++)
    {
      if (i + j < len)
      {
        FEB_UART_Printf("%02X ", data[i + j]);
      }
      else
      {
        FEB_UART_Printf("   ");
      }
      if (j == 7)
      {
        FEB_UART_Printf(" ");
      }
    }

    /* ASCII */
    FEB_UART_Printf(" |");
    for (size_t j = 0; j < 16 && i + j < len; j++)
    {
      char c = (char)data[i + j];
      FEB_UART_Printf("%c", (c >= 32 && c < 127) ? c : '.');
    }
    FEB_UART_Printf("|\r\n");
  }
}

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief Start DMA TX from ring buffer
 */
static void start_dma_tx(void)
{
  if (ctx.tx_state != FEB_UART_TX_IDLE || ctx.hdma_tx == NULL)
  {
    return;
  }

  size_t available = feb_uart_ring_count(&ctx.tx_ring);
  if (available == 0)
  {
    return;
  }

  /* Get contiguous bytes from tail */
  size_t contig_len = feb_uart_ring_contig_read_len(&ctx.tx_ring);

  ctx.tx_dma_len = contig_len;
  ctx.tx_state = FEB_UART_TX_DMA_ACTIVE;

  HAL_UART_Transmit_DMA(ctx.huart, &ctx.tx_ring.buffer[ctx.tx_ring.tail], contig_len);
}

/**
 * @brief Internal write function - caller must hold mutex
 * @param data Data to write
 * @param len  Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
static int feb_uart_write_internal(const uint8_t *data, size_t len)
{
  /* Write to ring buffer */
  size_t written = feb_uart_ring_write(&ctx.tx_ring, data, len);

  /* Start DMA if idle */
  if (ctx.tx_state == FEB_UART_TX_IDLE && ctx.hdma_tx != NULL)
  {
    start_dma_tx();
  }
  else if (ctx.hdma_tx == NULL)
  {
    /* Polling mode - transmit directly */
    size_t count = feb_uart_ring_count(&ctx.tx_ring);
    while (count > 0)
    {
      uint8_t byte;
      feb_uart_ring_read(&ctx.tx_ring, &byte, 1);
      HAL_UART_Transmit(ctx.huart, &byte, 1, FEB_UART_TX_TIMEOUT_MS);
      count--;
    }
  }

  return (int)written;
}

/**
 * @brief Internal printf function - caller must hold mutex
 * @param format Printf format string
 * @return Number of bytes written, or -1 on error
 */
static int feb_uart_printf_internal(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int len = vsnprintf(staging_buffer, sizeof(staging_buffer), format, args);
  va_end(args);

  if (len < 0)
  {
    return -1;
  }

  if ((size_t)len >= sizeof(staging_buffer))
  {
    len = sizeof(staging_buffer) - 1;
  }

  return feb_uart_write_internal((const uint8_t *)staging_buffer, (size_t)len);
}

/**
 * @brief Get number of bytes available in RX buffer
 */
static size_t get_rx_count(void)
{
  size_t head = ctx.rx_head;
  size_t tail = ctx.rx_tail;

  if (head >= tail)
  {
    return head - tail;
  }
  return ctx.rx_buffer_size - tail + head;
}

/**
 * @brief Default timestamp function using HAL_GetTick
 */
static uint32_t default_get_tick(void)
{
  return HAL_GetTick();
}
