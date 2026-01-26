#include "FEB_CAN_RX.h"
#include <string.h>

/* Private defines -----------------------------------------------------------*/
#define FEB_CAN_RX_MAX_HANDLES 32     // Increased capacity
#define FEB_CAN_MAX_STD_ID 0x7FF      // Maximum 11-bit standard CAN ID
#define FEB_CAN_MAX_EXT_ID 0x1FFFFFFF // Maximum 29-bit extended CAN ID
#define FEB_CAN_NUM_INSTANCES 2       // Number of CAN instances

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  FEB_CAN_RX_Callback_t callback;
  uint32_t can_id;
  FEB_CAN_ID_Type_t id_type;
  FEB_CAN_Instance_t instance;
  bool is_active;
} FEB_CAN_RX_Handle_t;

/* Private variables ---------------------------------------------------------*/
static FEB_CAN_RX_Handle_t feb_can_rx_handles[FEB_CAN_RX_MAX_HANDLES];
static CAN_RxHeaderTypeDef feb_can_rx_header[FEB_CAN_NUM_INSTANCES];
static uint8_t feb_can_rx_data[FEB_CAN_NUM_INSTANCES][8];
static uint32_t feb_can_rx_registered_count = 0;
static bool feb_can_rx_initialized = false;

/* External variables --------------------------------------------------------*/
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

/* Private function prototypes -----------------------------------------------*/
static int32_t FEB_CAN_RX_FindHandle(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type);
static int32_t FEB_CAN_RX_FindFreeHandle(void);
static bool FEB_CAN_RX_ValidateCanId(uint32_t can_id, FEB_CAN_ID_Type_t id_type);
static FEB_CAN_Instance_t FEB_CAN_RX_GetInstanceFromHandle(CAN_HandleTypeDef *hcan);

/* Public functions ----------------------------------------------------------*/

FEB_CAN_Status_t FEB_CAN_RX_Init(void)
{
  // Clear all handles
  memset(feb_can_rx_handles, 0, sizeof(feb_can_rx_handles));
  feb_can_rx_registered_count = 0;
  feb_can_rx_initialized = true;

  return FEB_CAN_OK;
}

FEB_CAN_Status_t FEB_CAN_RX_Register(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                     FEB_CAN_RX_Callback_t callback)
{
  // Input validation
  if (!feb_can_rx_initialized)
  {
    return FEB_CAN_ERROR;
  }

  if (!FEB_CAN_RX_ValidateCanId(can_id, id_type))
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (callback == NULL)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (instance >= FEB_CAN_NUM_INSTANCES)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  // Check if already registered
  if (FEB_CAN_RX_FindHandle(instance, can_id, id_type) >= 0)
  {
    return FEB_CAN_ERROR_ALREADY_EXISTS;
  }

  // Find free handle
  int32_t handle_index = FEB_CAN_RX_FindFreeHandle();
  if (handle_index < 0)
  {
    return FEB_CAN_ERROR_FULL;
  }

  // Register the callback
  feb_can_rx_handles[handle_index].can_id = can_id;
  feb_can_rx_handles[handle_index].id_type = id_type;
  feb_can_rx_handles[handle_index].instance = instance;
  feb_can_rx_handles[handle_index].callback = callback;
  feb_can_rx_handles[handle_index].is_active = true;
  feb_can_rx_registered_count++;

  // Update filters to include the new ID (declare function locally to avoid circular dependency)
  extern FEB_CAN_Status_t FEB_CAN_TX_UpdateFiltersForRegisteredIDs(FEB_CAN_Instance_t instance);
  FEB_CAN_TX_UpdateFiltersForRegisteredIDs(instance);

  return FEB_CAN_OK;
}

FEB_CAN_Status_t FEB_CAN_RX_Unregister(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type)
{
  // Input validation
  if (!feb_can_rx_initialized)
  {
    return FEB_CAN_ERROR;
  }

  if (!FEB_CAN_RX_ValidateCanId(can_id, id_type))
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  if (instance >= FEB_CAN_NUM_INSTANCES)
  {
    return FEB_CAN_ERROR_INVALID_PARAM;
  }

  // Find the handle
  int32_t handle_index = FEB_CAN_RX_FindHandle(instance, can_id, id_type);
  if (handle_index < 0)
  {
    return FEB_CAN_ERROR_NOT_FOUND;
  }

  // Unregister the callback
  feb_can_rx_handles[handle_index].can_id = 0;
  feb_can_rx_handles[handle_index].id_type = FEB_CAN_ID_STD;
  feb_can_rx_handles[handle_index].instance = FEB_CAN_INSTANCE_1;
  feb_can_rx_handles[handle_index].callback = NULL;
  feb_can_rx_handles[handle_index].is_active = false;
  feb_can_rx_registered_count--;

  // Update filters to remove the unregistered ID
  extern FEB_CAN_Status_t FEB_CAN_TX_UpdateFiltersForRegisteredIDs(FEB_CAN_Instance_t instance);
  FEB_CAN_TX_UpdateFiltersForRegisteredIDs(instance);

  return FEB_CAN_OK;
}

