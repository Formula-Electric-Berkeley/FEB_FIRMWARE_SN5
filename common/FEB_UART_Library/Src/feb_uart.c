/**
 ******************************************************************************
 * @file           : feb_uart.c
 * @brief          : FEB UART Library Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Implements:
 *   - Multi-instance support (up to FEB_UART_MAX_INSTANCES UARTs)
 *   - DMA-based non-blocking TX with ring buffer
 *   - DMA-based circular RX with idle line detection
 *   - Printf/scanf redirection via _write/_read overrides (instance 0 only)
 *   - FreeRTOS-optional thread safety
 *   - Logging with verbosity levels, colors, timestamps
 *   - Optional FreeRTOS queue support per-instance
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
 * @brief Library context structure (one per instance)
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

#if FEB_UART_ENABLE_QUEUES
  /* Queue state */
  FEB_UART_Queue_t rx_queue;
  FEB_UART_Queue_t tx_queue;
  bool rx_queue_enabled;
  bool tx_queue_enabled;
#endif

} FEB_UART_Context_t;

/* ============================================================================
 * Private Variables
 * ============================================================================ */

/* Array of contexts for multi-instance support */
static FEB_UART_Context_t ctx[FEB_UART_MAX_INSTANCES] = {0};

/* Per-instance staging buffers for printf formatting */
static char staging_buffer[FEB_UART_MAX_INSTANCES][FEB_UART_STAGING_BUFFER_SIZE];

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void start_dma_tx(int inst);
static int feb_uart_write_internal(int inst, const uint8_t *data, size_t len);
static size_t get_rx_count(int inst);
static uint32_t default_get_tick(void);
static int find_instance_by_huart(UART_HandleTypeDef *huart);

/* Instance validation macros */
#define VALIDATE_INSTANCE(inst)                                                                                        \
  do                                                                                                                   \
  {                                                                                                                    \
    if ((inst) >= FEB_UART_MAX_INSTANCES)                                                                              \
      return -1;                                                                                                       \
  } while (0)

#define VALIDATE_INSTANCE_INIT(inst)                                                                                   \
  do                                                                                                                   \
  {                                                                                                                    \
    if ((inst) >= FEB_UART_MAX_INSTANCES || !ctx[inst].initialized)                                                    \
      return -1;                                                                                                       \
  } while (0)

#define VALIDATE_INSTANCE_VOID(inst)                                                                                   \
  do                                                                                                                   \
  {                                                                                                                    \
    if ((inst) >= FEB_UART_MAX_INSTANCES || !ctx[inst].initialized)                                                    \
      return;                                                                                                          \
  } while (0)

#define VALIDATE_INSTANCE_ZERO(inst)                                                                                   \
  do                                                                                                                   \
  {                                                                                                                    \
    if ((inst) >= FEB_UART_MAX_INSTANCES || !ctx[inst].initialized)                                                    \
      return 0;                                                                                                        \
  } while (0)

#define VALIDATE_INSTANCE_BOOL(inst)                                                                                   \
  do                                                                                                                   \
  {                                                                                                                    \
    if ((inst) >= FEB_UART_MAX_INSTANCES)                                                                              \
      return false;                                                                                                    \
  } while (0)

/* ============================================================================
 * Initialization
 * ============================================================================ */

