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
  bool initialized;

  /* Operating mode */
  FEB_UART_Mode_t mode;             /* LINE or BINARY */
  FEB_UART_FramingConfig_t framing; /* Framing config for binary mode */
  FEB_UART_RxBinaryCallback_t rx_binary_callback;
  size_t rx_binary_min_bytes;         /* Min bytes before callback */
  uint32_t rx_binary_idle_timeout_ms; /* Idle timeout for callback */
  uint32_t rx_last_data_tick;         /* Last time data was received */
  bool rx_in_frame;                   /* Currently receiving a frame */
  bool rx_escape_next;                /* Next byte is escaped */

  /* TX state */
  FEB_UART_RingBuffer_t tx_ring;
  FEB_UART_TxState_t tx_state;
  size_t tx_dma_len;

#if FEB_UART_USE_FREERTOS
  /* User-provided sync primitives (FreeRTOS mode) */
  FEB_UART_MutexHandle_t tx_mutex;            /* User-created mutex */
  FEB_UART_SemaphoreHandle_t tx_complete_sem; /* User-created semaphore */
#else
  FEB_UART_Mutex_t tx_mutex; /* Bare-metal: PRIMASK storage */
#endif

  /* RX state */
  uint8_t *rx_buffer;
  size_t rx_buffer_size;
  volatile size_t rx_head; /* Updated by DMA/ISR */
  size_t rx_tail;          /* Updated by ProcessRx */
  FEB_UART_RxLineCallback_t rx_line_callback;
  FEB_UART_LineBuffer_t line_buffer;
  bool last_was_line_ending; /* Track \r\n and \n\r sequences */

#if FEB_UART_USE_FREERTOS
  /* Queue state - user-provided handles */
  FEB_UART_QueueHandle_t rx_queue;
  FEB_UART_QueueHandle_t tx_queue;
  bool rx_queue_enabled;
  bool tx_queue_enabled;
  uint32_t rx_queue_drops; /**< Count of dropped messages when queue full */
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
    return FEB_UART_ERR_INVALID_ARG;
  }

  if (config->tx_buffer == NULL || config->tx_buffer_size == 0)
  {
    return FEB_UART_ERR_INVALID_ARG;
  }

  if (config->rx_buffer == NULL || config->rx_buffer_size == 0)
  {
    return FEB_UART_ERR_INVALID_ARG;
  }

#if FEB_UART_USE_FREERTOS
  /* Validate REQUIRED sync primitives in FreeRTOS mode */
  if (config->tx_mutex == NULL)
  {
    return FEB_UART_ERR_NO_MUTEX;
  }
  if (config->tx_complete_sem == NULL)
  {
    return FEB_UART_ERR_NO_SEMAPHORE;
  }

  /* Validate optional queue handles if queue mode is enabled */
  if (config->enable_rx_queue && config->rx_queue == NULL)
  {
    return FEB_UART_ERR_NO_QUEUE;
  }
  if (config->enable_tx_queue && config->tx_queue == NULL)
  {
    return FEB_UART_ERR_NO_QUEUE;
  }
#endif

  /* Store configuration */
  ctx[inst].huart = config->huart;
  ctx[inst].hdma_tx = config->hdma_tx;
  ctx[inst].hdma_rx = config->hdma_rx;
  ctx[inst].get_tick_ms = config->get_tick_ms ? config->get_tick_ms : default_get_tick;

  /* Initialize TX ring buffer */
  feb_uart_ring_init(&ctx[inst].tx_ring, config->tx_buffer, config->tx_buffer_size);
  ctx[inst].tx_state = FEB_UART_TX_IDLE;
  ctx[inst].tx_dma_len = 0;

#if FEB_UART_USE_FREERTOS
  /* Store user-provided sync primitives (NOT created internally) */
  ctx[inst].tx_mutex = config->tx_mutex;
  ctx[inst].tx_complete_sem = config->tx_complete_sem;
