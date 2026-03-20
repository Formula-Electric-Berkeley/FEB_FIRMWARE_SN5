/**
 ******************************************************************************
 * @file           : feb_uart.h
 * @brief          : Public API for FEB UART Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides a comprehensive UART interface with:
 *   - Multi-instance support (up to FEB_UART_MAX_INSTANCES UARTs)
 *   - DMA-based non-blocking TX/RX
 *   - Line mode (console) and Binary mode (device-to-device) with framing
 *   - FreeRTOS-optional thread safety
 *   - Printf/scanf redirection with buffering
 *   - Optional FreeRTOS queue support
 *
 * Usage:
 *   1. Configure UART + DMA in STM32CubeMX
 *   2. Add library to CMakeLists.txt
 *   3. Call FEB_UART_Init(instance, &config)
 *   4. Route HAL callbacks to library functions
 *   5. Use FEB_UART_Printf() or printf() for output
 *   6. Call FEB_UART_ProcessRx() in main loop for input
 *
 * Example:
 *   FEB_UART_Config_t cfg = {
 *     .huart = &huart2,
 *     .hdma_tx = &hdma_usart2_tx,
 *     .hdma_rx = &hdma_usart2_rx,
 *     .tx_buffer = my_tx_buf,
 *     .tx_buffer_size = sizeof(my_tx_buf),
 *     .rx_buffer = my_rx_buf,
 *     .rx_buffer_size = sizeof(my_rx_buf),
 *   };
 *   FEB_UART_Init(FEB_UART_INSTANCE_1, &cfg);
 *
 ******************************************************************************
 */

#ifndef FEB_UART_H
#define FEB_UART_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "feb_uart_config.h"

  /* ============================================================================
   * Forward Declarations
   * ============================================================================
   *
   * Forward declare HAL types to avoid requiring STM32 headers in user includes.
   * The actual types are defined in stm32f4xx_hal_uart.h / stm32f4xx_hal_dma.h
   */

  struct __UART_HandleTypeDef;
  typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

  struct __DMA_HandleTypeDef;
  typedef struct __DMA_HandleTypeDef DMA_HandleTypeDef;

  /* ============================================================================
   * FreeRTOS Sync Primitive Types (forward declarations)
   * ============================================================================
   *
   * These are forward declared to avoid requiring FreeRTOS headers in user code.
   * In FreeRTOS mode, these map to osMutexId_t, osSemaphoreId_t, osMessageQueueId_t.
   * In bare-metal mode, these are unused but kept for API consistency.
   */
#if FEB_UART_USE_FREERTOS
#include "cmsis_os2.h"
  typedef osMutexId_t FEB_UART_MutexHandle_t;
  typedef osSemaphoreId_t FEB_UART_SemaphoreHandle_t;
  typedef osMessageQueueId_t FEB_UART_QueueHandle_t;