int FEB_UART_Init(FEB_UART_Instance_t instance, const FEB_UART_Config_t *config)
{
  VALIDATE_INSTANCE(instance);
  int inst = (int)instance;

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
  ctx[inst].huart = config->huart;
  ctx[inst].hdma_tx = config->hdma_tx;
  ctx[inst].hdma_rx = config->hdma_rx;
  ctx[inst].log_level = config->log_level;
  ctx[inst].colors_enabled = config->enable_colors;
  ctx[inst].timestamps_enabled = config->enable_timestamps;
  ctx[inst].get_tick_ms = config->get_tick_ms ? config->get_tick_ms : default_get_tick;

  /* Initialize TX ring buffer */
  feb_uart_ring_init(&ctx[inst].tx_ring, config->tx_buffer, config->tx_buffer_size);
  ctx[inst].tx_state = FEB_UART_TX_IDLE;
  ctx[inst].tx_dma_len = 0;

#if FEB_UART_USE_FREERTOS
  ctx[inst].tx_mutex = FEB_UART_MUTEX_CREATE();
#endif

  /* Initialize RX state */
  ctx[inst].rx_buffer = config->rx_buffer;
  ctx[inst].rx_buffer_size = config->rx_buffer_size;
  ctx[inst].rx_head = 0;
  ctx[inst].rx_tail = 0;
  ctx[inst].rx_line_callback = NULL;
  ctx[inst].line_buffer.len = 0;
  ctx[inst].last_was_line_ending = false;

  /* Start DMA RX if DMA available */
  if (ctx[inst].hdma_rx != NULL)
  {
    /* Start DMA reception FIRST (before enabling IDLE interrupt) */
    HAL_UARTEx_ReceiveToIdle_DMA(ctx[inst].huart, ctx[inst].rx_buffer, ctx[inst].rx_buffer_size);

    /* THEN enable IDLE line interrupt (after DMA is ready) */
    __HAL_UART_ENABLE_IT(ctx[inst].huart, UART_IT_IDLE);

    /* Disable half-transfer interrupt (we only care about IDLE and complete) */
    __HAL_DMA_DISABLE_IT(ctx[inst].hdma_rx, DMA_IT_HT);
  }

#if FEB_UART_ENABLE_QUEUES
  /* Initialize queues if enabled */
  ctx[inst].rx_queue_enabled = config->enable_rx_queue;
  ctx[inst].tx_queue_enabled = config->enable_tx_queue;

  if (ctx[inst].rx_queue_enabled)
  {
    ctx[inst].rx_queue = FEB_UART_QUEUE_CREATE(FEB_UART_RX_QUEUE_DEPTH, sizeof(FEB_UART_RxQueueMsg_t));
  }

  if (ctx[inst].tx_queue_enabled)
  {
    ctx[inst].tx_queue = FEB_UART_QUEUE_CREATE(FEB_UART_TX_QUEUE_DEPTH, sizeof(FEB_UART_TxQueueMsg_t));
  }
#endif

  ctx[inst].initialized = true;

  return 0;
}

void FEB_UART_DeInit(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_VOID(instance);
  int inst = (int)instance;

  /* Stop DMA transfers */
  if (ctx[inst].hdma_tx != NULL)
  {
    HAL_UART_DMAStop(ctx[inst].huart);
  }

  /* Disable IDLE interrupt */
  __HAL_UART_DISABLE_IT(ctx[inst].huart, UART_IT_IDLE);

#if FEB_UART_USE_FREERTOS
  FEB_UART_MUTEX_DELETE(ctx[inst].tx_mutex);
#endif

#if FEB_UART_ENABLE_QUEUES
  /* Delete queues if they exist */
  if (ctx[inst].rx_queue != NULL)
  {
    FEB_UART_QUEUE_DELETE(ctx[inst].rx_queue);
    ctx[inst].rx_queue = NULL;
  }
  if (ctx[inst].tx_queue != NULL)
  {
    FEB_UART_QUEUE_DELETE(ctx[inst].tx_queue);
    ctx[inst].tx_queue = NULL;
  }
  ctx[inst].rx_queue_enabled = false;
  ctx[inst].tx_queue_enabled = false;
#endif

  memset(&ctx[inst], 0, sizeof(ctx[inst]));
}

bool FEB_UART_IsInitialized(FEB_UART_Instance_t instance)
{
  if (instance >= FEB_UART_MAX_INSTANCES)
  {
    return false;
  }
  return ctx[instance].initialized;
}

/* ============================================================================
 * Runtime Configuration
 * ============================================================================ */

void FEB_UART_SetLogLevel(FEB_UART_Instance_t instance, FEB_UART_LogLevel_t level)
{
  VALIDATE_INSTANCE_VOID(instance);
  ctx[instance].log_level = level;
}

FEB_UART_LogLevel_t FEB_UART_GetLogLevel(FEB_UART_Instance_t instance)
{
  if (instance >= FEB_UART_MAX_INSTANCES || !ctx[instance].initialized)
  {
    return FEB_UART_LOG_NONE;
  }
  return ctx[instance].log_level;
}

