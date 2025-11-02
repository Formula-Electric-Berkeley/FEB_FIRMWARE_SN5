/**
 * @file FEB_CAN_RX.c
 * @brief CAN Reception System - Callback Registration Architecture
 * 
 * This module provides a clean callback-based system for handling incoming CAN messages.
 * Instead of large switch statements, you register a callback function for each CAN ID
 * you want to listen for. When that ID arrives, your callback is called automatically.
 * 
 * Key Features:
 * - Register up to 32 different CAN IDs
 * - Automatic filter configuration
 * - Callback-based processing (no manual switch statements)
 * - Thread-safe registration (can add/remove at runtime)
 */

// ============================================================================
// INCLUDES
// ============================================================================

#include "FEB_CAN_RX.h"
#include <string.h>

// ============================================================================
// PRIVATE DEFINES
// ============================================================================
#define FEB_CAN_RX_MAX_HANDLES 32          // Max number of CAN IDs we can listen for
#define FEB_CAN_MAX_STD_ID 0x7FF           // Maximum 11-bit standard CAN ID
#define FEB_CAN_MAX_EXT_ID 0x1FFFFFFF      // Maximum 29-bit extended CAN ID
#define FEB_CAN_NUM_INSTANCES 1            // DASH only has CAN1

// ============================================================================
// PRIVATE TYPEDEFS
// ============================================================================
/**
 * @brief Internal structure storing registered callback information
 */
typedef struct {
    FEB_CAN_RX_Callback_t callback;  // Function to call when message arrives
    uint32_t can_id;                  // Which CAN ID to listen for
    FEB_CAN_ID_Type_t id_type;        // Standard or Extended ID
    FEB_CAN_Instance_t instance;      // Which CAN bus (always CAN1 for DASH)
    bool is_active;                   // Is this slot in use?
} FEB_CAN_RX_Handle_t;

// ============================================================================
// PRIVATE VARIABLES
// ============================================================================
static FEB_CAN_RX_Handle_t feb_can_rx_handles[FEB_CAN_RX_MAX_HANDLES];
static CAN_RxHeaderTypeDef feb_can_rx_header[FEB_CAN_NUM_INSTANCES];
static uint8_t feb_can_rx_data[FEB_CAN_NUM_INSTANCES][8];
static uint32_t feb_can_rx_registered_count = 0;
static bool feb_can_rx_initialized = false;

// ============================================================================
// EXTERNAL VARIABLES
// ============================================================================

extern CAN_HandleTypeDef hcan1;

// ============================================================================
// PRIVATE FUNCTION PROTOTYPES
// ============================================================================
static int32_t FEB_CAN_RX_FindHandle(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type);
static int32_t FEB_CAN_RX_FindFreeHandle(void);
static bool FEB_CAN_RX_ValidateCanId(uint32_t can_id, FEB_CAN_ID_Type_t id_type);
static FEB_CAN_Instance_t FEB_CAN_RX_GetInstanceFromHandle(CAN_HandleTypeDef *hcan);

// ============================================================================
// PUBLIC FUNCTIONS - INITIALIZATION & REGISTRATION
// ============================================================================

/**
 * @brief Initialize CAN RX system
 * 
 * Clears all registered callbacks and prepares system for registration.
 * Call once at startup before registering any callbacks.
 */
FEB_CAN_Status_t FEB_CAN_RX_Init(void) {
    memset(feb_can_rx_handles, 0, sizeof(feb_can_rx_handles));
    feb_can_rx_registered_count = 0;
    feb_can_rx_initialized = true;
    
    return FEB_CAN_OK;
}

/**
 * @brief Register a callback for specific CAN ID
 * 
 * When a message with this ID arrives, your callback function will be called
 * automatically from the interrupt handler.
 * 
 * Example:
 *   FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, 0x10, FEB_CAN_ID_STD, bms_state_callback);
 */
FEB_CAN_Status_t FEB_CAN_RX_Register(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, FEB_CAN_RX_Callback_t callback) {
    if (!feb_can_rx_initialized || callback == NULL) {
        return FEB_CAN_ERROR_INVALID_PARAM;
    }
    
    if (!FEB_CAN_RX_ValidateCanId(can_id, id_type) || instance >= FEB_CAN_NUM_INSTANCES) {
        return FEB_CAN_ERROR_INVALID_PARAM;
    }
    
    // Check if already registered
    if (FEB_CAN_RX_FindHandle(instance, can_id, id_type) >= 0) {
        return FEB_CAN_ERROR_ALREADY_EXISTS;
    }
    
    // Find free slot
    int32_t handle_index = FEB_CAN_RX_FindFreeHandle();
    if (handle_index < 0) {
        return FEB_CAN_ERROR_FULL;
    }
    
    // Store registration info
    feb_can_rx_handles[handle_index].can_id = can_id;
    feb_can_rx_handles[handle_index].id_type = id_type;
    feb_can_rx_handles[handle_index].instance = instance;
    feb_can_rx_handles[handle_index].callback = callback;
    feb_can_rx_handles[handle_index].is_active = true;
    feb_can_rx_registered_count++;
    
    // Update hardware filters to accept this ID
    extern FEB_CAN_Status_t FEB_CAN_TX_UpdateFiltersForRegisteredIDs(FEB_CAN_Instance_t instance);
    FEB_CAN_TX_UpdateFiltersForRegisteredIDs(instance);
    
    return FEB_CAN_OK;
}

/**
 * @brief Unregister callback for CAN ID
 */
