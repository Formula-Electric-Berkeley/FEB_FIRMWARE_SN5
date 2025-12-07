// **************************************** Includes ****************************************

#include "FEB_CAN_ICS.h"

extern CAN_HandleTypeDef hcan2;
extern CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
extern uint8_t FEB_CAN_Tx_Data[8];
extern uint32_t FEB_CAN_Tx_Mailbox;

// ******************************** Variables ********************************

/* FOLLOW THE FOLLOWING CONVENTIONS:
	0 - R2D - This command should probably come from the APPS
	1 - Enable Accumulator Fans
	2 - Enable coolant pump
*/

bool ICS_States[3]; // Array to store the latest commands from the ICS via button


// **************************************** Functions ****************************************

uint8_t FEB_CAN_ICS_Filter(CAN_HandleTypeDef* hcan, uint8_t FIFO_assignment, uint8_t filter_bank) {
    // For multiple filters, create array of filter IDs and loop over IDs.

	uint16_t ids[] = {FEB_CAN_DASH_IO_FRAME_ID};

	for (uint8_t i = 0; i < 1; i++) {
		CAN_FilterTypeDef filter_config;

	    // Standard CAN - 2.0A - 11 bit
	    filter_config.FilterActivation = CAN_FILTER_ENABLE;
		filter_config.FilterBank = filter_bank;
		filter_config.FilterFIFOAssignment = FIFO_assignment;
		filter_config.FilterIdHigh = ids[i] << 5;
		filter_config.FilterIdLow = 0;
		filter_config.FilterMaskIdHigh = 0xFFE0;
		filter_config.FilterMaskIdLow = 0;
		filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
		filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
		filter_config.SlaveStartFilterBank = 14;
	    filter_bank++;

		if (HAL_CAN_ConfigFilter(hcan, &filter_config) != HAL_OK) {

		}
	}

	return filter_bank;
}

void FEB_CAN_ICS_Store_Msg(CAN_RxHeaderTypeDef *rx_header, uint8_t rx_data[]) {
	switch(rx_header->StdId) {
		case FEB_CAN_DASH_IO_FRAME_ID:
				ICS_States[0] = ((rx_data[0] | 0b11111101) == 0b11111111);
				ICS_States[1] = ((rx_data[0] | 0b01111111) == 0b11111111);
				ICS_States[2] = ((rx_data[0] | 0b10111111) == 0b11111111);
				ICS_States[3] = ((rx_data[0] | 0b11011111) == 0b11111111);
				break;
	}
}

bool FEB_Ready_To_Drive_Status() {
	return ICS_States[0];
}

bool FEB_Accum_Fan_Control(){
	return ICS_States[1];
}
bool FEB_Coolant_Pump_Control(){
	return ICS_States[2];
}

bool FEB_Extra_Control(){
	return ICS_States[3];
}
