// **************************************** Includes & External ****************************************

#include "FEB_CAN.h"

extern CAN_HandleTypeDef hcan;

// **************************************** CAN Configuration ****************************************

CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
static CAN_RxHeaderTypeDef FEB_CAN_Rx_Header;

uint8_t FEB_CAN_Tx_Data[8];
uint8_t FEB_CAN_Rx_Data[8];

uint32_t FEB_CAN_Tx_Mailbox;

// **************************************** Functions ****************************************

void FEB_CAN_Init(void)
{

  FEB_CAN_Filter_Config();

  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
}

void FEB_CAN_Filter_Config(void)
{
  uint8_t filter_bank = 0;

  filter_bank = FEB_CAN_BMS_Filter(&hcan, CAN_RX_FIFO0, filter_bank);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &FEB_CAN_Rx_Header, FEB_CAN_Rx_Data) == HAL_OK)
  {

    FEB_CAN_BMS_Process_Message(&FEB_CAN_Rx_Header, FEB_CAN_Rx_Data);
  }
}

void FEB_CAN_Transmit(CAN_HandleTypeDef *hcan, uint8_t *can_data)
{

  // Initialize Transmission Header
  FEB_CAN_Tx_Header.StdId = FEB_CAN_DART_TACH_MEASUREMENTS_1234_FRAME_ID;
  FEB_CAN_Tx_Header.IDE = FEB_CAN_DART_TACH_MEASUREMENTS_1234_IS_EXTENDED;
  FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA;
  FEB_CAN_Tx_Header.DLC = FEB_CAN_DART_TACH_MEASUREMENTS_1234_LENGTH;

  // Configure FEB_CAN_Tx_Data
  memcpy(FEB_CAN_Tx_Data, can_data, FEB_CAN_DART_TACH_MEASUREMENTS_1234_LENGTH);

  // Delay until mailbox available
  while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0)
  {
  }

  // Add Tx data to mailbox
  if (HAL_CAN_AddTxMessage(hcan, &FEB_CAN_Tx_Header, FEB_CAN_Tx_Data, &FEB_CAN_Tx_Mailbox) != HAL_OK)
  {
    // Code Error - Shutdown
  }

  // Initialize Transmission Header
  FEB_CAN_Tx_Header.StdId = FEB_CAN_DART_TACH_MEASUREMENTS_5_FRAME_ID;
  FEB_CAN_Tx_Header.IDE = FEB_CAN_DART_TACH_MEASUREMENTS_5_IS_EXTENDED;
  FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA;
  FEB_CAN_Tx_Header.DLC = FEB_CAN_DART_TACH_MEASUREMENTS_5_LENGTH;

  // Configure FEB_CAN_Tx_Data
  memcpy(FEB_CAN_Tx_Data, can_data + FEB_CAN_DART_TACH_MEASUREMENTS_1234_LENGTH,
         FEB_CAN_DART_TACH_MEASUREMENTS_5_LENGTH);

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