void FEB_UART_SetColorsEnabled(FEB_UART_Instance_t instance, bool enable)
{
  VALIDATE_INSTANCE_VOID(instance);
  ctx[instance].colors_enabled = enable;
}

bool FEB_UART_GetColorsEnabled(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_BOOL(instance);
  if (!ctx[instance].initialized)
    return false;
  return ctx[instance].colors_enabled;
}

void FEB_UART_SetTimestampsEnabled(FEB_UART_Instance_t instance, bool enable)
{
  VALIDATE_INSTANCE_VOID(instance);
  ctx[instance].timestamps_enabled = enable;
}

bool FEB_UART_GetTimestampsEnabled(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_BOOL(instance);
  if (!ctx[instance].initialized)
    return false;
  return ctx[instance].timestamps_enabled;
}

/* ============================================================================
 * Logging Functions
 * ============================================================================ */

void FEB_UART_Log(FEB_UART_LogLevel_t level, const char *tag, const char *file, int line, const char *format, ...)
{
  /* Use instance 0 for logging (all LOG_* macros use instance 0) */
  int inst = 0;

  if (!ctx[inst].initialized)
  {
    return;
  }

  /* Runtime level filter */
  if (level > ctx[inst].log_level || level == FEB_UART_LOG_NONE)
  {
    return;
  }

  bool in_isr = FEB_UART_IN_ISR();

  /* Acquire lock BEFORE formatting to protect staging_buffer */
#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_LOCK(ctx[inst].tx_mutex);
  }
#else
  (void)in_isr;
  FEB_UART_ENTER_CRITICAL();
#endif

  int offset = 0;
  char *buf = staging_buffer[inst];
  size_t buf_size = sizeof(staging_buffer[inst]);

  /* Add color prefix if enabled */
  if (ctx[inst].colors_enabled)
  {
    const char *color = "";
    switch (level)
    {
    case FEB_UART_LOG_ERROR:
      color = FEB_UART_COLOR_ERROR;
      break;
    case FEB_UART_LOG_WARN:
      color = FEB_UART_COLOR_WARN;
      break;
    case FEB_UART_LOG_INFO:
      color = FEB_UART_COLOR_INFO;
      break;
    case FEB_UART_LOG_DEBUG:
      color = FEB_UART_COLOR_DEBUG;
      break;
    case FEB_UART_LOG_TRACE:
      color = FEB_UART_COLOR_TRACE;
      break;
    default:
      break;
    }
    offset += snprintf(buf + offset, buf_size - offset, "%s", color);
  }

  /* Add timestamp if enabled */
  if (ctx[inst].timestamps_enabled && ctx[inst].get_tick_ms != NULL)
  {
    uint32_t tick = ctx[inst].get_tick_ms();
    offset += snprintf(buf + offset, buf_size - offset, "[%lu] ", (unsigned long)tick);
  }

  /* Add level prefix */
  const char *level_str = "";
  switch (level)
  {
  case FEB_UART_LOG_ERROR:
    level_str = "E";
    break;
  case FEB_UART_LOG_WARN:
    level_str = "W";
    break;
  case FEB_UART_LOG_INFO:
    level_str = "I";
    break;
  case FEB_UART_LOG_DEBUG:
    level_str = "D";
    break;
  case FEB_UART_LOG_TRACE:
    level_str = "T";
    break;
  default:
    break;
  }
  offset += snprintf(buf + offset, buf_size - offset, "%s ", level_str);

  /* Add tag */
  if (tag != NULL)
  {
    offset += snprintf(buf + offset, buf_size - offset, "%s ", tag);
  }

  /* Add user message */
  va_list args;
  va_start(args, format);
  offset += vsnprintf(buf + offset, buf_size - offset, format, args);
  va_end(args);

  /* Add file/line for ERROR and WARN */
  if (file != NULL && (level == FEB_UART_LOG_ERROR || level == FEB_UART_LOG_WARN))
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
    offset += snprintf(buf + offset, buf_size - offset, " (%s:%d)", filename, line);
  }

  /* Add color reset and newline */
  if (ctx[inst].colors_enabled)
  {
    offset += snprintf(buf + offset, buf_size - offset, "%s\r\n", FEB_UART_ANSI_RESET);
  }
  else
  {
    offset += snprintf(buf + offset, buf_size - offset, "\r\n");
  }

  /* Clamp to buffer size */
  if ((size_t)offset >= buf_size)
  {
    offset = buf_size - 1;
  }

  /* Write to ring buffer and start DMA */
  feb_uart_write_internal(inst, (const uint8_t *)buf, (size_t)offset);

  /* Release lock */
