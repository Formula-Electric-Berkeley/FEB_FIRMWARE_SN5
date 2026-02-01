/**
 ******************************************************************************
 * @file           : feb_uart.h
 * @brief          : Public API for FEB UART Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides a comprehensive UART interface with:
 *   - DMA-based non-blocking TX/RX
 *   - FreeRTOS-optional thread safety
 *   - Printf/scanf redirection with buffering
 *   - Verbosity levels, ANSI colors, timestamps
 *   - Bidirectional command parsing
 *
 * Usage:
 *   1. Configure UART + DMA in STM32CubeMX
 *   2. Add library to CMakeLists.txt
 *   3. Call FEB_UART_Init() with configuration
 *   4. Route HAL callbacks to library functions
 *   5. Use printf() or LOG_I() macros for output
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
 *   FEB_UART_Init(&cfg);
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
  uint8_t *tx_buffer;      /**< Pointer to TX ring buffer */
  size_t tx_buffer_size;   /**< TX buffer size in bytes (recommend 512+) */

  /* Required: RX buffer (user-provided) */
  uint8_t *rx_buffer;      /**< Pointer to RX circular DMA buffer */
  size_t rx_buffer_size;   /**< RX buffer size in bytes (recommend 256+) */

  /* Logging configuration */
  FEB_UART_LogLevel_t log_level; /**< Initial runtime log level */
  bool enable_colors;            /**< Enable ANSI color codes in output */
  bool enable_timestamps;        /**< Prefix messages with [timestamp_ms] */

  /* Optional: Timestamp source (defaults to HAL_GetTick if NULL) */
  uint32_t (*get_tick_ms)(void); /**< Function returning millisecond tick */

} FEB_UART_Config_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for received line/command
 *
 * Called from FEB_UART_ProcessRx() when a complete line is received.
 * Line is null-terminated and does not include the newline character.
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
 * @brief Initialize the UART library
 *
 * Sets up the TX ring buffer, RX circular DMA, and printf redirection.
 * After calling this function, printf() output will go through this library.
 *
 * @param config Pointer to configuration structure
 * @return 0 on success, negative error code on failure
 *   - -1: Invalid configuration (NULL pointers)
 *   - -2: DMA initialization failed
 *
 * @note Configuration structure and buffers must remain valid until DeInit
 * @note If using DMA RX, starts circular DMA reception automatically
 */
int FEB_UART_Init(const FEB_UART_Config_t *config);

/**
 * @brief Deinitialize the UART library
 *
 * Stops DMA transfers and releases resources.
 * Printf redirection is disabled after this call.
 */
void FEB_UART_DeInit(void);

/**
 * @brief Check if library is initialized
 *
 * @return true if initialized, false otherwise
 */
bool FEB_UART_IsInitialized(void);

/* ============================================================================
 * Runtime Configuration API
 * ============================================================================ */

/**
 * @brief Set runtime log verbosity level
 *
 * Messages above this level are filtered at runtime.
 * Note: Compile-time level (FEB_UART_COMPILE_LOG_LEVEL) takes precedence.
 *
 * @param level New log level (FEB_UART_LOG_NONE to FEB_UART_LOG_TRACE)
 */
void FEB_UART_SetLogLevel(FEB_UART_LogLevel_t level);

/**
 * @brief Get current runtime log level
 *
 * @return Current log level
 */
FEB_UART_LogLevel_t FEB_UART_GetLogLevel(void);

/**
 * @brief Enable or disable ANSI color codes at runtime
 *
 * @param enable true to enable colors, false to disable
 */
void FEB_UART_SetColorsEnabled(bool enable);

/**
 * @brief Check if colors are enabled
 *
 * @return true if colors enabled, false otherwise
 */
bool FEB_UART_GetColorsEnabled(void);

/**
 * @brief Enable or disable timestamps at runtime
 *
 * @param enable true to enable timestamps, false to disable
 */
void FEB_UART_SetTimestampsEnabled(bool enable);

/**
 * @brief Check if timestamps are enabled
 *
 * @return true if timestamps enabled, false otherwise
 */
bool FEB_UART_GetTimestampsEnabled(void);

/* ============================================================================
 * Output API
 * ============================================================================ */

