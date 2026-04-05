#include "FEB_CAN.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

static CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
static CAN_RxHeaderTypeDef FEB_CAN_Rx_Header1;
static CAN_RxHeaderTypeDef FEB_CAN_Rx_Header2;
static uint8_t FEB_CAN_Tx_Data[8];
static uint8_t FEB_CAN_Rx_Data1[8];
static uint8_t FEB_CAN_Rx_Data2[8];
static uint32_t FEB_CAN_Tx_Mailbox;
static FEB_CAN_Rx_Callback_t FEB_CAN_Rx_Callback;

static CAN_HandleTypeDef *FEB_CAN_GetHandle(FEB_CAN_Bus_t bus)
{
  return (bus == FEB_CAN_BUS_1) ? &hcan1 : &hcan2;
}

void FEB_CAN_Init(FEB_CAN_Rx_Callback_t callback)
{
  FEB_CAN_Filter_Config();

  // Start CAN1
  if (HAL_CAN_Start(&hcan1) != HAL_OK)
  {
    // Error handling
  }
  if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    // Error handling
  }

  // Start CAN2
  if (HAL_CAN_Start(&hcan2) != HAL_OK)
  {
    // Error handling
  }
  if (HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    // Error handling
  }

  FEB_CAN_Rx_Callback = callback;
}

void FEB_CAN_Filter_Config(void)
{
  CAN_FilterTypeDef filter_config;

  // CAN1 filter (filter banks 0-13)
  filter_config.FilterActivation = CAN_FILTER_ENABLE;
  filter_config.FilterBank = 0;
  filter_config.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
  filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
  filter_config.FilterIdHigh = 0;
  filter_config.FilterIdLow = 0;
  filter_config.FilterMaskIdHigh = 0;
  filter_config.FilterMaskIdLow = 0;
  filter_config.SlaveStartFilterBank = 14; // CAN2 starts at bank 14
  HAL_CAN_ConfigFilter(&hcan1, &filter_config);

  // CAN2 filter (filter banks 14-27)
  filter_config.FilterBank = 14;
  HAL_CAN_ConfigFilter(&hcan2, &filter_config);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  FEB_CAN_Bus_t bus;
  CAN_RxHeaderTypeDef *rx_header;
  uint8_t *rx_data;

  if (hcan == &hcan1)
  {
    bus = FEB_CAN_BUS_1;
    rx_header = &FEB_CAN_Rx_Header1;
    rx_data = FEB_CAN_Rx_Data1;
  }
  else
  {
    bus = FEB_CAN_BUS_2;
    rx_header = &FEB_CAN_Rx_Header2;
    rx_data = FEB_CAN_Rx_Data2;
  }

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, rx_header, rx_data) == HAL_OK)
  {
    if (FEB_CAN_Rx_Callback != NULL)
    {
      FEB_CAN_Rx_Callback(bus, rx_header, rx_data);
    }
  }
}

void FEB_CAN_Transmit(FEB_CAN_Bus_t bus, uint32_t std_id, uint8_t *data, uint8_t length)
{
  CAN_HandleTypeDef *hcan = FEB_CAN_GetHandle(bus);

  FEB_CAN_Tx_Header.StdId = std_id;
  FEB_CAN_Tx_Header.IDE = CAN_ID_STD;
  FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA;
  FEB_CAN_Tx_Header.DLC = length;

  memcpy(FEB_CAN_Tx_Data, data, length);

  while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0)
  {
  }

  HAL_CAN_AddTxMessage(hcan, &FEB_CAN_Tx_Header, FEB_CAN_Tx_Data, &FEB_CAN_Tx_Mailbox);
}

void FEB_CAN_PingPong_Send(FEB_CAN_Bus_t bus, uint8_t counter_id, int32_t counter_value)
{
  uint8_t data[8] = {0};
  uint32_t frame_id;

  // Map counter_id (0-3) to frame IDs (0xE0-0xE3)
  switch (counter_id)
  {
  case 0:
    frame_id = FEB_CAN_FEB_PING_PONG_COUNTER1_FRAME_ID;
    break;
  case 1:
    frame_id = FEB_CAN_FEB_PING_PONG_COUNTER2_FRAME_ID;
    break;
  case 2:
    frame_id = FEB_CAN_FEB_PING_PONG_COUNTER3_FRAME_ID;
    break;
  case 3:
    frame_id = FEB_CAN_FEB_PING_PONG_COUNTER4_FRAME_ID;
    break;
  default:
    return;
  }

  // Pack counter value (little endian, 32-bit signed)
  data[0] = (uint8_t)(counter_value & 0xFF);
  data[1] = (uint8_t)((counter_value >> 8) & 0xFF);
  data[2] = (uint8_t)((counter_value >> 16) & 0xFF);
  data[3] = (uint8_t)((counter_value >> 24) & 0xFF);

  FEB_CAN_Transmit(bus, frame_id, data, 8);
}
