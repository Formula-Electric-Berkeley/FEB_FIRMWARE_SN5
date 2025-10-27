#ifndef INC_FEB_CAN_BMS_H_
#define INC_FEB_CAN_BMS_H_

// ********************************** Includes **********************************

#include <FEB_CAN_Library_SN4/gen/feb_can.h>
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdbool.h>
#include <FEB_CAN_Heartbeat.h>

// ********************************** Structs **********************************
typedef struct BMS_MESSAGE_TYPE {
    volatile uint16_t max_acc_temp;
} BMS_MESSAGE_TYPE;

extern BMS_MESSAGE_TYPE bms_message;

// ********************************** Functions **********************************

uint8_t FEB_CAN_BMS_Filter_Config(CAN_HandleTypeDef* hcan, uint8_t FIFO_assignment, uint8_t filter_bank);

#endif /* INC_FEB_CAN_BMS_H_ */