#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_UNLOCK(ctx[inst].tx_mutex);
  }
#else
  FEB_UART_EXIT_CRITICAL();
#endif
}

void FEB_UART_LogHexdump(const char *tag, const uint8_t *data, size_t len)
{
  if (data == NULL || len == 0)
  {
    return;
  }

  FEB_UART_Printf(FEB_UART_INSTANCE_1, "%s HEX[%u]: ", tag ? tag : "", (unsigned)len);

  for (size_t i = 0; i < len; i++)
  {
    FEB_UART_Printf(FEB_UART_INSTANCE_1, "%02X ", data[i]);
  }

  FEB_UART_Printf(FEB_UART_INSTANCE_1, "\r\n");
}

/* ============================================================================
 * Output Functions
 * ============================================================================ */

int FEB_UART_Printf(FEB_UART_Instance_t instance, const char *format, ...)
{
  VALIDATE_INSTANCE_INIT(instance);
  int inst = (int)instance;

  int written;
  bool in_isr = FEB_UART_IN_ISR();

  /* Acquire lock BEFORE formatting to protect staging_buffer */
#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_LOCK(ctx[inst].tx_mutex);
  }
#else
  (void)in_isr;
  FEB_UART_ENTER_CRITICAL();
#endif

  va_list args;
  va_start(args, format);
  int len = vsnprintf(staging_buffer[inst], sizeof(staging_buffer[inst]), format, args);
  va_end(args);

  if (len < 0)
  {
    written = -1;
  }
  else
  {
    /* Clamp to buffer size */
    if ((size_t)len >= sizeof(staging_buffer[inst]))
    {
      len = sizeof(staging_buffer[inst]) - 1;
    }
    written = feb_uart_write_internal(inst, (const uint8_t *)staging_buffer[inst], (size_t)len);
  }

#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_UNLOCK(ctx[inst].tx_mutex);
  }
#else
  FEB_UART_EXIT_CRITICAL();
#endif

  return written;
}

int FEB_UART_Write(FEB_UART_Instance_t instance, const uint8_t *data, size_t len)
{
  VALIDATE_INSTANCE_INIT(instance);
  int inst = (int)instance;

  if (data == NULL || len == 0)
  {
    return -1;
  }

  int written;
  bool in_isr = FEB_UART_IN_ISR();

  /* Lock for thread safety */
#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_LOCK(ctx[inst].tx_mutex);
  }
#else
  (void)in_isr;
  FEB_UART_ENTER_CRITICAL();
#endif

  written = feb_uart_write_internal(inst, data, len);

#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_UNLOCK(ctx[inst].tx_mutex);
  }
#else
  FEB_UART_EXIT_CRITICAL();
#endif

  return written;
}

int FEB_UART_Flush(FEB_UART_Instance_t instance, uint32_t timeout_ms)
{
  VALIDATE_INSTANCE_INIT(instance);
  int inst = (int)instance;

  uint32_t start = ctx[inst].get_tick_ms();

  while (!feb_uart_ring_empty(&ctx[inst].tx_ring) || ctx[inst].tx_state == FEB_UART_TX_DMA_ACTIVE)
  {
    if (timeout_ms > 0 && (ctx[inst].get_tick_ms() - start) >= timeout_ms)
    {
      return -1; /* Timeout */
    }

#if FEB_UART_USE_FREERTOS
    osDelay(1);
#endif
  }

  return 0;
}

size_t FEB_UART_TxPending(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_ZERO(instance);
  return feb_uart_ring_count(&ctx[instance].tx_ring);
}

/* ============================================================================
 * Input Functions
 * ============================================================================ */

void FEB_UART_SetRxLineCallback(FEB_UART_Instance_t instance, FEB_UART_RxLineCallback_t callback)
{
  VALIDATE_INSTANCE_VOID(instance);
  ctx[instance].rx_line_callback = callback;
}