#else
typedef void *FEB_UART_MutexHandle_t;
typedef void *FEB_UART_SemaphoreHandle_t;
typedef void *FEB_UART_QueueHandle_t;
#endif

  /* ============================================================================
   * Error Codes
   * ============================================================================ */

  /**
   * @brief UART library error codes
   */
  typedef enum
  {
    FEB_UART_OK = 0,                /**< Success */
    FEB_UART_ERR_INVALID_ARG = -1,  /**< Invalid argument */
    FEB_UART_ERR_NOT_INIT = -2,     /**< Instance not initialized */
    FEB_UART_ERR_TIMEOUT = -3,      /**< Operation timeout */
    FEB_UART_ERR_BUFFER_FULL = -4,  /**< TX buffer full */
    FEB_UART_ERR_NO_MUTEX = -5,     /**< Required mutex not provided (FreeRTOS) */
    FEB_UART_ERR_NO_SEMAPHORE = -6, /**< Required semaphore not provided (FreeRTOS) */
    FEB_UART_ERR_NO_QUEUE = -7,     /**< Required queue not provided (FreeRTOS) */
  } FEB_UART_Error_t;

  /* ============================================================================
   * Instance Enumeration
   * ============================================================================ */

  /**
   * @brief UART instance identifiers
   *
   * Each instance is independent with its own TX/RX buffers, callbacks, and
   * optional FreeRTOS queues. Use FEB_UART_INSTANCE_1 for boards with one UART.
   */
  typedef enum
  {
    FEB_UART_INSTANCE_1 = 0, /**< First UART instance */
    FEB_UART_INSTANCE_2 = 1, /**< Second UART instance */
    FEB_UART_INSTANCE_COUNT  /**< Number of instances (for array sizing) */
  } FEB_UART_Instance_t;

  /* ============================================================================
   * Operating Mode Enumeration
   * ============================================================================ */

  /**
   * @brief UART operating modes
   *
   * LINE mode: Line-based reception with callbacks on \\r\\n
   * BINARY mode: Raw byte reception with optional framing
   */
  typedef enum
  {
    FEB_UART_MODE_LINE = 0,  /**< Line-based mode (callback on \\r\\n) */
    FEB_UART_MODE_BINARY = 1 /**< Binary mode (raw bytes or framed packets) */
  } FEB_UART_Mode_t;

  /* ============================================================================
   * Framing Configuration (for Binary mode)
   * ============================================================================ */

  /**
   * @brief Frame configuration for binary mode
   *
   * When enabled, the UART library will detect and extract frames
   * delimited by start/end bytes, with optional byte stuffing (escaping).
   *
   * Example (HDLC-style):
   *   .enable_framing = true,
   *   .start_delimiter = 0x7E,
   *   .end_delimiter = 0x7E,
   *   .escape_enabled = true,
   *   .escape_char = 0x7D,
   *   .max_frame_size = 256,
   */
  typedef struct
  {
    bool enable_framing;     /**< Enable frame detection */
    uint8_t start_delimiter; /**< Frame start byte (e.g., 0x7E) */
    uint8_t end_delimiter;   /**< Frame end byte (can be same as start) */
    bool escape_enabled;     /**< Enable byte stuffing */
    uint8_t escape_char;     /**< Escape character (e.g., 0x7D) */
    uint16_t max_frame_size; /**< Maximum frame size */
  } FEB_UART_FramingConfig_t;

  /* ============================================================================
   * Configuration Structure
   * ============================================================================ */

  /**
   * @brief UART library initialization configuration
   *
   * All pointers to buffers and handles must remain valid for the lifetime
   * of the library. Buffers are user-provided to allow static allocation.
   *
   * FreeRTOS Mode (FEB_UART_USE_FREERTOS == 1):
   *   - tx_mutex is REQUIRED (created in CubeMX .ioc, passed here)
   *   - tx_complete_sem is REQUIRED for blocking TX operations
   *   - rx_queue/tx_queue are optional (if queue mode enabled)
   *
   * Bare-Metal Mode (FEB_UART_USE_FREERTOS == 0):
   *   - Sync primitive fields are ignored
   *   - Set FEB_UART_FORCE_BARE_METAL=1 if ISR protection is needed
   */
  typedef struct
  {
    /* Required: UART peripheral */
    UART_HandleTypeDef *huart; /**< HAL UART handle (must be initialized) */

    /* Optional: DMA handles (NULL for polling mode) */
    DMA_HandleTypeDef *hdma_tx; /**< DMA TX handle (NULL = polling TX) */
    DMA_HandleTypeDef *hdma_rx; /**< DMA RX handle (NULL = polling RX) */

    /* Required: TX buffer (user-provided) */
    uint8_t *tx_buffer;    /**< Pointer to TX ring buffer */
    size_t tx_buffer_size; /**< TX buffer size in bytes (recommend 512+) */

    /* Required: RX buffer (user-provided) */
    uint8_t *rx_buffer;    /**< Pointer to RX circular DMA buffer */
    size_t rx_buffer_size; /**< RX buffer size in bytes (recommend 256+) */

    /* Optional: Timestamp source (defaults to HAL_GetTick if NULL) */
    uint32_t (*get_tick_ms)(void); /**< Function returning millisecond tick */

#if FEB_UART_USE_FREERTOS
    /* ========================================================================
     * REQUIRED Sync Primitives (FreeRTOS mode)
     * ========================================================================
     *
     * These MUST be created by the user (typically via CubeMX .ioc) and passed
     * to this config. The library does NOT create these internally.
     *
     * Init will return FEB_UART_ERR_NO_MUTEX/SEMAPHORE if required fields are NULL.
     */

    /** @brief TX mutex - REQUIRED. Protects TX buffer access. Create in CubeMX. */
    FEB_UART_MutexHandle_t tx_mutex;

    /** @brief TX complete semaphore - REQUIRED. Binary semaphore for DMA completion. */
    FEB_UART_SemaphoreHandle_t tx_complete_sem;

    /* ========================================================================
     * Optional Queue Configuration (FreeRTOS mode)
     * ========================================================================
     *
     * Enable queue mode for decoupled TX/RX processing. When enabled, the
     * corresponding queue handle must be provided.
     */
    bool enable_rx_queue; /**< Enable RX line queue mode (disables callback) */
    bool enable_tx_queue; /**< Enable TX queue mode */

    /** @brief RX queue - Required if enable_rx_queue is true. For received lines. */
    FEB_UART_QueueHandle_t rx_queue;

    /** @brief TX queue - Required if enable_tx_queue is true. For queued TX data. */
    FEB_UART_QueueHandle_t tx_queue;
#endif

  } FEB_UART_Config_t;

  /* ============================================================================
   * Callback Types
   * ============================================================================ */

  /**
   * @brief Callback for received line/command (LINE mode)
   *
   * Called from FEB_UART_ProcessRx() when a complete line is received.
   * Line is null-terminated and does not include line ending characters.
   *
   * @param line Pointer to null-terminated line (without \\n or \\r)
   * @param len  Length of line in bytes (not including null terminator)
   *
   * @note Called from main loop context, not ISR context
   * @note Line buffer is reused after callback returns - copy if needed
   */
  typedef void (*FEB_UART_RxLineCallback_t)(const char *line, size_t len);

  /**
   * @brief Callback for binary data reception (BINARY mode)
   *
   * Called from FEB_UART_ProcessRx() when binary data is received.
   * If framing is enabled, data contains the unescaped frame payload.
   *
   * @param instance UART instance that received data
   * @param data     Pointer to received data
   * @param len      Length of data in bytes
   *
   * @note Called from main loop context, not ISR context
   * @note Data buffer is reused after callback returns - copy if needed
   */
  typedef void (*FEB_UART_RxBinaryCallback_t)(FEB_UART_Instance_t instance, const uint8_t *data, size_t len);

  /* ============================================================================
   * Initialization API
   * ============================================================================ */

  /**
   * @brief Initialize a UART instance
   *
   * Sets up the TX ring buffer, RX circular DMA, and printf redirection
   * (for instance 0 only). Multiple instances can be initialized independently.
   *
   * @param instance UART instance to initialize
   * @param config   Pointer to configuration structure
   * @return 0 on success, negative error code on failure
   *
   * @note Configuration structure and buffers must remain valid until DeInit
   * @note Printf/scanf redirection uses instance 0 only
   */
  int FEB_UART_Init(FEB_UART_Instance_t instance, const FEB_UART_Config_t *config);

  /**
   * @brief Deinitialize a UART instance
   *
   * Stops DMA transfers and releases resources for the specified instance.
   *
   * @param instance UART instance to deinitialize
   */
  void FEB_UART_DeInit(FEB_UART_Instance_t instance);

  /**
   * @brief Check if instance is initialized
   *
   * @param instance UART instance to check
   * @return true if initialized, false otherwise
   */
  bool FEB_UART_IsInitialized(FEB_UART_Instance_t instance);

  /* ============================================================================
   * Mode Configuration API
   * ============================================================================ */

  /**
   * @brief Set UART operating mode
   *
   * @param instance UART instance
   * @param mode     FEB_UART_MODE_LINE or FEB_UART_MODE_BINARY
   * @return 0 on success, -1 on error
   *
   * @note Clears any pending RX data when mode changes
   */
  int FEB_UART_SetMode(FEB_UART_Instance_t instance, FEB_UART_Mode_t mode);

  /**
   * @brief Get current operating mode
   *
   * @param instance UART instance
   * @return Current mode
   */
  FEB_UART_Mode_t FEB_UART_GetMode(FEB_UART_Instance_t instance);

  /**
   * @brief Configure binary mode framing
   *
   * @param instance UART instance
   * @param config   Framing configuration (NULL to disable framing)
   *
   * @note Only applies in BINARY mode
   */
  void FEB_UART_SetFramingConfig(FEB_UART_Instance_t instance, const FEB_UART_FramingConfig_t *config);

  /* ============================================================================
   * Output API
   * ============================================================================ */

  /**
   * @brief Printf-style formatted output to UART
   *
   * Formats the message and queues it for DMA transmission.
   * Non-blocking unless TX buffer is full.
   *
   * @param instance UART instance
   * @param format   Printf format string
   * @param ...      Variable arguments
   * @return Number of bytes queued, or negative on error
   *
   * @note Thread-safe when FreeRTOS is enabled
   * @note Safe to call from ISR (uses separate path)
   */
  int FEB_UART_Printf(FEB_UART_Instance_t instance, const char *format, ...) __attribute__((format(printf, 2, 3)));

  /**
   * @brief Write raw bytes to UART
   *
   * @param instance UART instance
   * @param data     Pointer to data to send
   * @param len      Number of bytes to send
   * @return Number of bytes queued, or negative on error
   */
  int FEB_UART_Write(FEB_UART_Instance_t instance, const uint8_t *data, size_t len);

  /**
   * @brief Write binary data with optional framing
   *
   * @param instance    UART instance
   * @param data        Data to send
   * @param len         Length of data
   * @param add_framing If true and framing enabled, wraps data with delimiters
   * @return Number of bytes queued (before framing), or negative on error
   *
   * @note When add_framing is true, adds start/end delimiters and escapes
   *       any occurrences of delimiter/escape bytes in the data
   */
  int FEB_UART_WriteBinary(FEB_UART_Instance_t instance, const uint8_t *data, size_t len, bool add_framing);

  /**
   * @brief Flush pending TX data
   *
   * Blocks until all queued data is transmitted or timeout expires.
   *
   * @param instance   UART instance
   * @param timeout_ms Maximum time to wait in milliseconds (0 = infinite)
   * @return 0 on success, -1 on timeout
   */
  int FEB_UART_Flush(FEB_UART_Instance_t instance, uint32_t timeout_ms);

  /**
   * @brief Get number of bytes pending in TX buffer
   *
   * @param instance UART instance
   * @return Number of bytes waiting to be transmitted
   */
  size_t FEB_UART_TxPending(FEB_UART_Instance_t instance);

  /* ============================================================================
   * Input API
   * ============================================================================ */

  /**
   * @brief Register callback for received lines (LINE mode)
   *
   * @param instance UART instance
   * @param callback Function to call, or NULL to disable
   */
  void FEB_UART_SetRxLineCallback(FEB_UART_Instance_t instance, FEB_UART_RxLineCallback_t callback);

  /**
   * @brief Register callback for binary data (BINARY mode)
   *
   * @param instance        UART instance
   * @param callback        Function to call when data received
   * @param min_bytes       Minimum bytes before callback (0 = any data)
   * @param idle_timeout_ms Trigger callback after idle (0 = disabled)
   */
  void FEB_UART_SetRxBinaryCallback(FEB_UART_Instance_t instance, FEB_UART_RxBinaryCallback_t callback,
                                    size_t min_bytes, uint32_t idle_timeout_ms);

  /**
   * @brief Process received data and invoke callbacks
   *
   * Must be called periodically from the main loop or a SINGLE RTOS task.
   *
   * @param instance UART instance
   */
  void FEB_UART_ProcessRx(FEB_UART_Instance_t instance);

  /**
   * @brief Check if RX data is available
   *
   * @param instance UART instance
   * @return Number of unprocessed bytes in RX buffer
   */
  size_t FEB_UART_RxAvailable(FEB_UART_Instance_t instance);

  /**
   * @brief Read raw bytes from RX buffer
   *
   * @param instance UART instance
   * @param data     Destination buffer
   * @param max_len  Maximum bytes to read
   * @return Number of bytes actually read
   */
  size_t FEB_UART_Read(FEB_UART_Instance_t instance, uint8_t *data, size_t max_len);

  /* ============================================================================
   * HAL Callback Integration
   * ============================================================================
   *
   * These callbacks auto-route to the correct instance by matching the huart
   * handle. Simply call them from HAL callbacks without specifying instance.
   */

  /**
   * @brief Call from HAL_UART_TxCpltCallback()
   */
  void FEB_UART_TxCpltCallback(UART_HandleTypeDef *huart);

  /**
   * @brief Call from HAL_UARTEx_RxEventCallback()
   */
  void FEB_UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size);

  /**
   * @brief Call from USARTx_IRQHandler for IDLE line detection
   */
  void FEB_UART_IDLE_Callback(UART_HandleTypeDef *huart);

  /* ============================================================================
   * Weak Task Functions (FreeRTOS Mode)
   * ============================================================================
   *
   * These are weak default implementations that can be overridden by user code.
   * Create tasks in CubeMX .ioc pointing to these entry functions.
   *
   * CubeMX Task Configuration:
   *   - Task name: e.g., "UART_RX_Task"
   *   - Entry function: FEB_UART_RxTaskFunc
   *   - Argument: (void*)(uintptr_t)FEB_UART_INSTANCE_1
   *   - Stack size: User choice (recommend 256+ words)
   *   - Priority: User choice
   *
   * To override, define a non-weak version in user code:
   *   void FEB_UART_RxTaskFunc(void *argument) {
   *     FEB_UART_Instance_t inst = (FEB_UART_Instance_t)(uintptr_t)argument;
   *     for (;;) {
   *       FEB_UART_ProcessRx(inst);
   *       // Custom user processing...
   *       osDelay(5);
   *     }
   *   }
   */

