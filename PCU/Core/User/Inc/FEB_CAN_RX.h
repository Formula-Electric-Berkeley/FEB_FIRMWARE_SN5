#ifndef __FEB_CAN_RX_H
#define __FEB_CAN_RX_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Status codes for FEB CAN operations
   */
  typedef enum
  {
    FEB_CAN_OK = 0,               /**< Operation successful */
    FEB_CAN_ERROR,                /**< General error */
    FEB_CAN_ERROR_INVALID_PARAM,  /**< Invalid parameter */
    FEB_CAN_ERROR_FULL,           /**< Buffer/registry full */
    FEB_CAN_ERROR_NOT_FOUND,      /**< ID not found */
    FEB_CAN_ERROR_ALREADY_EXISTS, /**< ID already registered */
    FEB_CAN_ERROR_TIMEOUT,        /**< Operation timeout */
    FEB_CAN_ERROR_HAL             /**< HAL layer error */
  } FEB_CAN_Status_t;

  /**
   * @brief CAN ID type enumeration
   */
  typedef enum
  {
    FEB_CAN_ID_STD = 0, /**< Standard 11-bit CAN ID */
    FEB_CAN_ID_EXT = 1  /**< Extended 29-bit CAN ID */
  } FEB_CAN_ID_Type_t;

  /**
   * @brief CAN instance enumeration
   */
  typedef enum
  {
    FEB_CAN_INSTANCE_1 = 0, /**< CAN1 instance */
    FEB_CAN_INSTANCE_2 = 1  /**< CAN2 instance */
  } FEB_CAN_Instance_t;

  /**
   * @brief Callback function type for CAN RX messages
   * @param instance CAN instance that received the message
   * @param can_id CAN identifier of received message
   * @param id_type Type of CAN ID (standard or extended)
   * @param data Pointer to received data
   * @param length Length of received data (0-8 bytes)
   */
  typedef void (*FEB_CAN_RX_Callback_t)(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                        uint8_t *data, uint8_t length);

  /**
   * @brief Initialize the FEB CAN RX system
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_RX_Init(void);

  /**
   * @brief Register a callback for a specific CAN ID
   * @param instance CAN instance (CAN1 or CAN2)
   * @param can_id CAN identifier to register for
   * @param id_type Type of CAN ID (standard 11-bit or extended 29-bit)
   * @param callback Function to call when message is received
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_RX_Register(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                       FEB_CAN_RX_Callback_t callback);

  /**
   * @brief Unregister a callback for a specific CAN ID
   * @param instance CAN instance (CAN1 or CAN2)
   * @param can_id CAN identifier to unregister
   * @param id_type Type of CAN ID (standard 11-bit or extended 29-bit)
   * @return FEB_CAN_Status_t Operation status
   */
  FEB_CAN_Status_t FEB_CAN_RX_Unregister(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type);

  /**
   * @brief Check if a CAN ID is currently registered
   * @param instance CAN instance (CAN1 or CAN2)
   * @param can_id CAN identifier to check
   * @param id_type Type of CAN ID (standard 11-bit or extended 29-bit)
   * @return true if registered, false otherwise
   */
  bool FEB_CAN_RX_IsRegistered(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type);

  /**
   * @brief Get the number of currently registered callbacks
   * @return Number of registered callbacks
   */
  uint32_t FEB_CAN_RX_GetRegisteredCount(void);

  /**
   * @brief Get all registered CAN IDs for a specific instance
   * @param instance CAN instance to query
   * @param id_list Buffer to store registered IDs
   * @param id_type_list Buffer to store ID types
   * @param max_count Maximum number of IDs to return
   * @return Number of IDs returned
   */
  uint32_t FEB_CAN_RX_GetRegisteredIDs(FEB_CAN_Instance_t instance, uint32_t *id_list, FEB_CAN_ID_Type_t *id_type_list,
                                       uint32_t max_count);

#ifdef __cplusplus
}
#endif

#endif