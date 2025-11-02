// **************************************** Includes & External ****************************************

#include "FEB_CAN.h"

extern CAN_HandleTypeDef hcan1;
extern UART_HandleTypeDef huart3;

// **************************************** CAN Configuration ****************************************

CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
static CAN_RxHeaderTypeDef FEB_CAN_Rx_Header;

uint8_t FEB_CAN_Tx_Data[8];
uint8_t FEB_CAN_Rx_Data[8];

uint32_t FEB_CAN_Tx_Mailbox;

// **************************************** Functions ****************************************

void FEB_CAN_Init(void) {
	FEB_CAN_Filter_Config();
	if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        // Code Error - Shutdown
	}
	HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
}

void FEB_CAN_Filter_Config(void) {
	uint8_t filter_bank = 0;
    filter_bank = FEB_CAN_DASH_Filter(&hcan1, CAN_RX_FIFO0, filter_bank);
    filter_bank = FEB_CAN_PCU_Filter(&hcan1, CAN_RX_FIFO0, filter_bank);
    filter_bank = FEB_CAN_BMS_Filter_Config(&hcan1, CAN_RX_FIFO0, filter_bank);

	// Assign Filter
    // filter_bank = Function(&hcan1, CAN_RX_FIFO0, filter_bank);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &FEB_CAN_Rx_Header, FEB_CAN_Rx_Data) == HAL_OK) {
		FEB_CAN_DASH_Rx_Handler(&FEB_CAN_Rx_Header, FEB_CAN_Rx_Data);
		FEB_CAN_PCU_Str_Msg(&FEB_CAN_Rx_Header, FEB_CAN_Rx_Data);		// Store Message
		FEB_CAN_BMS_Store_Msg(&FEB_CAN_Rx_Header, FEB_CAN_Rx_Data);

        // Function(&FEB_CAN_Rx_Header, FEB_CAN_Rx_Data);
	}
}

// **************************************** Template Code [Other Files] ****************************************

uint8_t FEB_CAN_Filter(CAN_HandleTypeDef* hcan, uint8_t FIFO_assignment, uint8_t filter_bank) {
    // For multiple filters, create array of filter IDs and loop over IDs.

	CAN_FilterTypeDef filter_config;

    // Standard CAN - 2.0A - 11 bit
    filter_config.FilterActivation = CAN_FILTER_ENABLE;
	filter_config.FilterBank = filter_bank;
	filter_config.FilterFIFOAssignment = FIFO_assignment;
	filter_config.FilterIdHigh = /* ID */ 0 << 5;
	filter_config.FilterIdLow = 0;
	filter_config.FilterMaskIdHigh = 0xFFE0;
	filter_config.FilterMaskIdLow = 0;
	filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
	filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
	filter_config.SlaveStartFilterBank = 27;
    filter_bank++;

    // Extended CAN - 2.0B - 29 bit
	filter_config.FilterActivation = CAN_FILTER_ENABLE;
	filter_config.FilterBank = filter_bank;
	filter_config.FilterFIFOAssignment = FIFO_assignment;
	filter_config.FilterIdHigh = /* ID */ 0 >> 13;
	filter_config.FilterIdLow = (/* ID */ 0 & 0x1FFF) << 3;
	filter_config.FilterMaskIdHigh = 0xFFFF;
	filter_config.FilterMaskIdLow = 0xFFF8;
	filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
	filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
	filter_config.SlaveStartFilterBank = 27;
    filter_bank++;

	if (HAL_CAN_ConfigFilter(hcan, &filter_config) != HAL_OK) {
        // Code Error - Shutdown
	}

	return filter_bank;
}

//void FEB_CAN_Transmit(CAN_HandleTypeDef* hcan) {
//	// Initialize Transmission Header
//    // Write Code Here
//
//	// Configure FEB_CAN_Tx_Data
//    // Write Code Here
//
//	// Delay until mailbox available
//	while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0) {}
//
//	// Add Tx data to mailbox
//	if (HAL_CAN_AddTxMessage(hcan, &FEB_CAN_Tx_Header, FEB_CAN_Tx_Data, &FEB_CAN_Tx_Mailbox) != HAL_OK) {
//		// Code Error - Shutdown
//	}
//}