void FEB_UART_ProcessRx(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_VOID(instance);
  int inst = (int)instance;

  size_t count = get_rx_count(inst);

  while (count > 0)
  {
    uint8_t byte = ctx[inst].rx_buffer[ctx[inst].rx_tail];
    ctx[inst].rx_tail = (ctx[inst].rx_tail + 1) % ctx[inst].rx_buffer_size;
    count--;

    /* Check if this is a line ending character (\r or \n) */
    bool is_line_ending = (byte == '\r' || byte == '\n');

    if (is_line_ending)
    {
      /* Skip if this is the second char of a \r\n or \n\r sequence */
      if (ctx[inst].last_was_line_ending)
      {
        ctx[inst].last_was_line_ending = false;
        continue;
      }

      /* Trigger callback or post to queue for complete line */
      if (ctx[inst].line_buffer.len > 0)
      {
        ctx[inst].line_buffer.buffer[ctx[inst].line_buffer.len] = '\0';

#if FEB_UART_ENABLE_QUEUES
        if (ctx[inst].rx_queue_enabled && ctx[inst].rx_queue != NULL)
        {
          /* Post to RX queue */
          FEB_UART_RxQueueMsg_t msg;
          memcpy(msg.line, ctx[inst].line_buffer.buffer, ctx[inst].line_buffer.len + 1);
          msg.len = (uint16_t)ctx[inst].line_buffer.len;
          msg.timestamp = ctx[inst].get_tick_ms ? ctx[inst].get_tick_ms() : 0;
          FEB_UART_QUEUE_SEND(ctx[inst].rx_queue, &msg, 0); /* Non-blocking */
        }
        else
#endif
            if (ctx[inst].rx_line_callback != NULL)
        {
          ctx[inst].rx_line_callback(ctx[inst].line_buffer.buffer, ctx[inst].line_buffer.len);
        }
      }
      ctx[inst].line_buffer.len = 0;
      ctx[inst].last_was_line_ending = true;
      continue;
    }

    /* Reset line ending tracking for non-line-ending characters */
    ctx[inst].last_was_line_ending = false;

    /* Add to line buffer (with overflow protection) */
    if (ctx[inst].line_buffer.len < sizeof(ctx[inst].line_buffer.buffer) - 1)
    {
      ctx[inst].line_buffer.buffer[ctx[inst].line_buffer.len++] = (char)byte;
    }
  }
}

size_t FEB_UART_RxAvailable(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_ZERO(instance);
  return get_rx_count((int)instance);
}

size_t FEB_UART_Read(FEB_UART_Instance_t instance, uint8_t *data, size_t max_len)
{
  VALIDATE_INSTANCE_ZERO(instance);
  int inst = (int)instance;

  if (data == NULL || max_len == 0)
  {
    return 0;
  }

  size_t count = get_rx_count(inst);
  if (max_len > count)
  {
    max_len = count;
  }

  size_t read = 0;
  while (read < max_len)
  {
    data[read] = ctx[inst].rx_buffer[ctx[inst].rx_tail];
    ctx[inst].rx_tail = (ctx[inst].rx_tail + 1) % ctx[inst].rx_buffer_size;
    read++;
  }

  return read;
}

/* ============================================================================
 * HAL Callback Functions
 * ============================================================================ */

/**
 * @brief Find instance by UART handle
 * @return Instance index, or -1 if not found
 */
static int find_instance_by_huart(UART_HandleTypeDef *huart)
{
  for (int i = 0; i < FEB_UART_MAX_INSTANCES; i++)
  {
    if (ctx[i].initialized && ctx[i].huart == huart)
    {
      return i;
    }
  }
  return -1;
}

void FEB_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  int inst = find_instance_by_huart(huart);
  if (inst < 0)
  {
    return;
  }

  /* Advance ring buffer tail by completed DMA length */
  feb_uart_ring_advance_tail(&ctx[inst].tx_ring, ctx[inst].tx_dma_len);
  ctx[inst].tx_state = FEB_UART_TX_IDLE;
  ctx[inst].tx_dma_len = 0;

  /* Start next transfer if data pending */
  if (!feb_uart_ring_empty(&ctx[inst].tx_ring))
  {
    start_dma_tx(inst);
  }
}

