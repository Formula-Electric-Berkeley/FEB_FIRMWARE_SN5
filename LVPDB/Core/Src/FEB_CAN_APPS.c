#include "FEB_CAN_APPS.h"

FEB_CAN_APPS_Message_t FEB_CAN_APPS_Message;

extern CAN_TxHeaderTypeDef FEB_CAN_Tx_Header;
extern uint8_t FEB_CAN_Tx_Data[8];
extern uint32_t APPS_CAN_ID;
extern uint32_t FEB_CAN_Tx_Mailbox;

uint8_t FEB_CAN_APPS_Filter(CAN_HandleTypeDef* hcan, uint8_t FIFO_assignment, uint8_t filter_bank) {
    // For multiple filters, create array of filter IDs and loop over IDs.
//
//	uint16_t ids[] = {FEB_CAN_ID_APPS_BRAKE_PEDAL};
//
//	for(uint8_t i = 0; i < 1; i ++){
//
//		CAN_FilterTypeDef filter_config;
//
//		// Standard CAN - 2.0A - 11 bit
//		filter_config.FilterActivation = CAN_FILTER_ENABLE;
//		filter_config.FilterBank = filter_bank;
//		filter_config.FilterFIFOAssignment = FIFO_assignment;
//		filter_config.FilterIdHigh = ids[i] << 5;
//		filter_config.FilterIdLow = 0;
//		filter_config.FilterMaskIdHigh = 0xFFE0;
//		filter_config.FilterMaskIdLow = 0;
//		filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
//		filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
//		filter_config.SlaveStartFilterBank = 14;
//		filter_bank++;
//
//
//		if (HAL_CAN_ConfigFilter(hcan, &filter_config) != HAL_OK) {
//			// Code Error - Shutdown
//		}
//	}

	CAN_FilterTypeDef filter_config;

	// Standard CAN - 2.0A - 11 bit
	filter_config.FilterActivation = CAN_FILTER_ENABLE;
	filter_config.FilterBank = filter_bank;
	filter_config.FilterFIFOAssignment = FIFO_assignment;
	filter_config.FilterIdHigh = 0;
	filter_config.FilterIdLow = 0;
	filter_config.FilterMaskIdHigh = 0;
	filter_config.FilterMaskIdLow = 0;
	filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
	filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
	filter_config.SlaveStartFilterBank = 27;
	filter_bank++;

	if (HAL_CAN_ConfigFilter(hcan, &filter_config) != HAL_OK) {
		// Code Error - Shutdown
	}

	return filter_bank;
}


void FEB_CAN_APPS_Str_Msg(CAN_RxHeaderTypeDef *FEB_CAN_Rx_Header, uint8_t FEB_CAN_Rx_Data[]) {

	switch(FEB_CAN_Rx_Header->StdId) {
	    case FEB_CAN_DASH_IO_FRAME_ID:
	    	//concatonate float from the array into temp   https://forum.arduino.cc/t/how-to-extract-float-data-from-longer-array/696454
//		    uint32_t data_brakepedal = FEB_CAN_Rx_Data[0];        // MSB
//		    data_brakepedal = (data_brakepedal << 8) | FEB_CAN_Rx_Data[1];
//		    data_brakepedal = (data_brakepedal << 8) | FEB_CAN_Rx_Data[2];
//		    data_brakepedal = (data_brakepedal << 8) | FEB_CAN_Rx_Data[3]; // LSB
//	    	memcpy(&(FEB_CAN_APPS_Message.brake_pedal), &(data_brakepedal), 4);
	    	FEB_CAN_APPS_Message.brake_pedal = FEB_CAN_Rx_Data[0];
	    	break;
	    //Probably don't need this section to be honest why do we even need this.
	    case 0x80:
	    	//concatonate float from the array into temp
			uint32_t data_current = FEB_CAN_Rx_Data[0];        // MSB
			data_current = (data_current << 8) | FEB_CAN_Rx_Data[1];
			data_current = (data_current << 8) | FEB_CAN_Rx_Data[2];
			data_current = (data_current << 8) | FEB_CAN_Rx_Data[3]; // LSB
			memcpy(&(FEB_CAN_APPS_Message.current), &(data_current), 4);
	    	break;
	}
}

