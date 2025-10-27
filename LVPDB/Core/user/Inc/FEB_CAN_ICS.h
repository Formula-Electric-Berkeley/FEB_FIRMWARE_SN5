#ifndef INC_FEB_CAN_ICS_H_
#define INC_FEB_CAN_ICS_H_

// **************************************** Includes ****************************************

#include <FEB_CAN_Library_SN4/gen/feb_can.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

// **************************************** Functions ****************************************


uint8_t FEB_CAN_ICS_Filter(CAN_HandleTypeDef* hcan, uint8_t FIFO_assignment, uint8_t filter_bank);

void FEB_CAN_ICS_Store_Msg(CAN_RxHeaderTypeDef *FEB_CAN_Rx_Header, uint8_t FEB_CAN_Rx_Data[]);

bool FEB_Ready_To_Drive_Status(); //Get the ready to drive status from ICS

bool FEB_Accum_Fan_Control(); //Get the accum fan status from ICs

bool FEB_Coolant_Pump_Control(); //Get the collant pump status from ICS

bool FEB_Extra_Control();

#endif /* INC_FEB_CAN_ICS_H_ */