void FEB_UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  int inst = find_instance_by_huart(huart);
  if (inst < 0)
  {
    return;
  }

  /* Update head position so ProcessRx can read the received data */
  ctx[inst].rx_head = size;

  /* Non-circular DMA mode: restart reception after each transfer.
   * Circular mode (F4) keeps running; non-circular mode (U5 GPDMA) stops after each transfer. */
  if (ctx[inst].hdma_rx != NULL && ctx[inst].hdma_rx->Init.Mode == DMA_NORMAL)
  {
    /* Restart DMA from buffer start */
    HAL_UARTEx_ReceiveToIdle_DMA(ctx[inst].huart, ctx[inst].rx_buffer, ctx[inst].rx_buffer_size);
    __HAL_DMA_DISABLE_IT(ctx[inst].hdma_rx, DMA_IT_HT);
  }
}

void FEB_UART_IDLE_Callback(UART_HandleTypeDef *huart)
{
  int inst = find_instance_by_huart(huart);
  if (inst < 0)
  {
    return;
  }

  /* Check and clear IDLE flag */
  if (__HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE))
  {
    __HAL_UART_CLEAR_IDLEFLAG(huart);

    /* Update head position from DMA counter */
    if (ctx[inst].hdma_rx != NULL)
    {
      size_t dma_remaining = __HAL_DMA_GET_COUNTER(ctx[inst].hdma_rx);
      ctx[inst].rx_head = ctx[inst].rx_buffer_size - dma_remaining;
    }
  }
}

/* ============================================================================
 * Printf/Scanf Redirection (Instance 0 Only)
 * ============================================================================
 *
 * Override weak symbols from syscalls.c to redirect stdio.
 * These always use instance 0 for printf(), scanf(), etc.
 */

/**
 * @brief Override _write for printf output (newlib)
 */
__attribute__((weak)) int _write(int file, char *ptr, int len)
{
  /* Only handle stdout (1) and stderr (2) */
  if (file != 1 && file != 2)
  {
    return -1;
  }

  /* Printf always uses instance 0 */
  if (!ctx[0].initialized)
  {
    /* Fall back to blocking HAL transmit if library not initialized */
    if (ctx[0].huart != NULL)
    {
      HAL_UART_Transmit(ctx[0].huart, (uint8_t *)ptr, len, HAL_MAX_DELAY);
      return len;
    }
    return -1;
  }

  int written = FEB_UART_Write(FEB_UART_INSTANCE_1, (const uint8_t *)ptr, (size_t)len);
  return (written >= 0) ? written : -1;
}

/**
 * @brief Override _read for scanf input (newlib)
 */
__attribute__((weak)) int _read(int file, char *ptr, int len)
{
  if (file != 0) /* stdin only */
  {
    return -1;
  }

  if (!ctx[0].initialized)
  {
    return -1;
  }

  /* Block until data available */
  while (get_rx_count(0) == 0)
  {
#if FEB_UART_USE_FREERTOS
    osDelay(1);
#endif
  }

  return (int)FEB_UART_Read(FEB_UART_INSTANCE_1, (uint8_t *)ptr, (size_t)len);
}

/**
 * @brief Override __io_putchar for single character output
 */
__attribute__((weak)) int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;

  if (!ctx[0].initialized)
  {
    /* Fall back to blocking HAL transmit */
    if (ctx[0].huart != NULL)
    {
      HAL_UART_Transmit(ctx[0].huart, &c, 1, HAL_MAX_DELAY);
    }
    return ch;
  }

  FEB_UART_Write(FEB_UART_INSTANCE_1, &c, 1);
  return ch;
}

/**
 * @brief Override __io_getchar for single character input
 */
__attribute__((weak)) int __io_getchar(void)
{
  uint8_t c;

  if (!ctx[0].initialized)
  {
    return -1;
  }

  /* Block until data available */
  while (FEB_UART_Read(FEB_UART_INSTANCE_1, &c, 1) == 0)
  {
#if FEB_UART_USE_FREERTOS
    osDelay(1);
#endif
  }

  return (int)c;
}

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/**
 * @brief Start DMA TX from ring buffer
 */
