#ifndef __FEB_CAN_TX_H
#define __FEB_CAN_TX_H

#include "stm32f4xx_hal.h"
#include "FEB_CAN_RX.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration structure for CAN filters
 */
typedef struct {
    uint32_t filter_id;        /**< Filter ID */
    uint32_t filter_mask;      /**< Filter mask */
    uint32_t filter_mode;      /**< Filter mode (list or mask) */
    uint32_t filter_scale;     /**< Filter scale (32-bit or 16-bit) */
    uint32_t filter_fifo;      /**< FIFO assignment */
    bool filter_enable;        /**< Enable this filter? */
} FEB_CAN_Filter_Config_t;

/**
 * @brief Default transmission timeout
 */
#define FEB_CAN_TX_TIMEOUT_MS 100

/**
 * @brief Initialize complete CAN system (RX + TX + Filters + Start)
 * 
 * This initializes everything needed for CAN communication:
 * - Initializes RX callback system
 * - Configures hardware filters
 * - Starts CAN peripheral
 * - Enables RX interrupts
 * 
 * Call once at startup after HAL_CAN_Init() but before registering callbacks.
 * 
 * @return FEB_CAN_Status_t Operation status
 */
FEB_CAN_Status_t FEB_CAN_TX_Init(void);

/**
 * @brief Manually configure a CAN filter
 * 
 * Usually not needed - filters are auto-configured when you register callbacks.
 * 
 * @param instance CAN instance (only CAN1 for DASH)
 * @param filter_config Pointer to filter configuration
 * @return FEB_CAN_Status_t Operation status
 */
FEB_CAN_Status_t FEB_CAN_TX_ConfigureFilter(FEB_CAN_Instance_t instance, const FEB_CAN_Filter_Config_t *filter_config);

/**
 * @brief Update hardware filters to match registered IDs
 * 
 * Called automatically when you register/unregister callbacks.
 * Configures CAN hardware filters to only accept registered IDs.
 * 
 * @param instance CAN instance
 * @return FEB_CAN_Status_t Operation status
 */
FEB_CAN_Status_t FEB_CAN_TX_UpdateFiltersForRegisteredIDs(FEB_CAN_Instance_t instance);

/**
 * @brief Transmit a CAN message
 * 
 * Sends a CAN message on the bus. Blocks until TX mailbox available or timeout.
 * 
 * Example:
 *   uint8_t data[2] = {0x01, 0x02};
 *   FEB_CAN_TX_Transmit(FEB_CAN_INSTANCE_1, 0x20, FEB_CAN_ID_STD, data, 2, 100);
 * 
 * @param instance CAN instance (only CAN1 for DASH)
 * @param can_id CAN identifier (e.g., 0x20 for dashboard messages)
 * @param id_type Standard or Extended ID
 * @param data Pointer to data bytes (8 bytes max)
 * @param length Data length (0-8)
 * @param timeout_ms Timeout in milliseconds
 * @return FEB_CAN_Status_t Operation status
 */
FEB_CAN_Status_t FEB_CAN_TX_Transmit(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data, uint8_t length, uint32_t timeout_ms);

/**
 * @brief Transmit CAN message with default timeout
 * 
 * Convenience function that uses standard 11-bit ID and default timeout.
 * 
 * @param instance CAN instance
 * @param can_id CAN identifier  
 * @param data Pointer to data
 * @param length Data length (0-8)
 * @return FEB_CAN_Status_t Operation status
 */
FEB_CAN_Status_t FEB_CAN_TX_TransmitDefault(FEB_CAN_Instance_t instance, uint32_t can_id, const uint8_t *data, uint8_t length);

/**
 * @brief Get number of free TX mailboxes
 * 
 * @param instance CAN instance
 * @return Number of free mailboxes (0-3)
 */
uint32_t FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_Instance_t instance);

/**
 * @brief Check if CAN system is ready to transmit
 * 
 * @param instance CAN instance
 * @return true if ready, false otherwise
 */
bool FEB_CAN_TX_IsReady(FEB_CAN_Instance_t instance);

#ifdef __cplusplus
}
#endif

#endif

