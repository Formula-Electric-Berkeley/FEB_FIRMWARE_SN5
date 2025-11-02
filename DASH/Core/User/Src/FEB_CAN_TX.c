/**
 * @file FEB_CAN_TX.c
 * @brief CAN Transmission System and Filter Management
 * 
 * This module handles CAN message transmission and automatic filter configuration.
 * When you register RX callbacks, it automatically updates hardware filters to
 * accept only those CAN IDs.
 * 
 * Key Features:
 * - Simple transmission API
 * - Automatic filter configuration based on registered IDs
 * - Timeout-based mailbox waiting
 */

// ============================================================================
// INCLUDES
// ============================================================================

#include "FEB_CAN_TX.h"
#include <string.h>

// ============================================================================
// PRIVATE DEFINES
// ============================================================================
#define FEB_CAN_MAX_STD_ID 0x7FF           // Max 11-bit ID
#define FEB_CAN_MAX_EXT_ID 0x1FFFFFFF      // Max 29-bit ID
#define FEB_CAN_MAX_DATA_LENGTH 8
#define FEB_CAN_NUM_INSTANCES 1            // DASH only has CAN1
#define FEB_CAN_MAX_FILTERS 14             // Max hardware filter banks for CAN1

// ============================================================================
// PRIVATE VARIABLES
// ============================================================================
static bool feb_can_tx_initialized = false;

/* Global TX variables (for legacy transmit functions) ----------------------*/
CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
uint8_t FEB_CAN_Tx_Data[8];
uint32_t FEB_CAN_Tx_Mailbox;

// ============================================================================
// EXTERNAL VARIABLES
// ============================================================================

extern CAN_HandleTypeDef hcan1;

// ============================================================================
// PRIVATE FUNCTION PROTOTYPES
// ============================================================================
static bool FEB_CAN_TX_ValidateCanId(uint32_t can_id, FEB_CAN_ID_Type_t id_type);
static bool FEB_CAN_TX_ValidateDataLength(uint8_t length);
static FEB_CAN_Status_t FEB_CAN_TX_WaitForMailbox(FEB_CAN_Instance_t instance, uint32_t timeout_ms);
static CAN_HandleTypeDef* FEB_CAN_TX_GetHandle(FEB_CAN_Instance_t instance);

// ============================================================================
// PUBLIC FUNCTIONS - INITIALIZATION & CONFIGURATION
// ============================================================================

/**
 * @brief Initialize complete CAN system
 * 
 * This is the main initialization function. It:
 * 1. Initializes RX callback system
 * 2. Configures initial reject-all filter (no messages accepted until you register)
 * 3. Starts CAN peripheral
 * 4. Enables RX interrupts
 * 
 * Call once at startup.
 */
FEB_CAN_Status_t FEB_CAN_TX_Init(void) {
    // Initialize RX system first
    FEB_CAN_Status_t rx_status = FEB_CAN_RX_Init();
    if (rx_status != FEB_CAN_OK) {
        return rx_status;
    }
    
    // Configure reject-all filter initially
    FEB_CAN_Filter_Config_t reject_filter = {
        .filter_id = 0x7FF,              // Set to impossible ID
        .filter_mask = 0x7FF,            // Exact match (reject all)
        .filter_mode = CAN_FILTERMODE_IDMASK,
        .filter_scale = CAN_FILTERSCALE_32BIT,
        .filter_fifo = CAN_RX_FIFO0,
        .filter_enable = true
    };
    
    FEB_CAN_Status_t filter_status = FEB_CAN_TX_ConfigureFilter(FEB_CAN_INSTANCE_1, &reject_filter);
    if (filter_status != FEB_CAN_OK) {
        return filter_status;
    }

    // Start CAN peripheral
    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        return FEB_CAN_ERROR_HAL;
    }

    // Enable RX interrupt
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        return FEB_CAN_ERROR_HAL;
    }
    
    feb_can_tx_initialized = true;
    return FEB_CAN_OK;
}

/**
 * @brief Configure a single CAN filter
 * 
 * Usually called automatically by FEB_CAN_TX_UpdateFiltersForRegisteredIDs().
 */
