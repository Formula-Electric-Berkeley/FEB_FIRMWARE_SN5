#include "FEB_CAN_TX.h"
#include "FEB_Debug.h"

/* Private defines -----------------------------------------------------------*/
#define FEB_CAN_MAX_STD_ID 0x7FF            // Maximum 11-bit standard CAN ID
#define FEB_CAN_MAX_EXT_ID 0x1FFFFFFF       // Maximum 29-bit extended CAN ID
#define FEB_CAN_MAX_DATA_LENGTH 8           // Maximum CAN data length
#define FEB_CAN_DEFAULT_FILTER_BANK 0       // Default filter bank
#define FEB_CAN_NUM_INSTANCES 2             // Number of CAN instances
#define FEB_CAN_MAX_FILTERS_PER_INSTANCE 14 // Maximum filters per CAN instance

/* Private variables ---------------------------------------------------------*/
static bool feb_can_tx_initialized = false;

/* External variables --------------------------------------------------------*/
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

/* Private function prototypes -----------------------------------------------*/
static bool FEB_CAN_TX_ValidateCanId(uint32_t can_id, FEB_CAN_ID_Type_t id_type);
static bool FEB_CAN_TX_ValidateDataLength(uint8_t length);
static FEB_CAN_Status_t FEB_CAN_TX_WaitForMailbox(FEB_CAN_Instance_t instance, uint32_t timeout_ms);
static CAN_HandleTypeDef *FEB_CAN_TX_GetHandle(FEB_CAN_Instance_t instance);
static uint32_t FEB_CAN_TX_GetFilterBank(FEB_CAN_Instance_t instance, uint32_t filter_index);

/* Public functions ----------------------------------------------------------*/

FEB_CAN_Status_t FEB_CAN_TX_Init(void)
{
  LOG_I(TAG_CAN, "Initializing CAN TX system");

  // Initialize RX system first
  FEB_CAN_Status_t rx_status = FEB_CAN_RX_Init();
  if (rx_status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to initialize CAN RX: %d", rx_status);
    return rx_status;
  }

  // Configure reject-all filter initially (no messages accepted until IDs are registered)
  FEB_CAN_Filter_Config_t reject_filter = {.filter_id = 0x7FF,   // Set to max standard ID
                                           .filter_mask = 0x7FF, // Exact match
                                           .filter_mode = CAN_FILTERMODE_IDMASK,
                                           .filter_scale = CAN_FILTERSCALE_32BIT,
                                           .filter_fifo = CAN_RX_FIFO0,
                                           .filter_enable = true};

  // Initialize filters for both instances
  FEB_CAN_Status_t filter_status1 = FEB_CAN_TX_ConfigureFilter(FEB_CAN_INSTANCE_1, &reject_filter);
  if (filter_status1 != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to configure CAN1 filter: %d", filter_status1);
    return filter_status1;
  }

  FEB_CAN_Status_t filter_status2 = FEB_CAN_TX_ConfigureFilter(FEB_CAN_INSTANCE_2, &reject_filter);
  if (filter_status2 != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to configure CAN2 filter: %d", filter_status2);
    return filter_status2;
  }

  // Start CAN peripherals
  if (HAL_CAN_Start(&hcan1) != HAL_OK)
  {
    LOG_E(TAG_CAN, "Failed to start CAN1");
    return FEB_CAN_ERROR_HAL;
  }

  if (HAL_CAN_Start(&hcan2) != HAL_OK)
  {
    LOG_E(TAG_CAN, "Failed to start CAN2");
    return FEB_CAN_ERROR_HAL;
  }

  // Activate RX notifications for both instances
  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    LOG_E(TAG_CAN, "Failed to activate CAN1 notifications");
    return FEB_CAN_ERROR_HAL;
  }

  if (HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    LOG_E(TAG_CAN, "Failed to activate CAN2 notifications");
    return FEB_CAN_ERROR_HAL;
  }

  feb_can_tx_initialized = true;
  LOG_I(TAG_CAN, "CAN TX system initialized successfully");
  return FEB_CAN_OK;
}

