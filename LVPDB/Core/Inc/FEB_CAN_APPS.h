#ifndef INC_FEB_CAN_APPS_H_
#define INC_FEB_CAN_APPS_H_

#include <stdint.h>
#include <string.h>


#include <FEB_CAN_Library_SN4/gen/feb_can.h>
#include "stm32f4xx_hal.h"



typedef struct {
	uint8_t brake_pedal; // Brake pedal stores value from 0 to 100
	float current;
} FEB_CAN_APPS_Message_t;

void FEB_CAN_APPS_Str_Msg(CAN_RxHeaderTypeDef *FEB_CAN_Rx_Header, uint8_t FEB_CAN_Rx_Data[]);
uint8_t FEB_CAN_APPS_Filter(CAN_HandleTypeDef* hcan, uint8_t FIFO_assignment, uint8_t filter_bank);


#endif /* INC_FEB_CAN_APPS_H_ */
