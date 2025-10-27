// ********************************** Includes & External **********************************
#include "FEB_CAN_Heartbeat.h"

extern CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
extern uint32_t FEB_CAN_Tx_Mailbox;
extern CAN_HandleTypeDef hcan1; 
static uint8_t heartbeat[8];

// *********************************** Struct ************************************

// typedef struct BMS_MESSAGE_TYPE {
//     uint16_t temp;
//     FEB_SM_ST_t status;

// } BMS_MESSAGE_TYPE;
// BMS_MESSAGE_TYPE BMS_MESSAGE;


// ***** CAN FUNCTIONS ****
/* i dont think we need to config
uint8_t FEB_CAN_HEARTBEAT_Filter_Config(CAN_HandleTypeDef* hcan, uint8_t FIFO_assignment, uint8_t filter_bank) {
    uint16_t id = FEB_CAN_HEARTBEAT_ID; // Use the correct ID.

    CAN_FilterTypeDef filter_config;

    // Standard CAN - 2.0A - 11 bit
    filter_config.FilterActivation = CAN_FILTER_ENABLE;
    filter_config.FilterBank = filter_bank;
    filter_config.FilterFIFOAssignment = FIFO_assignment;
    filter_config.FilterIdHigh = id << 5;  // Corrected: Shift the ID properly.
    filter_config.FilterIdLow = 0;
    filter_config.FilterMaskIdHigh = 0xFFE0; // Corrected mask.
    filter_config.FilterMaskIdLow = 0;
    filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
    filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
    filter_config.SlaveStartFilterBank = 27; //check this value

    if (HAL_CAN_ConfigFilter(hcan, &filter_config) != HAL_OK) {
        printf("Error: CAN Filter Configuration Failed!\n");
        return 0xFF; // Return an error code.
    }

    return filter_bank;
}
*/

void FEB_CAN_HEARTBEAT_Transmit() {

    FEB_CAN_Tx_Header.StdId = FEB_CAN_LVPDB_HEARTBEAT_FRAME_ID;
    FEB_CAN_Tx_Header.ExtId = 0;
    FEB_CAN_Tx_Header.RTR = CAN_RTR_DATA; // Data frame.
    FEB_CAN_Tx_Header.IDE = CAN_ID_STD;    // Standard ID.
    FEB_CAN_Tx_Header.DLC = 8;       // Data Length Code (0-8 for standard CAN)
    FEB_CAN_Tx_Header.TransmitGlobalTime = DISABLE;

	// Delay until mailbox available
	while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {}

    if (HAL_CAN_AddTxMessage(&hcan1, &FEB_CAN_Tx_Header, heartbeat, &FEB_CAN_Tx_Mailbox) != HAL_OK) {
		// Code Error - Shutdown
	}

}

void FEB_CAN_HEARTBEAT_Init() {
	for ( int i = 0; i < 8; ++i ) {
		memset(((uint8_t *)&heartbeat) + i, 0xFF, sizeof(uint8_t));
	}

}