#endif

  /* Initialize RX state */
  ctx[inst].rx_buffer = config->rx_buffer;
  ctx[inst].rx_buffer_size = config->rx_buffer_size;
  ctx[inst].rx_head = 0;
  ctx[inst].rx_tail = 0;
  ctx[inst].rx_line_callback = NULL;
  ctx[inst].line_buffer.len = 0;
  ctx[inst].last_was_line_ending = false;

  /* Initialize binary mode state */
  ctx[inst].mode = FEB_UART_MODE_LINE; /* Default to line mode */
  memset(&ctx[inst].framing, 0, sizeof(ctx[inst].framing));
  ctx[inst].rx_binary_callback = NULL;
  ctx[inst].rx_binary_min_bytes = 0;
  ctx[inst].rx_binary_idle_timeout_ms = 0;
  ctx[inst].rx_last_data_tick = 0;
  ctx[inst].rx_in_frame = false;
  ctx[inst].rx_escape_next = false;

  /* Start DMA RX if DMA available */
  if (ctx[inst].hdma_rx != NULL)
  {
    /* Start DMA reception FIRST (before enabling IDLE interrupt) */
    HAL_StatusTypeDef status =
        HAL_UARTEx_ReceiveToIdle_DMA(ctx[inst].huart, ctx[inst].rx_buffer, ctx[inst].rx_buffer_size);

    /* If DMA start failed, try to recover by aborting and retrying */
    if (status != HAL_OK)
    {
      HAL_UART_Abort(ctx[inst].huart);
      status = HAL_UARTEx_ReceiveToIdle_DMA(ctx[inst].huart, ctx[inst].rx_buffer, ctx[inst].rx_buffer_size);
    }

    if (status == HAL_OK)
    {
      /* Disable half-transfer interrupt (we only care about IDLE and complete) */
      __HAL_DMA_DISABLE_IT(ctx[inst].hdma_rx, DMA_IT_HT);
    }
  }

  /* ALWAYS enable IDLE line interrupt - required for RX to work even if DMA fails */
  __HAL_UART_ENABLE_IT(ctx[inst].huart, UART_IT_IDLE);

#if FEB_UART_USE_FREERTOS
  /* Store user-provided queue handles (NOT created internally) */
  ctx[inst].rx_queue_enabled = config->enable_rx_queue;
  ctx[inst].tx_queue_enabled = config->enable_tx_queue;
  ctx[inst].rx_queue = config->rx_queue;
  ctx[inst].tx_queue = config->tx_queue;
  ctx[inst].rx_queue_drops = 0;
#endif

  ctx[inst].initialized = true;

  return FEB_UART_OK;
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

  /*
   * NOTE: We do NOT delete user-provided sync primitives (mutex, semaphore, queues).
   * The user created them in CubeMX/.ioc and owns their lifecycle.
   * We only clear our references to them.
   */

#if FEB_UART_USE_FREERTOS
  ctx[inst].tx_mutex = NULL;
  ctx[inst].tx_complete_sem = NULL;
  ctx[inst].rx_queue = NULL;
  ctx[inst].tx_queue = NULL;
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
 * Mode Configuration
 * ============================================================================ */

int FEB_UART_SetMode(FEB_UART_Instance_t instance, FEB_UART_Mode_t mode)
{
  VALIDATE_INSTANCE_INIT(instance);
  int inst = (int)instance;

  /* Clear pending RX data when mode changes */
  ctx[inst].rx_tail = ctx[inst].rx_head;
  ctx[inst].line_buffer.len = 0;
  ctx[inst].last_was_line_ending = false;
  ctx[inst].rx_in_frame = false;
  ctx[inst].rx_escape_next = false;

  ctx[inst].mode = mode;
  return 0;
}

FEB_UART_Mode_t FEB_UART_GetMode(FEB_UART_Instance_t instance)
{
  if (instance >= FEB_UART_MAX_INSTANCES || !ctx[instance].initialized)
  {
    return FEB_UART_MODE_LINE;
  }
  return ctx[instance].mode;
}

