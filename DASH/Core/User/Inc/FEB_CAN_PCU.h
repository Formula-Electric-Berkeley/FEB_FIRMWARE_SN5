#ifndef INC_FEB_CAN_PCU_H_
#define INC_FEB_CAN_PCU_H_

#include <stdint.h>
#include <string.h>
#include "stm32f4xx_hal.h"

typedef struct {
	uint8_t brake_pedal; // Should be some value between 0 and 100
	float current;
	uint8_t enabled;
} FEB_CAN_PCU_Message_t;

extern FEB_CAN_PCU_Message_t FEB_CAN_PCU_Message;

uint8_t FEB_CAN_PCU_Get_Enabled();
void FEB_CAN_PCU_Str_Msg(CAN_RxHeaderTypeDef *FEB_CAN_Rx_Header, uint8_t FEB_CAN_Rx_Data[]);
uint8_t FEB_CAN_PCU_Filter(CAN_HandleTypeDef* hcan, uint8_t FIFO_assignment, uint8_t filter_bank);
uint8_t FEB_CAN_PCU_Get_Brake_Pos();

#endif /* INC_FEB_CAN_PCU_H_ */