FEB_CAN_Status_t FEB_CAN_RX_Unregister(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type) {
    if (!feb_can_rx_initialized) {
        return FEB_CAN_ERROR;
    }
    
    int32_t handle_index = FEB_CAN_RX_FindHandle(instance, can_id, id_type);
    if (handle_index < 0) {
        return FEB_CAN_ERROR_NOT_FOUND;
    }
    
    // Clear registration
    feb_can_rx_handles[handle_index].is_active = false;
    feb_can_rx_handles[handle_index].callback = NULL;
    feb_can_rx_registered_count--;
    
    // Update filters
    extern FEB_CAN_Status_t FEB_CAN_TX_UpdateFiltersForRegisteredIDs(FEB_CAN_Instance_t instance);
    FEB_CAN_TX_UpdateFiltersForRegisteredIDs(instance);
    
    return FEB_CAN_OK;
}

/**
 * @brief Check if CAN ID is registered
 */
bool FEB_CAN_RX_IsRegistered(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type) {
    if (!feb_can_rx_initialized || instance >= FEB_CAN_NUM_INSTANCES) {
        return false;
    }
    return (FEB_CAN_RX_FindHandle(instance, can_id, id_type) >= 0);
}

/**
 * @brief Get number of registered callbacks
 */
uint32_t FEB_CAN_RX_GetRegisteredCount(void) {
    return feb_can_rx_registered_count;
}

/**
 * @brief Get list of all registered CAN IDs
 */
uint32_t FEB_CAN_RX_GetRegisteredIDs(FEB_CAN_Instance_t instance, uint32_t *id_list, FEB_CAN_ID_Type_t *id_type_list, uint32_t max_count) {
    if (!feb_can_rx_initialized || instance >= FEB_CAN_NUM_INSTANCES || id_list == NULL || id_type_list == NULL || max_count == 0) {
        return 0;
    }
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < FEB_CAN_RX_MAX_HANDLES && count < max_count; i++) {
        if (feb_can_rx_handles[i].is_active && feb_can_rx_handles[i].instance == instance) {
            id_list[count] = feb_can_rx_handles[i].can_id;
            id_type_list[count] = feb_can_rx_handles[i].id_type;
            count++;
        }
    }
    
    return count;
}

/* HAL Callbacks -------------------------------------------------------------*/

/**
 * @brief CAN RX interrupt callback - called by hardware when message arrives
 * 
 * This function is called automatically by STM32 HAL when a CAN message is received.
 * It reads the message from hardware, finds the registered callback for that ID,
 * and calls it with the received data.
 * 
 * Runs in INTERRUPT CONTEXT - keep fast!
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (!feb_can_rx_initialized) {
        return;
    }
    
    FEB_CAN_Instance_t instance = FEB_CAN_RX_GetInstanceFromHandle(hcan);
    if (instance >= FEB_CAN_NUM_INSTANCES) {
        return;
    }
    
    // Read message from hardware FIFO
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &feb_can_rx_header[instance], feb_can_rx_data[instance]) == HAL_OK) {
        uint32_t can_id;
        FEB_CAN_ID_Type_t id_type;
        
        // Extract CAN ID based on type
        if (feb_can_rx_header[instance].IDE == CAN_ID_STD) {
            can_id = feb_can_rx_header[instance].StdId;
            id_type = FEB_CAN_ID_STD;
        } else {
            can_id = feb_can_rx_header[instance].ExtId;
            id_type = FEB_CAN_ID_EXT;
        }
        
        // Find registered callback for this ID
        int32_t handle_index = FEB_CAN_RX_FindHandle(instance, can_id, id_type);
        if (handle_index >= 0) {
            // Call user's callback function
            feb_can_rx_handles[handle_index].callback(
                instance,
                can_id,
                id_type,
                feb_can_rx_data[instance], 
                feb_can_rx_header[instance].DLC
            );
        }
    }
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Find handle for specific CAN ID
 * @return Index if found, -1 if not found
 */
static int32_t FEB_CAN_RX_FindHandle(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type) {
    for (uint32_t i = 0; i < FEB_CAN_RX_MAX_HANDLES; i++) {
        if (feb_can_rx_handles[i].is_active && 
            feb_can_rx_handles[i].can_id == can_id &&
            feb_can_rx_handles[i].id_type == id_type &&
            feb_can_rx_handles[i].instance == instance) {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief Find free handle slot
 * @return Index if found, -1 if all slots full
 */
static int32_t FEB_CAN_RX_FindFreeHandle(void) {
    for (uint32_t i = 0; i < FEB_CAN_RX_MAX_HANDLES; i++) {
        if (!feb_can_rx_handles[i].is_active) {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief Validate CAN ID is in valid range
 */
static bool FEB_CAN_RX_ValidateCanId(uint32_t can_id, FEB_CAN_ID_Type_t id_type) {
    if (id_type == FEB_CAN_ID_STD) {
        return (can_id <= FEB_CAN_MAX_STD_ID);
    } else if (id_type == FEB_CAN_ID_EXT) {
        return (can_id <= FEB_CAN_MAX_EXT_ID);
    }
    return false;
}

/**
 * @brief Get CAN instance from HAL handle
 */
static FEB_CAN_Instance_t FEB_CAN_RX_GetInstanceFromHandle(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance == CAN1) {
        return FEB_CAN_INSTANCE_1;
    }
    return FEB_CAN_NUM_INSTANCES; // Invalid
}