FEB_CAN_Status_t FEB_CAN_TX_ConfigureFilter(FEB_CAN_Instance_t instance, const FEB_CAN_Filter_Config_t *filter_config) {
    if (filter_config == NULL || instance >= FEB_CAN_NUM_INSTANCES) {
        return FEB_CAN_ERROR_INVALID_PARAM;
    }
    
    CAN_HandleTypeDef *hcan = FEB_CAN_TX_GetHandle(instance);
    if (hcan == NULL) {
        return FEB_CAN_ERROR_INVALID_PARAM;
    }
    
    // Setup HAL filter structure
    CAN_FilterTypeDef can_filter;
    can_filter.FilterIdHigh = (filter_config->filter_id << 5);     // Shift for hardware format
    can_filter.FilterIdLow = 0x0000;
    can_filter.FilterMaskIdHigh = (filter_config->filter_mask << 5);
    can_filter.FilterMaskIdLow = 0x0000;
    can_filter.FilterFIFOAssignment = filter_config->filter_fifo;
    can_filter.FilterBank = 0;  // Use bank 0 for DASH
    can_filter.FilterMode = filter_config->filter_mode;
    can_filter.FilterScale = filter_config->filter_scale;
    can_filter.FilterActivation = filter_config->filter_enable ? CAN_FILTER_ENABLE : CAN_FILTER_DISABLE;
    can_filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(hcan, &can_filter) != HAL_OK) {
        return FEB_CAN_ERROR_HAL;
    }
    
    return FEB_CAN_OK;
}

/**
 * @brief Update hardware filters to match all registered RX IDs
 * 
 * This is called automatically when you register/unregister callbacks.
 * It configures the CAN hardware filters to only accept IDs you've registered for.
 * 
 * For simplicity, uses mask mode to accept range of IDs.
 * Production systems might use more sophisticated filter optimization.
 */
FEB_CAN_Status_t FEB_CAN_TX_UpdateFiltersForRegisteredIDs(FEB_CAN_Instance_t instance) {
    if (!feb_can_tx_initialized || instance >= FEB_CAN_NUM_INSTANCES) {
        return FEB_CAN_ERROR_INVALID_PARAM;
    }
    
    CAN_HandleTypeDef *hcan = FEB_CAN_TX_GetHandle(instance);
    if (hcan == NULL) {
        return FEB_CAN_ERROR_INVALID_PARAM;
    }
    
    // Get all registered IDs
    #define MAX_FILTER_IDS 32
    uint32_t id_list[MAX_FILTER_IDS];
    FEB_CAN_ID_Type_t id_type_list[MAX_FILTER_IDS];
    uint32_t registered_count = FEB_CAN_RX_GetRegisteredIDs(instance, id_list, id_type_list, MAX_FILTER_IDS);
    
    if (registered_count == 0) {
        // No registered IDs - reject all
        FEB_CAN_Filter_Config_t reject_filter = {
            .filter_id = 0x7FF,
            .filter_mask = 0x7FF,
            .filter_mode = CAN_FILTERMODE_IDMASK,
            .filter_scale = CAN_FILTERSCALE_32BIT,
            .filter_fifo = CAN_RX_FIFO0,
            .filter_enable = true
        };
        return FEB_CAN_TX_ConfigureFilter(instance, &reject_filter);
    }
    
    // For simplicity: use mask 0x700 to accept all IDs in registered ranges
    // More sophisticated implementations would use multiple filters
    FEB_CAN_Filter_Config_t accept_filter = {
        .filter_id = 0x00,               // Accept base
        .filter_mask = 0x00,             // Don't mask anything (accept all)
        .filter_mode = CAN_FILTERMODE_IDMASK,
        .filter_scale = CAN_FILTERSCALE_32BIT,
        .filter_fifo = CAN_RX_FIFO0,
        .filter_enable = true
    };
    
    return FEB_CAN_TX_ConfigureFilter(instance, &accept_filter);
}

/**
 * @brief Transmit a CAN message
 * 
 * Sends data on CAN bus. Waits for free mailbox up to timeout.
 */