/**
 * @brief Printf-style formatted output to UART
 *
 * Formats the message and queues it for DMA transmission.
 * Non-blocking unless TX buffer is full.
 *
 * @param format Printf format string
 * @param ... Variable arguments
 * @return Number of bytes queued, or negative on error
 *   - >= 0: Bytes successfully queued
 *   - -1: Not initialized
 *   - -2: Buffer overflow (partial write)
 *
 * @note Thread-safe when FreeRTOS is enabled
 * @note Safe to call from ISR (uses separate path)
 */
int FEB_UART_Printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Write raw bytes to UART
 *
 * Queues raw data for DMA transmission without formatting.
 *
 * @param data Pointer to data to send
 * @param len  Number of bytes to send
 * @return Number of bytes queued, or negative on error
 *
 * @note Thread-safe when FreeRTOS is enabled
 */
int FEB_UART_Write(const uint8_t *data, size_t len);

/**
 * @brief Flush pending TX data
 *
 * Blocks until all queued data is transmitted or timeout expires.
 *
 * @param timeout_ms Maximum time to wait in milliseconds (0 = infinite)
 * @return 0 on success (buffer empty), -1 on timeout
 *
 * @warning Do not call from ISR context
 */
int FEB_UART_Flush(uint32_t timeout_ms);

/**
 * @brief Get number of bytes pending in TX buffer
 *
 * @return Number of bytes waiting to be transmitted
 */
size_t FEB_UART_TxPending(void);

/* ============================================================================
 * Input API
 * ============================================================================ */

/**
 * @brief Register callback for received lines
 *
 * The callback is invoked from FEB_UART_ProcessRx() when a complete
 * line (terminated by \\n) is received. Carriage return (\\r) is stripped.
 *
 * @param callback Function to call, or NULL to disable
 *
 * @note Callback is called from main loop context, not ISR
 */
void FEB_UART_SetRxLineCallback(FEB_UART_RxLineCallback_t callback);

/**
 * @brief Process received data and invoke callbacks
 *
 * Must be called periodically from the main loop or an RTOS task.
 * Parses received data, builds lines, and invokes the line callback.
 *
 * @note Not reentrant - call from single context only
 */
void FEB_UART_ProcessRx(void);

/**
 * @brief Check if RX data is available
 *
 * @return Number of unprocessed bytes in RX buffer
 */
size_t FEB_UART_RxAvailable(void);

/**
 * @brief Read raw bytes from RX buffer
 *
 * Reads and removes bytes from the RX buffer without line parsing.
 * Use this for binary protocols instead of line-based commands.
 *
 * @param data Destination buffer
 * @param max_len Maximum bytes to read
 * @return Number of bytes actually read
 */
size_t FEB_UART_Read(uint8_t *data, size_t max_len);

/* ============================================================================
 * HAL Callback Integration
 * ============================================================================
 *
 * The user must call these functions from the appropriate HAL callbacks.
 * This is required because HAL callbacks are defined as weak symbols and
 * must be implemented in user code to route to this library.
 */

/**
 * @brief Call from HAL_UART_TxCpltCallback()
 *
 * Handles DMA TX complete interrupt. Advances the TX ring buffer
 * and starts the next DMA transfer if data is pending.
 *
 * @param huart UART handle that completed transmission
 *
 * Example usage in stm32f4xx_hal_msp.c or callbacks file:
 * @code
 * void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
 *   FEB_UART_TxCpltCallback(huart);
 * }
 * @endcode
 */
void FEB_UART_TxCpltCallback(UART_HandleTypeDef *huart);

/**
 * @brief Call from HAL_UARTEx_RxEventCallback()
 *
 * Handles DMA RX events (half-complete, complete, idle).
 *
 * @param huart UART handle
 * @param size  Number of bytes received
 *
 * Example usage:
 * @code
 * void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
 *   FEB_UART_RxEventCallback(huart, Size);
 * }
 * @endcode
 */
void FEB_UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size);

/**
 * @brief Call from USARTx_IRQHandler for IDLE line detection
 *
 * Checks and clears the IDLE flag, then updates the RX head position.
 * Call BEFORE HAL_UART_IRQHandler() in the interrupt handler.
 *
 * @param huart UART handle
 *
 * Example usage in stm32f4xx_it.c:
 * @code
 * void USART2_IRQHandler(void) {
 *   FEB_UART_IDLE_Callback(&huart2);  // Call first
 *   HAL_UART_IRQHandler(&huart2);
 * }
 * @endcode
 */
void FEB_UART_IDLE_Callback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* FEB_UART_H */
