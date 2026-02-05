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
 *   - FreeRTOS-optional thread safety
 *   - Printf/scanf redirection with buffering
 *   - Verbosity levels, ANSI colors, timestamps
 *   - Bidirectional command parsing
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
 *     .log_level = FEB_UART_LOG_DEBUG,
 *     .enable_colors = true,
 *     .enable_timestamps = true,
 *     .get_tick_ms = HAL_GetTick,
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
   * Log Level Enumeration
   * ============================================================================ */

  /**
   * @brief UART log verbosity levels
   *
   * Lower values = higher priority. Messages are filtered based on runtime
   * log level setting. Compile-time log level (FEB_UART_COMPILE_LOG_LEVEL)
   * can eliminate code for higher levels entirely.
   */
  typedef enum
  {
    FEB_UART_LOG_NONE = 0,  /**< No logging output */
    FEB_UART_LOG_ERROR = 1, /**< Critical errors only */
    FEB_UART_LOG_WARN = 2,  /**< Warnings and errors */
    FEB_UART_LOG_INFO = 3,  /**< Informational messages */
    FEB_UART_LOG_DEBUG = 4, /**< Debug-level output */
    FEB_UART_LOG_TRACE = 5, /**< Verbose trace output */
  } FEB_UART_LogLevel_t;

  /* ============================================================================
   * Configuration Structure
   * ============================================================================ */

  /**
   * @brief UART library initialization configuration
   *
   * All pointers to buffers and handles must remain valid for the lifetime
   * of the library. Buffers are user-provided to allow static allocation.
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

    /* Logging configuration */
    FEB_UART_LogLevel_t log_level; /**< Initial runtime log level */
    bool enable_colors;            /**< Enable ANSI color codes in output */
    bool enable_timestamps;        /**< Prefix messages with [timestamp_ms] */

    /* Optional: Timestamp source (defaults to HAL_GetTick if NULL) */
    uint32_t (*get_tick_ms)(void); /**< Function returning millisecond tick */

#if FEB_UART_ENABLE_QUEUES
    /* Queue configuration (FreeRTOS only) */
    bool enable_rx_queue; /**< Enable RX line queue mode (disables callback) */
    bool enable_tx_queue; /**< Enable TX queue mode */
#endif

  } FEB_UART_Config_t;

  /* ============================================================================
   * Callback Types
   * ============================================================================ */

  /**
   * @brief Callback for received line/command
   *
   * Called from FEB_UART_ProcessRx() when a complete line is received.
   * Line is null-terminated and does not include line ending characters.
   *
   * Line termination is triggered by:
   *   - \\r (carriage return)
   *   - \\n (line feed)
   *   - \\r\\n or \\n\\r sequences (treated as single termination, no double-trigger)
   *
   * @param line Pointer to null-terminated line (without \\n or \\r)
   * @param len  Length of line in bytes (not including null terminator)
   *
   * @note Called from main loop context, not ISR context
   * @note Line buffer is reused after callback returns - copy if needed
   */
  typedef void (*FEB_UART_RxLineCallback_t)(const char *line, size_t len);

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
   *   - -1: Invalid instance or configuration (NULL pointers)
   *   - -2: DMA initialization failed
   *
   * @note Configuration structure and buffers must remain valid until DeInit
   * @note If using DMA RX, starts circular DMA reception automatically
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
   * Runtime Configuration API
   * ============================================================================ */

  /**
   * @brief Set runtime log verbosity level
   *
   * @param instance UART instance
   * @param level    New log level (FEB_UART_LOG_NONE to FEB_UART_LOG_TRACE)
   */
  void FEB_UART_SetLogLevel(FEB_UART_Instance_t instance, FEB_UART_LogLevel_t level);

  /**
   * @brief Get current runtime log level
   *
   * @param instance UART instance
   * @return Current log level
   */
  FEB_UART_LogLevel_t FEB_UART_GetLogLevel(FEB_UART_Instance_t instance);

  /**
   * @brief Enable or disable ANSI color codes
   *
   * @param instance UART instance
   * @param enable   true to enable colors, false to disable
   */
  void FEB_UART_SetColorsEnabled(FEB_UART_Instance_t instance, bool enable);

  /**
   * @brief Check if colors are enabled
   *
   * @param instance UART instance
   * @return true if colors enabled, false otherwise
   */
  bool FEB_UART_GetColorsEnabled(FEB_UART_Instance_t instance);

  /**
   * @brief Enable or disable timestamps
   *
   * @param instance UART instance
   * @param enable   true to enable timestamps, false to disable
   */
  void FEB_UART_SetTimestampsEnabled(FEB_UART_Instance_t instance, bool enable);

  /**
   * @brief Check if timestamps are enabled
   *
   * @param instance UART instance
   * @return true if timestamps enabled, false otherwise
   */
  bool FEB_UART_GetTimestampsEnabled(FEB_UART_Instance_t instance);

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
   * @brief Register callback for received lines
   *
   * @param instance UART instance
   * @param callback Function to call, or NULL to disable
   */
  void FEB_UART_SetRxLineCallback(FEB_UART_Instance_t instance, FEB_UART_RxLineCallback_t callback);

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
   *
   * Auto-routes to correct instance based on huart handle.
   *
   * @param huart UART handle that completed transmission
   */
  void FEB_UART_TxCpltCallback(UART_HandleTypeDef *huart);

  /**
   * @brief Call from HAL_UARTEx_RxEventCallback()
   *
   * Auto-routes to correct instance based on huart handle.
   *
   * @param huart UART handle
   * @param size  Number of bytes received
   */
  void FEB_UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size);

  /**
   * @brief Call from USARTx_IRQHandler for IDLE line detection
   *
   * Auto-routes to correct instance based on huart handle.
   *
   * @param huart UART handle
   */
  void FEB_UART_IDLE_Callback(UART_HandleTypeDef *huart);

  /* ============================================================================
   * Queue-Based API (FreeRTOS only)
   * ============================================================================ */