static void start_dma_tx(int inst)
{
  if (ctx[inst].tx_state != FEB_UART_TX_IDLE || ctx[inst].hdma_tx == NULL)
  {
    return;
  }

  size_t available = feb_uart_ring_count(&ctx[inst].tx_ring);
  if (available == 0)
  {
    return;
  }

  /* Get contiguous bytes from tail */
  size_t contig_len = feb_uart_ring_contig_read_len(&ctx[inst].tx_ring);

  ctx[inst].tx_dma_len = contig_len;
  ctx[inst].tx_state = FEB_UART_TX_DMA_ACTIVE;

  /* Try DMA, fall back to polling on failure */
  HAL_StatusTypeDef status =
      HAL_UART_Transmit_DMA(ctx[inst].huart, &ctx[inst].tx_ring.buffer[ctx[inst].tx_ring.tail], contig_len);

  if (status != HAL_OK)
  {
    /* DMA failed - reset state and fall back to polling */
    ctx[inst].tx_state = FEB_UART_TX_IDLE;
    ctx[inst].tx_dma_len = 0;

    /* Transmit using polling */
    while (!feb_uart_ring_empty(&ctx[inst].tx_ring))
    {
      uint8_t byte;
      feb_uart_ring_read(&ctx[inst].tx_ring, &byte, 1);
      HAL_UART_Transmit(ctx[inst].huart, &byte, 1, FEB_UART_TX_TIMEOUT_MS);
    }
  }
}

/**
 * @brief Internal write function - caller must hold mutex/critical section
 *
 * Blocks until sufficient buffer space is available (in main context).
 * In ISR context, falls back to truncated write to avoid blocking.
 */
static int feb_uart_write_internal(int inst, const uint8_t *data, size_t len)
{
  /* Block until sufficient space available (with timeout) */
  /* Note: Caller already holds critical section, so we exit/enter to allow DMA ISR */
  uint32_t start = ctx[inst].get_tick_ms ? ctx[inst].get_tick_ms() : 0;
  const uint32_t timeout_ms = 1000; /* 1 second timeout */

  while (feb_uart_ring_space(&ctx[inst].tx_ring) < len)
  {
    /* ISR context: can't block, just truncate as before */
    if (FEB_UART_IN_ISR())
    {
      break;
    }

    /* Check timeout */
    if (ctx[inst].get_tick_ms && (ctx[inst].get_tick_ms() - start) > timeout_ms)
    {
      break; /* Timeout - proceed with truncated write */
    }

    /* Release critical section to allow DMA completion interrupt to run */
    FEB_UART_EXIT_CRITICAL();
    /* Brief yield - DMA ISR can now advance tail pointer */
    for (volatile int i = 0; i < 100; i++)
    {
    }
    FEB_UART_ENTER_CRITICAL();
  }

  /* Write to ring buffer (may still truncate if ISR or timeout) */
  size_t written = feb_uart_ring_write(&ctx[inst].tx_ring, data, len);

  /* Start DMA if idle */
  if (ctx[inst].tx_state == FEB_UART_TX_IDLE && ctx[inst].hdma_tx != NULL)
  {
    start_dma_tx(inst);
  }
  else if (ctx[inst].hdma_tx == NULL)
  {
    /* Polling mode - transmit directly */
    size_t count = feb_uart_ring_count(&ctx[inst].tx_ring);
    while (count > 0)
    {
      uint8_t byte;
      feb_uart_ring_read(&ctx[inst].tx_ring, &byte, 1);
      HAL_UART_Transmit(ctx[inst].huart, &byte, 1, FEB_UART_TX_TIMEOUT_MS);
      count--;
    }
  }

  return (int)written;
}

/**
 * @brief Get number of bytes available in RX buffer
 */
static size_t get_rx_count(int inst)
{
  size_t head = ctx[inst].rx_head;
  size_t tail = ctx[inst].rx_tail;

  if (head >= tail)
  {
    return head - tail;
  }
  return ctx[inst].rx_buffer_size - tail + head;
}

/**
 * @brief Default timestamp function using HAL_GetTick
 */
static uint32_t default_get_tick(void)
{
  return HAL_GetTick();
}

/* ============================================================================
 * Queue API Implementation (FreeRTOS only)
 * ============================================================================ */

#if FEB_UART_ENABLE_QUEUES

