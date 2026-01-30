#ifndef __FEB_CAN_TX_H
#define __FEB_CAN_TX_H

#include "stm32f4xx_hal.h"
#include "FEB_CAN_RX.h" // For shared status types
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Configuration structure for CAN filters
   */
  typedef struct
  {
    uint32_t filter_id;    /**< Filter ID */
    uint32_t filter_mask;  /**< Filter mask */
    uint32_t filter_mode;  /**< Filter mode */
    uint32_t filter_scale; /**< Filter scale */
    uint32_t filter_fifo;  /**< Filter FIFO assignment */
    bool filter_enable;    /**< Filter enable state */
  } FEB_CAN_Filter_Config_t;

/**
 * @brief CAN transmission timeout in milliseconds
 */
#define FEB_CAN_TX_TIMEOUT_MS 100

  /**
   * @brief Initialize the FEB CAN system (both RX and TX)
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_Init(void);

  /**
   * @brief Configure CAN filters manually
   * @param instance CAN instance (CAN1 or CAN2)
   * @param filter_config Pointer to filter configuration
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_ConfigureFilter(FEB_CAN_Instance_t instance,
                                              const FEB_CAN_Filter_Config_t *filter_config);

  /**
   * @brief Update filters to match currently registered RX IDs
   * @param instance CAN instance (CAN1 or CAN2)
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_UpdateFiltersForRegisteredIDs(FEB_CAN_Instance_t instance);

  /**
   * @brief Transmit a CAN message with timeout
   * @param instance CAN instance (CAN1 or CAN2)
   * @param can_id CAN identifier
   * @param id_type Type of CAN ID (standard 11-bit or extended 29-bit)
   * @param data Pointer to data to transmit
   * @param length Length of data (0-8 bytes)
   * @param timeout_ms Timeout in milliseconds (0 = no timeout)
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_Transmit(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                       const uint8_t *data, uint8_t length, uint32_t timeout_ms);

  /**
   * @brief Transmit a CAN message with default timeout
   * @param instance CAN instance (CAN1 or CAN2)
   * @param can_id CAN identifier
   * @param id_type Type of CAN ID (standard 11-bit or extended 29-bit)
   * @param data Pointer to data to transmit
   * @param length Length of data (0-8 bytes)
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_TX_TransmitDefault(FEB_CAN_Instance_t instance, uint32_t can_id, const uint8_t *data,
                                              uint8_t length);

  /**
   * @brief Check if CAN TX mailboxes are available
   * @param instance CAN instance (CAN1 or CAN2)
   * @return Number of free mailboxes (0-3)
   */
  uint32_t FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_Instance_t instance);

  /**
   * @brief Check if CAN system is ready for transmission
   * @param instance CAN instance (CAN1 or CAN2)
   * @return true if ready, false otherwise
   */
  bool FEB_CAN_TX_IsReady(FEB_CAN_Instance_t instance);

#ifdef __cplusplus
}
#endif

#endif