FEB_CAN_Status_t FEB_CAN_TX_ConfigureFilter(FEB_CAN_Instance_t instance, const FEB_CAN_Filter_Config_t *filter_config)
{
  // Input validation
  if (filter_config == NULL || instance >= FEB_CAN_NUM_INSTANCES)
  {
    LOG_E(TAG_CAN, "Invalid filter config parameters");
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  CAN_HandleTypeDef *hcan = FEB_CAN_TX_GetHandle(instance);
  if (hcan == NULL)
  {
    LOG_E(TAG_CAN, "Invalid CAN instance: %d", instance);
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  CAN_FilterTypeDef can_filter;

  can_filter.FilterIdHigh = (filter_config->filter_id << 5);
  can_filter.FilterIdLow = 0x0000;
  can_filter.FilterMaskIdHigh = (filter_config->filter_mask << 5);
  can_filter.FilterMaskIdLow = 0x0000;
  can_filter.FilterFIFOAssignment = filter_config->filter_fifo;
  can_filter.FilterBank = FEB_CAN_TX_GetFilterBank(instance, 0);
  can_filter.FilterMode = filter_config->filter_mode;
  can_filter.FilterScale = filter_config->filter_scale;
  can_filter.FilterActivation = filter_config->filter_enable ? CAN_FILTER_ENABLE : CAN_FILTER_DISABLE;
  can_filter.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(hcan, &can_filter) != HAL_OK)
  {
    LOG_E(TAG_CAN, "HAL filter configuration failed for instance %d", instance);
    return FEB_CAN_ERROR_HAL;
  }

  LOG_D(TAG_CAN, "Filter configured for CAN%d: ID=0x%03lX, Mask=0x%03lX", instance + 1, filter_config->filter_id,
        filter_config->filter_mask);

  return FEB_CAN_OK;
}

FEB_CAN_Status_t FEB_CAN_TX_UpdateFiltersForRegisteredIDs(FEB_CAN_Instance_t instance)
{
  if (!feb_can_tx_initialized || instance >= FEB_CAN_NUM_INSTANCES)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  CAN_HandleTypeDef *hcan = FEB_CAN_TX_GetHandle(instance);
  if (hcan == NULL)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

// Get registered IDs for this instance (use conservative buffer size)
#define MAX_FILTER_IDS 32
  uint32_t id_list[MAX_FILTER_IDS];
  FEB_CAN_ID_Type_t id_type_list[MAX_FILTER_IDS];
  uint32_t registered_count = FEB_CAN_RX_GetRegisteredIDs(instance, id_list, id_type_list, MAX_FILTER_IDS);

  if (registered_count == 0)
  {
    // No registered IDs - configure reject-all filter
    FEB_CAN_Filter_Config_t reject_filter = {.filter_id = 0x7FF,   // Set to max standard ID (will never match)
                                             .filter_mask = 0x7FF, // Exact match
                                             .filter_mode = CAN_FILTERMODE_IDMASK,
                                             .filter_scale = CAN_FILTERSCALE_32BIT,
                                             .filter_fifo = CAN_RX_FIFO0,
                                             .filter_enable = true};
    return FEB_CAN_TX_ConfigureFilter(instance, &reject_filter);
  }

  // For simplicity, we'll configure one filter per registered ID
  // In a production system, you might want to optimize by grouping IDs with masks
  for (uint32_t i = 0; i < registered_count && i < FEB_CAN_MAX_FILTERS_PER_INSTANCE; i++)
  {
    CAN_FilterTypeDef can_filter;

    if (id_type_list[i] == FEB_CAN_ID_STD)
    {
      // Standard ID filter
      can_filter.FilterIdHigh = (id_list[i] << 5);
      can_filter.FilterIdLow = 0x0000;
      can_filter.FilterMaskIdHigh = (0x7FF << 5); // Exact match for standard ID
      can_filter.FilterMaskIdLow = 0x0000;
    }
    else
    {
      // Extended ID filter
      can_filter.FilterIdHigh = (id_list[i] >> 13) & 0xFFFF;
      can_filter.FilterIdLow = ((id_list[i] << 3) | CAN_ID_EXT) & 0xFFFF;
      can_filter.FilterMaskIdHigh = 0xFFFF; // Exact match for extended ID
      can_filter.FilterMaskIdLow = 0xFFFF;
    }

    can_filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    can_filter.FilterBank = FEB_CAN_TX_GetFilterBank(instance, i);
    can_filter.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter.FilterActivation = CAN_FILTER_ENABLE;
    can_filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(hcan, &can_filter) != HAL_OK)
    {
      return FEB_CAN_ERROR_HAL;
    }
  }

  // Disable unused filter banks for this instance
  for (uint32_t i = registered_count; i < FEB_CAN_MAX_FILTERS_PER_INSTANCE; i++)
  {
    CAN_FilterTypeDef disable_filter;
    memset(&disable_filter, 0, sizeof(disable_filter));
    disable_filter.FilterBank = FEB_CAN_TX_GetFilterBank(instance, i);
    disable_filter.FilterActivation = CAN_FILTER_DISABLE;
    disable_filter.SlaveStartFilterBank = 14;

    HAL_CAN_ConfigFilter(hcan, &disable_filter); // Ignore errors for disable
  }

  return FEB_CAN_OK;
}