void FEB_UART_SetFramingConfig(FEB_UART_Instance_t instance, const FEB_UART_FramingConfig_t *config)
{
  VALIDATE_INSTANCE_VOID(instance);

  if (config != NULL)
  {
    ctx[instance].framing = *config;
  }
  else
  {
    memset(&ctx[instance].framing, 0, sizeof(ctx[instance].framing));
  }
  ctx[instance].rx_in_frame = false;
  ctx[instance].rx_escape_next = false;
}

void FEB_UART_SetRxBinaryCallback(FEB_UART_Instance_t instance, FEB_UART_RxBinaryCallback_t callback, size_t min_bytes,
                                  uint32_t idle_timeout_ms)
{
  VALIDATE_INSTANCE_VOID(instance);
  ctx[instance].rx_binary_callback = callback;
  ctx[instance].rx_binary_min_bytes = min_bytes;
  ctx[instance].rx_binary_idle_timeout_ms = idle_timeout_ms;
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

int FEB_UART_WriteBinary(FEB_UART_Instance_t instance, const uint8_t *data, size_t len, bool add_framing)
{
  VALIDATE_INSTANCE_INIT(instance);
  int inst = (int)instance;

  if (data == NULL || len == 0)
  {
    return -1;
  }

  /* If framing not enabled or not requested, just write raw */
  if (!add_framing || !ctx[inst].framing.enable_framing)
  {
    return FEB_UART_Write(instance, data, len);
  }

  /* With framing: write start delimiter, escaped data, end delimiter */
  int total_written = 0;
  bool in_isr = FEB_UART_IN_ISR();

#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_LOCK(ctx[inst].tx_mutex);
  }
#else
  (void)in_isr;
  FEB_UART_ENTER_CRITICAL();
#endif

  /* Write start delimiter */
  uint8_t delim = ctx[inst].framing.start_delimiter;
  feb_uart_write_internal(inst, &delim, 1);
  total_written++;

  /* Write data with escaping if enabled */
  for (size_t i = 0; i < len; i++)
  {
    uint8_t byte = data[i];

    if (ctx[inst].framing.escape_enabled)
    {
      /* Check if byte needs escaping */
      if (byte == ctx[inst].framing.start_delimiter || byte == ctx[inst].framing.end_delimiter ||
          byte == ctx[inst].framing.escape_char)
      {
        /* Write escape char + XOR'd byte (HDLC style) */
        uint8_t esc = ctx[inst].framing.escape_char;
        feb_uart_write_internal(inst, &esc, 1);
        byte ^= 0x20; /* HDLC XOR */
        total_written++;
      }
    }

    feb_uart_write_internal(inst, &byte, 1);
    total_written++;
  }

  /* Write end delimiter */
  delim = ctx[inst].framing.end_delimiter;
  feb_uart_write_internal(inst, &delim, 1);
  total_written++;

#if FEB_UART_USE_FREERTOS
  if (!in_isr)
  {
    FEB_UART_MUTEX_UNLOCK(ctx[inst].tx_mutex);
  }
#else
  FEB_UART_EXIT_CRITICAL();
#endif

  return (int)len; /* Return original data length, not framed length */
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

// DEBUG: Set to 1 to enable verbose RX debugging
#define FEB_UART_DEBUG_RX 0

/* Forward declaration for binary processing */
static void process_rx_binary(int inst);
static void process_rx_line(int inst);

void FEB_UART_ProcessRx(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_VOID(instance);
  int inst = (int)instance;

  /* Dispatch based on mode */
  if (ctx[inst].mode == FEB_UART_MODE_BINARY)
  {
    process_rx_binary(inst);
  }
  else
  {
    process_rx_line(inst);
  }
}

/**
 * @brief Process RX in binary mode
 */
static void process_rx_binary(int inst)
{
  size_t count = get_rx_count(inst);

  if (count == 0)
  {
    /* Check idle timeout */
    if (ctx[inst].rx_binary_idle_timeout_ms > 0 && ctx[inst].line_buffer.len > 0)
    {
      uint32_t now = ctx[inst].get_tick_ms ? ctx[inst].get_tick_ms() : 0;
      if ((now - ctx[inst].rx_last_data_tick) >= ctx[inst].rx_binary_idle_timeout_ms)
      {
        /* Idle timeout - deliver accumulated data */
        if (ctx[inst].rx_binary_callback != NULL)
        {
          ctx[inst].rx_binary_callback((FEB_UART_Instance_t)inst, (const uint8_t *)ctx[inst].line_buffer.buffer,
                                       ctx[inst].line_buffer.len);
        }
        ctx[inst].line_buffer.len = 0;
      }
    }
    return;
  }

  /* Update last data timestamp */
  ctx[inst].rx_last_data_tick = ctx[inst].get_tick_ms ? ctx[inst].get_tick_ms() : 0;

  while (count > 0)
  {
    uint8_t byte = ctx[inst].rx_buffer[ctx[inst].rx_tail];
    ctx[inst].rx_tail = (ctx[inst].rx_tail + 1) % ctx[inst].rx_buffer_size;
    count--;

    /* Process with framing if enabled */
    if (ctx[inst].framing.enable_framing)
    {
      /* Handle escape sequences */
      if (ctx[inst].rx_escape_next)
      {
        byte ^= 0x20; /* HDLC XOR */
        ctx[inst].rx_escape_next = false;
      }
      else if (ctx[inst].framing.escape_enabled && byte == ctx[inst].framing.escape_char)
      {
        ctx[inst].rx_escape_next = true;
        continue;
      }
      else if (byte == ctx[inst].framing.start_delimiter)
      {
        /* Start of frame - reset buffer */
        ctx[inst].line_buffer.len = 0;
        ctx[inst].rx_in_frame = true;
        continue;
      }
      else if (byte == ctx[inst].framing.end_delimiter)
      {
        /* End of frame - deliver if we have data */
        if (ctx[inst].rx_in_frame && ctx[inst].line_buffer.len > 0)
        {
          if (ctx[inst].rx_binary_callback != NULL)
          {
            ctx[inst].rx_binary_callback((FEB_UART_Instance_t)inst, (const uint8_t *)ctx[inst].line_buffer.buffer,
                                         ctx[inst].line_buffer.len);
          }
        }
        ctx[inst].line_buffer.len = 0;
        ctx[inst].rx_in_frame = false;
        continue;
      }

      /* Only add byte if we're in a frame */
      if (!ctx[inst].rx_in_frame)
      {
        continue;
      }
    }

    /* Add byte to buffer */
    if (ctx[inst].line_buffer.len < sizeof(ctx[inst].line_buffer.buffer) - 1)
    {
      ctx[inst].line_buffer.buffer[ctx[inst].line_buffer.len++] = (char)byte;
    }

    /* Check min_bytes threshold (only without framing) */
    if (!ctx[inst].framing.enable_framing && ctx[inst].rx_binary_min_bytes > 0 &&
        ctx[inst].line_buffer.len >= ctx[inst].rx_binary_min_bytes)
    {
      if (ctx[inst].rx_binary_callback != NULL)
      {
        ctx[inst].rx_binary_callback((FEB_UART_Instance_t)inst, (const uint8_t *)ctx[inst].line_buffer.buffer,
                                     ctx[inst].line_buffer.len);
      }
      ctx[inst].line_buffer.len = 0;
    }
  }
}

/**
 * @brief Process RX in line mode (original behavior)
 */
static void process_rx_line(int inst)
{
  size_t count = get_rx_count(inst);

#if FEB_UART_DEBUG_RX
  if (count > 0)
  {
    printf("[RX] count=%u head=%u tail=%u\r\n", (unsigned)count, (unsigned)ctx[inst].rx_head,
           (unsigned)ctx[inst].rx_tail);
  }
#endif

  while (count > 0)
  {
    uint8_t byte = ctx[inst].rx_buffer[ctx[inst].rx_tail];
    ctx[inst].rx_tail = (ctx[inst].rx_tail + 1) % ctx[inst].rx_buffer_size;
    count--;

#if FEB_UART_DEBUG_RX
    // Print each byte: printable chars as-is, control chars as hex
    if (byte >= 0x20 && byte < 0x7F)
    {
      printf("[RX] byte='%c' (0x%02X) linebuf_len=%u last_le=%d\r\n", byte, byte, (unsigned)ctx[inst].line_buffer.len,
             ctx[inst].last_was_line_ending);
    }
    else
    {
      printf("[RX] byte=0x%02X linebuf_len=%u last_le=%d\r\n", byte, (unsigned)ctx[inst].line_buffer.len,
             ctx[inst].last_was_line_ending);
    }
#endif

    /* Check if this is a line ending character (\r or \n) */
    bool is_line_ending = (byte == '\r' || byte == '\n');

    if (is_line_ending)
    {
      /* Skip if this is the second char of a \r\n or \n\r sequence */
      if (ctx[inst].last_was_line_ending)
      {
#if FEB_UART_DEBUG_RX
        printf("[RX] skipping second line ending\r\n");
#endif
        ctx[inst].last_was_line_ending = false;
        continue;
      }

#if FEB_UART_DEBUG_RX
      printf("[RX] LINE ENDING detected, firing callback with '%s' len=%u\r\n", ctx[inst].line_buffer.buffer,
             (unsigned)ctx[inst].line_buffer.len);
#endif

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
          if (!FEB_UART_QUEUE_SEND(ctx[inst].rx_queue, &msg, 0))
          {
            ctx[inst].rx_queue_drops++; /* Track dropped messages */
          }
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

uint32_t FEB_UART_GetRxQueueDrops(FEB_UART_Instance_t instance)
{
  VALIDATE_INSTANCE_ZERO(instance);
  return ctx[instance].rx_queue_drops;
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

#endif /* FEB_UART_USE_FREERTOS - Queue API */

/* ============================================================================
 * Weak Task Function Implementations (FreeRTOS Mode)
 * ============================================================================
 *
 * These are default implementations that users can override by defining
 * their own non-weak versions. Create tasks in CubeMX .ioc pointing to
 * these entry functions.
 *
 * Override example:
 *   void FEB_UART_RxTaskFunc(void *argument) {
 *     FEB_UART_Instance_t inst = (FEB_UART_Instance_t)(uintptr_t)argument;
 *     for (;;) {
 *       FEB_UART_ProcessRx(inst);
 *       // User's custom processing...
 *       osDelay(5);
 *     }
 *   }
 */

#if FEB_UART_USE_FREERTOS

#include <stdint.h>

/**
 * @brief Weak default RX processing task
 *
 * Processes received UART data in a loop. Override to add custom logic.
 *
 * @param argument UART instance cast to void*
 */
__attribute__((weak)) void FEB_UART_RxTaskFunc(void *argument)
{
  FEB_UART_Instance_t inst = (FEB_UART_Instance_t)(uintptr_t)argument;

  for (;;)
  {
    FEB_UART_ProcessRx(inst);
    osDelay(1); /* Yield to other tasks */
  }
}

/**
 * @brief Weak default TX queue processing task
 *
 * Processes queued TX messages. Only needed if TX queue mode is enabled.
 *
 * @param argument UART instance cast to void*
 */
__attribute__((weak)) void FEB_UART_TxTaskFunc(void *argument)
{
  FEB_UART_Instance_t inst = (FEB_UART_Instance_t)(uintptr_t)argument;

  for (;;)
  {
    FEB_UART_ProcessTxQueue(inst);
    osDelay(1); /* Yield to other tasks */
  }
}

#endif /* FEB_UART_USE_FREERTOS */