#if FEB_UART_ENABLE_QUEUES

  /**
   * @brief Receive a line from the RX queue (blocking)
   *
   * @param instance UART instance
   * @param buffer   Destination buffer for the line
   * @param max_len  Maximum buffer size
   * @param out_len  Output: actual line length (optional, can be NULL)
   * @param timeout  Timeout in ms (osWaitForever for infinite)
   * @return true if line received, false on timeout
   */
  bool FEB_UART_QueueReceiveLine(FEB_UART_Instance_t instance, char *buffer, size_t max_len, size_t *out_len,
                                 uint32_t timeout);

  /**
   * @brief Get number of lines waiting in RX queue
   *
   * @param instance UART instance
   * @return Number of complete lines queued
   */
  uint32_t FEB_UART_RxQueueCount(FEB_UART_Instance_t instance);

  /**
   * @brief Check if RX queue mode is enabled
   *
   * @param instance UART instance
   * @return true if RX queue is active
   */
  bool FEB_UART_IsRxQueueEnabled(FEB_UART_Instance_t instance);

  /**
   * @brief Queue data for transmission (blocking)
   *
   * @param instance UART instance
   * @param data     Data to transmit
   * @param len      Data length
   * @param timeout  Timeout in ms
   * @return Number of bytes queued, or -1 on error
   */
  int FEB_UART_QueueWrite(FEB_UART_Instance_t instance, const uint8_t *data, size_t len, uint32_t timeout);

  /**
   * @brief Queue formatted output for transmission (blocking)
   *
   * @param instance UART instance
   * @param timeout  Timeout in ms
   * @param format   Printf format string
   * @return Number of bytes queued, or -1 on error
   */
  int FEB_UART_QueuePrintf(FEB_UART_Instance_t instance, uint32_t timeout, const char *format, ...)
      __attribute__((format(printf, 3, 4)));

  /**
   * @brief Process TX queue and transmit pending messages
   *
   * @param instance UART instance
   */
  void FEB_UART_ProcessTxQueue(FEB_UART_Instance_t instance);

  /**
   * @brief Check if TX queue mode is enabled
   *
   * @param instance UART instance
   * @return true if TX queue is active
   */
  bool FEB_UART_IsTxQueueEnabled(FEB_UART_Instance_t instance);

#endif /* FEB_UART_ENABLE_QUEUES */

#ifdef __cplusplus
}
#endif

#endif /* FEB_UART_H */