FEB_CAN_Status_t FEB_CAN_TX_Transmit(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                     const uint8_t *data, uint8_t length, uint32_t timeout_ms)
{
  // Input validation
  if (!feb_can_tx_initialized)
  {
    LOG_E(TAG_CAN, "CAN TX not initialized");
    return FEB_CAN_ERROR;
  }

  if (instance >= FEB_CAN_NUM_INSTANCES)
  {
    LOG_E(TAG_CAN, "Invalid CAN instance: %d", instance);
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (!FEB_CAN_TX_ValidateCanId(can_id, id_type))
  {
    LOG_E(TAG_CAN, "Invalid CAN ID: 0x%08lX (type: %d)", can_id, id_type);
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (!FEB_CAN_TX_ValidateDataLength(length))
  {
    LOG_E(TAG_CAN, "Invalid data length: %d", length);
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (data == NULL && length > 0)
  {
    LOG_E(TAG_CAN, "NULL data pointer with non-zero length");
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  // Get CAN handle
  CAN_HandleTypeDef *hcan = FEB_CAN_TX_GetHandle(instance);
  if (hcan == NULL)
  {
    LOG_E(TAG_CAN, "Failed to get CAN handle for instance %d", instance);
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  // Wait for available mailbox with timeout
  FEB_CAN_Status_t wait_status = FEB_CAN_TX_WaitForMailbox(instance, timeout_ms);
  if (wait_status != FEB_CAN_OK)
  {
    LOG_W(TAG_CAN, "Mailbox timeout for CAN%d, ID: 0x%03lX", instance + 1, can_id);
    return wait_status;
  }

  // Prepare TX header
  CAN_TxHeaderTypeDef tx_header;
  if (id_type == FEB_CAN_ID_STD)
  {
    tx_header.StdId = can_id;
    tx_header.IDE = CAN_ID_STD;
  }
  else
  {
    tx_header.ExtId = can_id;
    tx_header.IDE = CAN_ID_EXT;
  }
  tx_header.RTR = CAN_RTR_DATA;
  tx_header.DLC = length;
  tx_header.TransmitGlobalTime = DISABLE;

  // Copy data to local buffer to ensure const correctness
  uint8_t tx_data[FEB_CAN_MAX_DATA_LENGTH] = {0};
  if (length > 0 && data != NULL)
  {
    memcpy(tx_data, data, length);
  }

  // Transmit message
  uint32_t tx_mailbox;
  if (HAL_CAN_AddTxMessage(hcan, &tx_header, tx_data, &tx_mailbox) != HAL_OK)
  {
    LOG_E(TAG_CAN, "HAL transmit failed for CAN%d, ID: 0x%03lX", instance + 1, can_id);
    return FEB_CAN_ERROR_HAL;
  }

  LOG_D(TAG_CAN, "TX CAN%d: ID=0x%03lX, Len=%d, Mailbox=%lu, Data: %02X %02X %02X %02X %02X %02X %02X %02X",
        instance + 1, can_id, length, tx_mailbox, tx_data[0], tx_data[1], tx_data[2], tx_data[3], tx_data[4],
        tx_data[5], tx_data[6], tx_data[7]);

  return FEB_CAN_OK;
}

FEB_CAN_Status_t FEB_CAN_TX_TransmitDefault(FEB_CAN_Instance_t instance, uint32_t can_id, const uint8_t *data,
                                            uint8_t length)
{
  return FEB_CAN_TX_Transmit(instance, can_id, FEB_CAN_ID_STD, data, length, FEB_CAN_TX_TIMEOUT_MS);
}

uint32_t FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_Instance_t instance)
{
  if (!feb_can_tx_initialized || instance >= FEB_CAN_NUM_INSTANCES)
  {
    return 0;
  }

  CAN_HandleTypeDef *hcan = FEB_CAN_TX_GetHandle(instance);
  if (hcan == NULL)
  {
    return 0;
  }

  return HAL_CAN_GetTxMailboxesFreeLevel(hcan);
}

bool FEB_CAN_TX_IsReady(FEB_CAN_Instance_t instance)
{
  return feb_can_tx_initialized && (FEB_CAN_TX_GetFreeMailboxes(instance) > 0);
}

/* Private functions ---------------------------------------------------------*/

static bool FEB_CAN_TX_ValidateCanId(uint32_t can_id, FEB_CAN_ID_Type_t id_type)
{
  if (id_type == FEB_CAN_ID_STD)
  {
    return (can_id <= FEB_CAN_MAX_STD_ID);
  }
  else if (id_type == FEB_CAN_ID_EXT)
  {
    return (can_id <= FEB_CAN_MAX_EXT_ID);
  }
  return false;
}

static bool FEB_CAN_TX_ValidateDataLength(uint8_t length)
{
  return (length <= FEB_CAN_MAX_DATA_LENGTH);
}

static FEB_CAN_Status_t FEB_CAN_TX_WaitForMailbox(FEB_CAN_Instance_t instance, uint32_t timeout_ms)
{
  if (timeout_ms == 0)
  {
    // No timeout - just check once
    return (FEB_CAN_TX_GetFreeMailboxes(instance) > 0) ? FEB_CAN_OK : FEB_CAN_ERROR_TIMEOUT;
  }

  uint32_t start_time = HAL_GetTick();
  while ((HAL_GetTick() - start_time) < timeout_ms)
  {
    if (FEB_CAN_TX_GetFreeMailboxes(instance) > 0)
    {
      return FEB_CAN_OK;
    }
    // Small delay to prevent busy-waiting
    HAL_Delay(1);
  }

  return FEB_CAN_ERROR_TIMEOUT;
}

static CAN_HandleTypeDef *FEB_CAN_TX_GetHandle(FEB_CAN_Instance_t instance)
{
  if (instance == FEB_CAN_INSTANCE_1)
  {
    return &hcan1;
  }
  else if (instance == FEB_CAN_INSTANCE_2)
  {
    return &hcan2;
  }
  return NULL;
}

static uint32_t FEB_CAN_TX_GetFilterBank(FEB_CAN_Instance_t instance, uint32_t filter_index)
{
  // CAN1 uses filter banks 0-13, CAN2 uses filter banks 14-27
  if (instance == FEB_CAN_INSTANCE_1)
  {
    return filter_index % 14; // Filter banks 0-13 for CAN1
  }
  else if (instance == FEB_CAN_INSTANCE_2)
  {
    return 14 + (filter_index % 14); // Filter banks 14-27 for CAN2
  }
  return 0;
}
