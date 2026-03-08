#include "FEB_CAN_BMS.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
extern uint8_t FEB_CAN_Tx_Data[8];
extern uint32_t FEB_CAN_Tx_Mailbox;
extern UART_HandleTypeDef huart2;

uint8_t FEB_CAN_BMS_Filter(CAN_HandleTypeDef *hcan, uint8_t FIFO_assignment, uint8_t filter_bank)
{
  CAN_FilterTypeDef filter_config;

  // Standard CAN - 2.0A - 11 bit
  filter_config.FilterActivation = CAN_FILTER_ENABLE;
  filter_config.FilterBank = filter_bank;
  filter_config.FilterFIFOAssignment = FIFO_assignment;
  filter_config.FilterIdHigh = FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID << 5;
  filter_config.FilterIdLow = 0;
  filter_config.FilterMaskIdHigh = 0xFFE0;
  filter_config.FilterMaskIdLow = 0;
  filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
  filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
  filter_config.SlaveStartFilterBank = 27;

  if (HAL_CAN_ConfigFilter(hcan, &filter_config) != HAL_OK)
  {
    // Code Error - Shutdown
  }

  return ++filter_bank;
}

void FEB_CAN_BMS_Process_Message(CAN_RxHeaderTypeDef *rx_header, uint8_t FEB_CAN_Rx_Data[])
{
  switch (rx_header->ExtId)
  {
  case FEB_CAN_BMS_ACCUMULATOR_TEMPERATURE_FRAME_ID:
    FEB_Fan_CAN_Msg_Process(FEB_CAN_Rx_Data);
    break;
  }
}