bool FEB_CAN_RX_IsRegistered(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type)
{
  if (!feb_can_rx_initialized || !FEB_CAN_RX_ValidateCanId(can_id, id_type) || instance >= FEB_CAN_NUM_INSTANCES)
  {
    return false;
  }

  return (FEB_CAN_RX_FindHandle(instance, can_id, id_type) >= 0);
}

uint32_t FEB_CAN_RX_GetRegisteredCount(void)
{
  return feb_can_rx_registered_count;
}

uint32_t FEB_CAN_RX_GetRegisteredIDs(FEB_CAN_Instance_t instance, uint32_t *id_list, FEB_CAN_ID_Type_t *id_type_list,
                                     uint32_t max_count)
{
  if (!feb_can_rx_initialized || instance >= FEB_CAN_NUM_INSTANCES || id_list == NULL || id_type_list == NULL ||
      max_count == 0)
  {
    return 0;
  }

  uint32_t count = 0;
  for (uint32_t i = 0; i < FEB_CAN_RX_MAX_HANDLES && count < max_count; i++)
  {
    if (feb_can_rx_handles[i].is_active && feb_can_rx_handles[i].instance == instance)
    {
      id_list[count] = feb_can_rx_handles[i].can_id;
      id_type_list[count] = feb_can_rx_handles[i].id_type;
      count++;
    }
  }

  return count;
}

/* HAL Callbacks -------------------------------------------------------------*/
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  if (!feb_can_rx_initialized)
  {
    return;
  }

  FEB_CAN_Instance_t instance = FEB_CAN_RX_GetInstanceFromHandle(hcan);
  if (instance >= FEB_CAN_NUM_INSTANCES)
  {
    return;
  }

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &feb_can_rx_header[instance], feb_can_rx_data[instance]) == HAL_OK)
  {
    uint32_t can_id;
    FEB_CAN_ID_Type_t id_type;

    // Determine ID type and extract ID
    if (feb_can_rx_header[instance].IDE == CAN_ID_STD)
    {
      can_id = feb_can_rx_header[instance].StdId;
      id_type = FEB_CAN_ID_STD;
    }
    else
    {
      can_id = feb_can_rx_header[instance].ExtId;
      id_type = FEB_CAN_ID_EXT;
    }

    // Find matching callback
    int32_t handle_index = FEB_CAN_RX_FindHandle(instance, can_id, id_type);
    if (handle_index >= 0)
    {
      feb_can_rx_handles[handle_index].callback(instance, can_id, id_type, feb_can_rx_data[instance],
                                                feb_can_rx_header[instance].DLC);
    }
  }
}

/* Private functions ---------------------------------------------------------*/

static int32_t FEB_CAN_RX_FindHandle(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type)
{
  for (uint32_t i = 0; i < FEB_CAN_RX_MAX_HANDLES; i++)
  {
    if (feb_can_rx_handles[i].is_active && feb_can_rx_handles[i].can_id == can_id &&
        feb_can_rx_handles[i].id_type == id_type && feb_can_rx_handles[i].instance == instance)
    {
      return (int32_t)i;
    }
  }
  return -1;
}

static int32_t FEB_CAN_RX_FindFreeHandle(void)
{
  for (uint32_t i = 0; i < FEB_CAN_RX_MAX_HANDLES; i++)
  {
    if (!feb_can_rx_handles[i].is_active)
    {
      return (int32_t)i;
    }
  }
  return -1;
}

static bool FEB_CAN_RX_ValidateCanId(uint32_t can_id, FEB_CAN_ID_Type_t id_type)
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

static FEB_CAN_Instance_t FEB_CAN_RX_GetInstanceFromHandle(CAN_HandleTypeDef *hcan)
{
  if (hcan->Instance == CAN1)
  {
    return FEB_CAN_INSTANCE_1;
  }
  else if (hcan->Instance == CAN2)
  {
    return FEB_CAN_INSTANCE_2;
  }
  return FEB_CAN_NUM_INSTANCES; // Invalid instance
}