bool FEB_UART_QueueReceiveLine(FEB_UART_Instance_t instance, char *buffer, size_t max_len, size_t *out_len,
                               uint32_t timeout)
{
  VALIDATE_INSTANCE_BOOL(instance);
  int inst = (int)instance;

  if (!ctx[inst].initialized || buffer == NULL || max_len == 0)
  {
    return false;
  }

  if (!ctx[inst].rx_queue_enabled || ctx[inst].rx_queue == NULL)
  {
    return false;
  }

  FEB_UART_RxQueueMsg_t msg;
  if (!FEB_UART_QUEUE_RECEIVE(ctx[inst].rx_queue, &msg, timeout))
  {
    return false;
  }

  /* Copy line to buffer, respecting max_len */
  size_t copy_len = (msg.len < max_len - 1) ? msg.len : max_len - 1;
  memcpy(buffer, msg.line, copy_len);
  buffer[copy_len] = '\0';

  if (out_len != NULL)
  {
    *out_len = copy_len;
  }

  return true;
}

uint32_t FEB_UART_RxQueueCount(FEB_UART_Instance_t instance)
{
  if (instance >= FEB_UART_MAX_INSTANCES || !ctx[instance].initialized)
  {
    return 0;
  }

  if (!ctx[instance].rx_queue_enabled || ctx[instance].rx_queue == NULL)
  {
    return 0;
  }

  return FEB_UART_QUEUE_COUNT(ctx[instance].rx_queue);
}

bool FEB_UART_IsRxQueueEnabled(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_BOOL(instance);
  return ctx[instance].initialized && ctx[instance].rx_queue_enabled;
}

int FEB_UART_QueueWrite(FEB_UART_Instance_t instance, const uint8_t *data, size_t len, uint32_t timeout)
{
  VALIDATE_INSTANCE_INIT(instance);
  int inst = (int)instance;

  if (data == NULL || len == 0)
  {
    return -1;
  }

  if (!ctx[inst].tx_queue_enabled || ctx[inst].tx_queue == NULL)
  {
    return -1;
  }

  /* Clamp to max message size */
  if (len > FEB_UART_TX_QUEUE_MSG_SIZE)
  {
    len = FEB_UART_TX_QUEUE_MSG_SIZE;
  }

  FEB_UART_TxQueueMsg_t msg;
  memcpy(msg.data, data, len);
  msg.len = (uint16_t)len;

  if (!FEB_UART_QUEUE_SEND(ctx[inst].tx_queue, &msg, timeout))
  {
    return -1;
  }

  return (int)len;
}

int FEB_UART_QueuePrintf(FEB_UART_Instance_t instance, uint32_t timeout, const char *format, ...)
{
  VALIDATE_INSTANCE_INIT(instance);
  int inst = (int)instance;

  if (format == NULL)
  {
    return -1;
  }

  if (!ctx[inst].tx_queue_enabled || ctx[inst].tx_queue == NULL)
  {
    return -1;
  }

  FEB_UART_TxQueueMsg_t msg;

  va_list args;
  va_start(args, format);
  int len = vsnprintf((char *)msg.data, FEB_UART_TX_QUEUE_MSG_SIZE, format, args);
  va_end(args);

  if (len < 0)
  {
    return -1;
  }

  msg.len = (len < FEB_UART_TX_QUEUE_MSG_SIZE) ? (uint16_t)len : FEB_UART_TX_QUEUE_MSG_SIZE;

  if (!FEB_UART_QUEUE_SEND(ctx[inst].tx_queue, &msg, timeout))
  {
    return -1;
  }

  return msg.len;
}

void FEB_UART_ProcessTxQueue(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_VOID(instance);
  int inst = (int)instance;

  if (!ctx[inst].tx_queue_enabled || ctx[inst].tx_queue == NULL)
  {
    return;
  }

  FEB_UART_TxQueueMsg_t msg;

  /* Process all pending messages (non-blocking) */
  while (FEB_UART_QUEUE_RECEIVE(ctx[inst].tx_queue, &msg, 0))
  {
    FEB_UART_Write(instance, msg.data, msg.len);
  }
}

bool FEB_UART_IsTxQueueEnabled(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_BOOL(instance);
  return ctx[instance].initialized && ctx[instance].tx_queue_enabled;
}

#endif /* FEB_UART_ENABLE_QUEUES */
