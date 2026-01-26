// **************************************** Includes & External ****************************************

#include <FEB_CAN.h>

extern CAN_HandleTypeDef hcan1;
extern UART_HandleTypeDef huart2;

// **************************************** CAN Configuration ****************************************

CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;

static CAN_RxHeaderTypeDef FEB_CAN_Rx_Header;

// Indiates that we're using extended ID
uint32_t LVPDB_CAN_ID = 0;

// data to configure
uint8_t FEB_CAN_Tx_Data[8];

uint8_t FEB_CAN_Rx_Data[8];

uint32_t FEB_CAN_Tx_Mailbox;

// Rx callback function
static void (*FEB_CAN_Rx_Callback)(CAN_RxHeaderTypeDef *, void *);

// **************************************** Functions ****************************************

void FEB_CAN_Init(void (*CAN_Callback)(CAN_RxHeaderTypeDef *, void *))
{
  FEB_CAN_Filter_Config();
  if (HAL_CAN_Start(&hcan1) != HAL_OK)
  {
    // Code Error - Shutdown
  }

  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
  }

  FEB_CAN_Rx_Callback = CAN_Callback;
}

void FEB_CAN_Filter_Config(void)
{
  uint8_t filter_bank = 0;
  filter_bank = FEB_CAN_LVPDB_Filter(&hcan1, CAN_RX_FIFO0, filter_bank);
}

uint8_t FEB_CAN_LVPDB_Filter(CAN_HandleTypeDef *hcan, unsigned char FIFO_assignment, uint8_t filter_bank)
{
  // For multiple filters, create array of filter IDs and loop over IDs.
  CAN_FilterTypeDef filter_config;

  filter_config.FilterActivation = CAN_FILTER_ENABLE;
  filter_config.FilterBank = filter_bank;
  filter_config.FilterFIFOAssignment = FIFO_assignment;

  filter_config.FilterMode = CAN_FILTERMODE_IDLIST;
  filter_config.FilterScale = CAN_FILTERSCALE_16BIT;

  // For standard IDs, place them in the top 11 bits of each 16-bit half-word (left shift by 5)
  filter_config.FilterIdHigh = FEB_CAN_BRAKE_FRAME_ID << 5;
  filter_config.FilterIdLow = FEB_CAN_DASH_IO_FRAME_ID << 5;

  // Not used in IDLIST mode but must be initialized
  filter_config.FilterMaskIdHigh = 0;
  filter_config.FilterMaskIdLow = 0;

  filter_config.SlaveStartFilterBank = 27;
  filter_bank += 2;

  if (HAL_CAN_ConfigFilter(hcan, &filter_config) != HAL_OK)
  {
    // Code Error - Shutdown
  }

  return filter_bank;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &FEB_CAN_Rx_Header, FEB_CAN_Rx_Data) == HAL_OK)
  {

    FEB_CAN_Rx_Callback(&FEB_CAN_Rx_Header, FEB_CAN_Rx_Data);
  }
}

void FEB_CAN_Transmit(CAN_HandleTypeDef *hcan, FEB_LVPDB_CAN_Data *can_data)
{
  uint8_t packetCount = (can_data->flags >> 24) & 0x000F;

  // Initialize Transmission Header
  FEB_CAN_Tx_Header.StdId = can_data->ids[packetCount];
  FEB_CAN_Tx_Header.IDE = CAN_ID_STD;
  FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA;
  FEB_CAN_Tx_Header.DLC = 8;

  // Configure FEB_CAN_Tx_Data
  memcpy(FEB_CAN_Tx_Data, ((uint8_t *)(&can_data->flags) + (packetCount * 8)), 8);

  // Delay until mailbox available
  while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0)
  {
  }

  // Add Tx data to mailbox
  if (HAL_CAN_AddTxMessage(hcan, &FEB_CAN_Tx_Header, FEB_CAN_Tx_Data, &FEB_CAN_Tx_Mailbox) != HAL_OK)
  {
    // Code Error - Shutdown
  }
}