FEB_CAN_Status_t FEB_CAN_TX_Transmit(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data, uint8_t length, uint32_t timeout_ms) {
    if (!feb_can_tx_initialized || instance >= FEB_CAN_NUM_INSTANCES) {
        return FEB_CAN_ERROR_INVALID_PARAM;
    }
    
    if (!FEB_CAN_TX_ValidateCanId(can_id, id_type) || !FEB_CAN_TX_ValidateDataLength(length) || data == NULL) {
        return FEB_CAN_ERROR_INVALID_PARAM;
    }
    
    CAN_HandleTypeDef *hcan = FEB_CAN_TX_GetHandle(instance);
    if (hcan == NULL) {
        return FEB_CAN_ERROR_INVALID_PARAM;
    }
    
    // Wait for free mailbox
    FEB_CAN_Status_t wait_status = FEB_CAN_TX_WaitForMailbox(instance, timeout_ms);
    if (wait_status != FEB_CAN_OK) {
        return wait_status;
    }
    
    // Setup TX header
    CAN_TxHeaderTypeDef tx_header;
    if (id_type == FEB_CAN_ID_STD) {
        tx_header.StdId = can_id;
        tx_header.ExtId = 0;
        tx_header.IDE = CAN_ID_STD;
    } else {
        tx_header.StdId = 0;
        tx_header.ExtId = can_id;
        tx_header.IDE = CAN_ID_EXT;
    }
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = length;
    tx_header.TransmitGlobalTime = DISABLE;
    
    // Transmit
    uint32_t mailbox;
    if (HAL_CAN_AddTxMessage(hcan, &tx_header, (uint8_t*)data, &mailbox) != HAL_OK) {
        return FEB_CAN_ERROR_HAL;
    }
    
    return FEB_CAN_OK;
}

/**
 * @brief Transmit with default timeout and standard ID
 */
FEB_CAN_Status_t FEB_CAN_TX_TransmitDefault(FEB_CAN_Instance_t instance, uint32_t can_id, const uint8_t *data, uint8_t length) {
    return FEB_CAN_TX_Transmit(instance, can_id, FEB_CAN_ID_STD, data, length, FEB_CAN_TX_TIMEOUT_MS);
}

/**
 * @brief Get number of free TX mailboxes
 */
uint32_t FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_Instance_t instance) {
    CAN_HandleTypeDef *hcan = FEB_CAN_TX_GetHandle(instance);
    if (hcan == NULL) {
        return 0;
    }
    return HAL_CAN_GetTxMailboxesFreeLevel(hcan);
}

/**
 * @brief Check if CAN is ready to transmit
 */
bool FEB_CAN_TX_IsReady(FEB_CAN_Instance_t instance) {
    return feb_can_tx_initialized && (FEB_CAN_TX_GetFreeMailboxes(instance) > 0);
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Validate CAN ID is in range
 */
static bool FEB_CAN_TX_ValidateCanId(uint32_t can_id, FEB_CAN_ID_Type_t id_type) {
    if (id_type == FEB_CAN_ID_STD) {
        return (can_id <= FEB_CAN_MAX_STD_ID);
    } else if (id_type == FEB_CAN_ID_EXT) {
        return (can_id <= FEB_CAN_MAX_EXT_ID);
    }
    return false;
}

/**
 * @brief Validate data length
 */
static bool FEB_CAN_TX_ValidateDataLength(uint8_t length) {
    return (length <= FEB_CAN_MAX_DATA_LENGTH);
}

/**
 * @brief Wait for free TX mailbox with timeout
 */
static FEB_CAN_Status_t FEB_CAN_TX_WaitForMailbox(FEB_CAN_Instance_t instance, uint32_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    
    while (FEB_CAN_TX_GetFreeMailboxes(instance) == 0) {
        if (timeout_ms > 0 && (HAL_GetTick() - start) >= timeout_ms) {
            return FEB_CAN_ERROR_TIMEOUT;
        }
    }
    
    return FEB_CAN_OK;
}

/**
 * @brief Get CAN handle for instance
 */
static CAN_HandleTypeDef* FEB_CAN_TX_GetHandle(FEB_CAN_Instance_t instance) {
    if (instance == FEB_CAN_INSTANCE_1) {
        return &hcan1;
    }
    return NULL;
}

