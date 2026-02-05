/**
 ******************************************************************************
 * @file           : feb_can.h
 * @brief          : Public API for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides a comprehensive CAN interface with:
 *   - FreeRTOS-safe queued TX/RX operations
 *   - TX registration with optional periodic auto-transmit
 *   - RX registration with flexible filtering (exact, mask, wildcard)
 *   - Multi-instance support (CAN1/CAN2)
 *   - Integration with generated feb_can.h pack/unpack functions
 *
 * Usage:
 *   1. Configure CAN peripheral in STM32CubeMX
 *   2. Add library to CMakeLists.txt: target_link_libraries(... feb_can)
 *   3. Call FEB_CAN_Init() with configuration
 *   4. Register TX slots and RX callbacks
 *   5. Route HAL callbacks to library functions
 *   6. Call FEB_CAN_ProcessTx/Rx() in appropriate tasks
 *
 * Example:
 *   FEB_CAN_Config_t cfg = {
 *     .hcan1 = &hcan1,
 *     .hcan2 = NULL,  // NULL if not using CAN2
 *     .get_tick_ms = HAL_GetTick,
 *   };
 *   FEB_CAN_Init(&cfg);
 *
 ******************************************************************************
 */

#ifndef FEB_CAN_LIBRARY_H
#define FEB_CAN_LIBRARY_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

  /* ============================================================================
   * Forward Declarations
   * ============================================================================ */

  /* Use void pointer for CAN handle to avoid HAL header dependency in public API.
   * The actual CAN_HandleTypeDef is defined in stm32f4xx_hal_can.h with an anonymous
   * struct, which cannot be forward declared portably. User code should include
   * the HAL headers and cast appropriately. */
  typedef void *FEB_CAN_Handle_t;

  /* ============================================================================
   * Status Enumeration
   * ============================================================================ */

  /**
   * @brief Status codes for FEB CAN operations
   */
  typedef enum
  {
    FEB_CAN_OK = 0,               /**< Operation successful */
    FEB_CAN_ERROR,                /**< General error */
    FEB_CAN_ERROR_INVALID_PARAM,  /**< Invalid parameter */
    FEB_CAN_ERROR_FULL,           /**< Buffer/registry/queue full */
    FEB_CAN_ERROR_NOT_FOUND,      /**< ID not found */
    FEB_CAN_ERROR_ALREADY_EXISTS, /**< ID already registered */
    FEB_CAN_ERROR_TIMEOUT,        /**< Operation timeout */
    FEB_CAN_ERROR_HAL,            /**< HAL layer error */
    FEB_CAN_ERROR_NOT_INIT,       /**< Library not initialized */
    FEB_CAN_ERROR_QUEUE,          /**< Queue operation failed */
  } FEB_CAN_Status_t;

  /* ============================================================================
   * CAN Instance and ID Type Enumerations
   * ============================================================================ */

  /**
   * @brief CAN instance enumeration
   */
  typedef enum FEB_CAN_Instance
  {
    FEB_CAN_INSTANCE_1 = 0, /**< CAN1 instance */
    FEB_CAN_INSTANCE_2 = 1, /**< CAN2 instance */
    FEB_CAN_INSTANCE_COUNT  /**< Number of instances */
  } FEB_CAN_Instance_t;

  /**
   * @brief CAN ID type enumeration
   */
  typedef enum FEB_CAN_ID_Type
  {
    FEB_CAN_ID_STD = 0, /**< Standard 11-bit CAN ID */
    FEB_CAN_ID_EXT = 1  /**< Extended 29-bit CAN ID */
  } FEB_CAN_ID_Type_t;

  /**
   * @brief RX filter type enumeration
   */
  typedef enum FEB_CAN_Filter_Type
  {
    FEB_CAN_FILTER_EXACT = 0,    /**< Exact ID match */
    FEB_CAN_FILTER_MASK = 1,     /**< ID with mask (range matching) */
    FEB_CAN_FILTER_WILDCARD = 2, /**< Accept all messages */
  } FEB_CAN_Filter_Type_t;

  /**
   * @brief FIFO assignment enumeration
   */
  typedef enum FEB_CAN_FIFO
  {
    FEB_CAN_FIFO_0 = 0, /**< Use FIFO0 */
    FEB_CAN_FIFO_1 = 1, /**< Use FIFO1 */
  } FEB_CAN_FIFO_t;

  /* ============================================================================
   * Configuration Structure
   * ============================================================================ */

  /**
   * @brief CAN library initialization configuration
   */
  typedef struct
  {
    /* Required: CAN peripheral handles (pass address of CAN_HandleTypeDef) */
    FEB_CAN_Handle_t hcan1; /**< HAL CAN1 handle (must be initialized) */
    FEB_CAN_Handle_t hcan2; /**< HAL CAN2 handle (NULL if not used) */

    /* Optional: Queue sizes (FreeRTOS mode, 0 = use defaults) */
    uint16_t tx_queue_size; /**< TX queue depth (default: 16) */
    uint16_t rx_queue_size; /**< RX queue depth (default: 32) */

    /* Optional: Timestamp source (defaults to HAL_GetTick if NULL) */
    uint32_t (*get_tick_ms)(void); /**< Function returning millisecond tick */

  } FEB_CAN_Config_t;

  /* ============================================================================
   * Callback Types
   * ============================================================================ */

  /**
   * @brief Standard RX callback function type
   *
   * @param instance CAN instance that received the message
   * @param can_id CAN identifier of received message
   * @param id_type Type of CAN ID (standard or extended)
   * @param data Pointer to received data (up to 8 bytes)
   * @param length Length of received data
   * @param user_data User context passed during registration
   */
  typedef void (*FEB_CAN_RX_Callback_t)(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                        const uint8_t *data, uint8_t length, void *user_data);

  /**
   * @brief Extended RX callback with metadata
   *
   * @param instance CAN instance that received the message
   * @param can_id CAN identifier of received message
   * @param id_type Type of CAN ID
   * @param data Pointer to received data
   * @param length Length of received data
   * @param timestamp Reception timestamp (ms)
   * @param error_flags Any error flags (bus errors, etc.)
   * @param user_data User context passed during registration
   */
  typedef void (*FEB_CAN_RX_Extended_Callback_t)(FEB_CAN_Instance_t instance, uint32_t can_id,
                                                 FEB_CAN_ID_Type_t id_type, const uint8_t *data, uint8_t length,
                                                 uint32_t timestamp, uint32_t error_flags, void *user_data);

  /* ============================================================================
   * Initialization API
   * ============================================================================ */

  /**
   * @brief Initialize the CAN library
   *
   * Sets up TX/RX queues (in FreeRTOS mode), initializes filter banks,
   * and starts the CAN peripherals.
   *
   * @param config Pointer to configuration structure
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_Init(const FEB_CAN_Config_t *config);

  /**
   * @brief Deinitialize the CAN library
   *
   * Stops CAN peripherals and releases resources.
   */
  void FEB_CAN_DeInit(void);

  /**
   * @brief Check if library is initialized
   *
   * @return true if initialized, false otherwise
   */
  bool FEB_CAN_IsInitialized(void);

  /* ============================================================================
   * TX Registration API
   * ============================================================================ */

  /**
   * @brief TX slot registration parameters
   */
  typedef struct
  {
    FEB_CAN_Instance_t instance;                       /**< CAN instance to transmit on */
    uint32_t can_id;                                   /**< CAN ID to use */
    FEB_CAN_ID_Type_t id_type;                         /**< Standard or Extended ID */
    void *data_ptr;                                    /**< Pointer to data structure (for periodic TX) */
    size_t data_size;                                  /**< Size of data structure */
    uint32_t period_ms;                                /**< Periodic interval (0 = manual TX only) */
    int (*pack_func)(uint8_t *, const void *, size_t); /**< Pack function (from feb_can.h) */
  } FEB_CAN_TX_Params_t;

  /**
   * @brief Register a TX slot for a specific CAN ID
   *
   * Creates a transmission slot that can be used for:
   * - Manual one-shot transmissions
   * - Automatic periodic transmissions (if period_ms > 0)
   *
   * @param params TX slot parameters
   * @return Handle ID (>= 0) on success, negative error code on failure
   */
  int32_t FEB_CAN_TX_Register(const FEB_CAN_TX_Params_t *params);

  /**
   * @brief Unregister a TX slot
   *
   * @param handle Handle returned from FEB_CAN_TX_Register
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_Unregister(int32_t handle);

  /**
   * @brief Update TX slot period
   *
   * @param handle Handle returned from FEB_CAN_TX_Register
   * @param period_ms New period (0 = disable periodic TX)
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_SetPeriod(int32_t handle, uint32_t period_ms);

  /* ============================================================================
   * TX Transmit API
   * ============================================================================ */

  /**
   * @brief Transmit a CAN message immediately
   *
   * In FreeRTOS mode: Queues the message for transmission by the TX task.
   * In bare-metal mode: Transmits directly with timeout.
   *
   * @param instance CAN instance
   * @param can_id CAN identifier
   * @param id_type Standard or Extended ID
   * @param data Pointer to data (up to 8 bytes)
   * @param length Data length (0-8)
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_Send(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                   const uint8_t *data, uint8_t length);

  /**
   * @brief Transmit using a registered TX slot
   *
   * Uses the slot's configured ID and packs data from the registered data_ptr.
   *
   * @param handle Handle returned from FEB_CAN_TX_Register
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_SendSlot(int32_t handle);

  /**
   * @brief Transmit with custom data using a registered TX slot
   *
   * Uses the slot's configured ID but with provided data.
   *
   * @param handle Handle returned from FEB_CAN_TX_Register
   * @param data Pointer to data to transmit
   * @param length Data length
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_SendSlotData(int32_t handle, const uint8_t *data, uint8_t length);

  /**
   * @brief ISR-safe transmit (for use in callbacks/interrupts)
   *
   * @param instance CAN instance
   * @param can_id CAN identifier
   * @param id_type Standard or Extended ID
   * @param data Pointer to data
   * @param length Data length
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_SendFromISR(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                          const uint8_t *data, uint8_t length);

  /* ============================================================================
   * RX Registration API
   * ============================================================================ */

  /**
   * @brief RX callback registration parameters
   */
  typedef struct
  {
    FEB_CAN_Instance_t instance;       /**< CAN instance to receive on */
    uint32_t can_id;                   /**< CAN ID to match */
    FEB_CAN_ID_Type_t id_type;         /**< Standard or Extended ID */
    FEB_CAN_Filter_Type_t filter_type; /**< Exact, mask, or wildcard */
    uint32_t mask;                     /**< ID mask (for FILTER_MASK type) */
    FEB_CAN_FIFO_t fifo;               /**< FIFO assignment (0 or 1) */
    FEB_CAN_RX_Callback_t callback;    /**< Callback function */
    void *user_data;                   /**< User context passed to callback */
  } FEB_CAN_RX_Params_t;

  /**
   * @brief Register an RX callback for a specific CAN ID or ID range
   *
   * @param params RX registration parameters
   * @return Handle ID (>= 0) on success, negative error code on failure
   */
  int32_t FEB_CAN_RX_Register(const FEB_CAN_RX_Params_t *params);

  /**
   * @brief Register an RX callback with extended metadata
   *
   * Same as FEB_CAN_RX_Register but uses extended callback with timestamp/errors.
   *
   * @param params RX registration parameters (callback field ignored)
   * @param ext_callback Extended callback function
   * @return Handle ID (>= 0) on success, negative error code on failure
   */
  int32_t FEB_CAN_RX_RegisterExtended(const FEB_CAN_RX_Params_t *params, FEB_CAN_RX_Extended_Callback_t ext_callback);

  /**
   * @brief Unregister an RX callback
   *
   * @param handle Handle returned from FEB_CAN_RX_Register
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_RX_Unregister(int32_t handle);

  /**
   * @brief Check if a CAN ID is registered for RX
   *
   * @param instance CAN instance
   * @param can_id CAN identifier
   * @param id_type Standard or Extended ID
   * @return true if registered, false otherwise
   */
  bool FEB_CAN_RX_IsRegistered(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type);

  /* ============================================================================
   * Filter Configuration API
   * ============================================================================ */

  /**
   * @brief Manually configure a filter bank
   *
   * @param instance CAN instance
   * @param filter_bank Filter bank number (0-13 for CAN1, 14-27 for CAN2)
   * @param id Filter ID
   * @param mask Filter mask
   * @param id_type Standard or Extended
   * @param fifo FIFO assignment
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_Filter_Configure(FEB_CAN_Instance_t instance, uint8_t filter_bank, uint32_t id,
                                            uint32_t mask, FEB_CAN_ID_Type_t id_type, FEB_CAN_FIFO_t fifo);

  /**
   * @brief Configure accept-all filter
   *
   * Configures a filter bank to accept all messages on the specified FIFO.
   *
   * @param instance CAN instance
   * @param filter_bank Filter bank number
   * @param fifo FIFO assignment
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_Filter_AcceptAll(FEB_CAN_Instance_t instance, uint8_t filter_bank, FEB_CAN_FIFO_t fifo);

  /**
   * @brief Update filters based on registered RX callbacks
   *
   * Automatically reconfigures filters to match currently registered RX IDs.
   *
   * @param instance CAN instance
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_Filter_UpdateFromRegistry(FEB_CAN_Instance_t instance);

  /* ============================================================================
   * Processing API
   * ============================================================================ */

  /**
   * @brief Process TX queue (FreeRTOS mode)
   *
   * Call this from a dedicated TX task. Dequeues messages and transmits them.
   * In bare-metal mode, this is a no-op (TX is immediate).
   *
   * @note Should be called in a loop with appropriate delay
   */
  void FEB_CAN_TX_Process(void);

  /**
   * @brief Process RX queue (FreeRTOS mode)
   *
   * Call this from a dedicated RX task. Dequeues received messages and
   * invokes registered callbacks.
   * In bare-metal mode, this is a no-op (callbacks invoked directly in ISR).
   *
   * @note Should be called in a loop with appropriate delay
   */
  void FEB_CAN_RX_Process(void);

  /**
   * @brief Process periodic TX slots
   *
   * Checks all registered periodic TX slots and transmits if due.
   * Should be called from main loop or a timer callback.
   */
  void FEB_CAN_TX_ProcessPeriodic(void);

  /* ============================================================================
   * Status and Diagnostics API
   * ============================================================================ */

  /**
   * @brief Get number of registered TX slots
   *
   * @return Number of active TX registrations
   */
  uint32_t FEB_CAN_TX_GetRegisteredCount(void);

  /**
   * @brief Get number of registered RX callbacks
   *
   * @return Number of active RX registrations
   */
  uint32_t FEB_CAN_RX_GetRegisteredCount(void);

  /**
   * @brief Get TX queue pending count (FreeRTOS mode)
   *
   * @return Number of messages waiting in TX queue
   */
  uint32_t FEB_CAN_TX_GetQueuePending(void);

  /**
   * @brief Get RX queue pending count (FreeRTOS mode)
   *
   * @return Number of messages waiting in RX queue
   */
  uint32_t FEB_CAN_RX_GetQueuePending(void);

  /**
   * @brief Check if CAN instance is ready for transmission
   *
   * @param instance CAN instance
   * @return true if at least one mailbox is free
   */
  bool FEB_CAN_TX_IsReady(FEB_CAN_Instance_t instance);

  /**
   * @brief Get number of free TX mailboxes
   *
   * @param instance CAN instance
   * @return Number of free mailboxes (0-3)
   */
  uint32_t FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_Instance_t instance);

  /**
   * @brief Get RX queue overflow count
   *
   * Returns the number of RX messages dropped due to full queue (FreeRTOS mode only).
   *
   * @return Number of dropped RX messages
   */
  uint32_t FEB_CAN_GetRxQueueOverflowCount(void);

  /**
   * @brief Get TX queue overflow count
   *
   * Returns the number of TX messages dropped due to full queue (FreeRTOS mode only).
   *
   * @return Number of dropped TX messages
   */
  uint32_t FEB_CAN_GetTxQueueOverflowCount(void);

  /**
   * @brief Get TX timeout count
   *
   * Returns the number of TX messages dropped due to mailbox timeout.
   *
   * @return Number of timed-out TX messages
   */
  uint32_t FEB_CAN_GetTxTimeoutCount(void);

  /**
   * @brief Get HAL error count
   *
   * Returns the number of HAL layer errors encountered during TX operations.
   *
   * @return Number of HAL errors
   */
  uint32_t FEB_CAN_GetHalErrorCount(void);

  /**
   * @brief Reset all error counters to zero
   */
  void FEB_CAN_ResetErrorCounters(void);

  /* ============================================================================
   * HAL Callback Integration
   * ============================================================================
   *
   * These functions must be called from the appropriate HAL callbacks.
   */

  /**
   * @brief Call from HAL_CAN_RxFifo0MsgPendingCallback
   *
   * @param hcan CAN handle that received the message (CAN_HandleTypeDef*)
   */
  void FEB_CAN_RxFifo0Callback(FEB_CAN_Handle_t hcan);

  /**
   * @brief Call from HAL_CAN_RxFifo1MsgPendingCallback
   *
   * @param hcan CAN handle that received the message (CAN_HandleTypeDef*)
   */
  void FEB_CAN_RxFifo1Callback(FEB_CAN_Handle_t hcan);

  /**
   * @brief Call from HAL_CAN_TxMailbox0CompleteCallback
   *
   * @param hcan CAN handle (CAN_HandleTypeDef*)
   */
  void FEB_CAN_TxMailbox0CompleteCallback(FEB_CAN_Handle_t hcan);

  /**
   * @brief Call from HAL_CAN_TxMailbox1CompleteCallback
   *
   * @param hcan CAN handle (CAN_HandleTypeDef*)
   */
  void FEB_CAN_TxMailbox1CompleteCallback(FEB_CAN_Handle_t hcan);

  /**
   * @brief Call from HAL_CAN_TxMailbox2CompleteCallback
   *
   * @param hcan CAN handle (CAN_HandleTypeDef*)
   */
  void FEB_CAN_TxMailbox2CompleteCallback(FEB_CAN_Handle_t hcan);

  /**
   * @brief Call from HAL_CAN_ErrorCallback
   *
   * @param hcan CAN handle (CAN_HandleTypeDef*)
   */
  void FEB_CAN_ErrorCallback(FEB_CAN_Handle_t hcan);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_LIBRARY_H */