#if FEB_UART_USE_FREERTOS

  /**
   * @brief Weak default RX processing task entry function
   *
   * @param argument UART instance cast to void* (e.g., (void*)FEB_UART_INSTANCE_1)
   */
  void FEB_UART_RxTaskFunc(void *argument);

  /**
   * @brief Weak default TX queue processing task entry function
   *
   * @param argument UART instance cast to void* (e.g., (void*)FEB_UART_INSTANCE_1)
   */
  void FEB_UART_TxTaskFunc(void *argument);

#endif /* FEB_UART_USE_FREERTOS */

  /* ============================================================================
   * Queue-Based API (FreeRTOS only)
   * ============================================================================ */

#if FEB_UART_ENABLE_QUEUES

  bool FEB_UART_QueueReceiveLine(FEB_UART_Instance_t instance, char *buffer, size_t max_len, size_t *out_len,
                                 uint32_t timeout);
  uint32_t FEB_UART_RxQueueCount(FEB_UART_Instance_t instance);
  bool FEB_UART_IsRxQueueEnabled(FEB_UART_Instance_t instance);
  uint32_t FEB_UART_GetRxQueueDrops(FEB_UART_Instance_t instance);
  int FEB_UART_QueueWrite(FEB_UART_Instance_t instance, const uint8_t *data, size_t len, uint32_t timeout);
  int FEB_UART_QueuePrintf(FEB_UART_Instance_t instance, uint32_t timeout, const char *format, ...)
      __attribute__((format(printf, 3, 4)));
  void FEB_UART_ProcessTxQueue(FEB_UART_Instance_t instance);
  bool FEB_UART_IsTxQueueEnabled(FEB_UART_Instance_t instance);

#endif /* FEB_UART_ENABLE_QUEUES */

#ifdef __cplusplus
}
#endif

#endif /* FEB_UART_H */
