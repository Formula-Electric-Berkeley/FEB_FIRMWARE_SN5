#ifndef INC_FEB_CAN_BMS_H_
#define INC_FEB_CAN_BMS_H_

// ******************************** Includes & External ********************************

#include <stm32f0xx_hal.h>
#include <stdint.h>
#include "FEB_CAN_Library_SN4/gen/feb_can.h"
#include "FEB_Fan.h"

// ******************************** Functions ********************************

uint8_t FEB_CAN_BMS_Filter(CAN_HandleTypeDef *hcan, uint8_t FIFO_assignment, uint8_t filter_bank);
void FEB_CAN_BMS_Process_Message(CAN_RxHeaderTypeDef *FEB_CAN_Rx_Header, uint8_t FEB_CAN_Rx_Data[]);

#endif /* INC_FEB_CAN_BMS_H_ */
