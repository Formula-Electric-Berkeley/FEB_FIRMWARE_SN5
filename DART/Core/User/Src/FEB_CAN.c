// **************************************** Includes & External ****************************************

#include "FEB_CAN.h"

extern CAN_HandleTypeDef hcan;

// **************************************** CAN Configuration ****************************************

CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
static CAN_RxHeaderTypeDef FEB_CAN_Rx_Header;

uint8_t FEB_CAN_Tx_Data[8];
uint8_t FEB_CAN_Rx_Data[8];

uint32_t FEB_CAN_Tx_Mailbox;

// TX/RX diagnostics (read by console / log)
static uint32_t tx_timeout_count = 0;
static uint32_t tx_hal_error_count = 0;
static uint32_t rx_count = 0;

// Hard ceiling on the busy-wait for a free TX mailbox.
#define FEB_CAN_TX_WAIT_MS 5u

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
    rx_count++;
    FEB_CAN_BMS_Process_Message(&FEB_CAN_Rx_Header, FEB_CAN_Rx_Data);
  }
}

// Wait up to FEB_CAN_TX_WAIT_MS for any mailbox to free; returns false on timeout.
static bool wait_for_tx_mailbox(CAN_HandleTypeDef *hcan)
{
  uint32_t start = HAL_GetTick();
  while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0)
  {
    if ((HAL_GetTick() - start) >= FEB_CAN_TX_WAIT_MS)
    {
      tx_timeout_count++;
      return false;
    }
  }
  return true;
}

static void send_frame(CAN_HandleTypeDef *hcan, uint16_t std_id, uint32_t is_ext, uint8_t dlc, const uint8_t *payload)
{
  FEB_CAN_Tx_Header.StdId = std_id;
  FEB_CAN_Tx_Header.IDE = is_ext;
  FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA;
  FEB_CAN_Tx_Header.DLC = dlc;

  memcpy(FEB_CAN_Tx_Data, payload, dlc);

  if (!wait_for_tx_mailbox(hcan))
  {
    return;
  }

  if (HAL_CAN_AddTxMessage(hcan, &FEB_CAN_Tx_Header, FEB_CAN_Tx_Data, &FEB_CAN_Tx_Mailbox) != HAL_OK)
  {
    tx_hal_error_count++;
  }
}

void FEB_CAN_Transmit(CAN_HandleTypeDef *hcan, const uint16_t *frequency_hz)
{
  struct feb_can_dart_tach_measurements_1234_t tx1234 = {
      .fan1_speed = frequency_hz[0],
      .fan2_speed = frequency_hz[1],
      .fan3_speed = frequency_hz[2],
      .fan4_speed = frequency_hz[3],
  };
  uint8_t buf1234[FEB_CAN_DART_TACH_MEASUREMENTS_1234_LENGTH];
  if (feb_can_dart_tach_measurements_1234_pack(buf1234, &tx1234, sizeof(buf1234)) > 0)
  {
    send_frame(hcan, FEB_CAN_DART_TACH_MEASUREMENTS_1234_FRAME_ID, FEB_CAN_DART_TACH_MEASUREMENTS_1234_IS_EXTENDED,
               FEB_CAN_DART_TACH_MEASUREMENTS_1234_LENGTH, buf1234);
  }

  struct feb_can_dart_tach_measurements_5_t tx5 = {.fan5_speed = frequency_hz[4]};
  uint8_t buf5[FEB_CAN_DART_TACH_MEASUREMENTS_5_LENGTH];
  if (feb_can_dart_tach_measurements_5_pack(buf5, &tx5, sizeof(buf5)) > 0)
  {
    send_frame(hcan, FEB_CAN_DART_TACH_MEASUREMENTS_5_FRAME_ID, FEB_CAN_DART_TACH_MEASUREMENTS_5_IS_EXTENDED,
               FEB_CAN_DART_TACH_MEASUREMENTS_5_LENGTH, buf5);
  }
}

uint32_t FEB_CAN_GetTxTimeoutCount(void)
{
  return tx_timeout_count;
}
uint32_t FEB_CAN_GetTxHalErrorCount(void)
{
  return tx_hal_error_count;
}
uint32_t FEB_CAN_GetRxCount(void)
{
  return rx_count;
}
