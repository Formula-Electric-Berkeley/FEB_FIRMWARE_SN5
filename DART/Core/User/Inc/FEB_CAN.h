#ifndef INC_FEB_CAN_H_
#define INC_FEB_CAN_H_

// **************************************** Includes ****************************************

#include <string.h>
#include <FEB_CAN_BMS.h>
#include <FEB_CAN_Library_SN4/gen/feb_can.h>
#include <stm32f0xx_hal.h>
#include <main.h>

// **************************************** Function Prototypes ****************************************

void FEB_CAN_Init(void);
void FEB_CAN_Filter_Config(void);
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);
void FEB_CAN_Transmit(CAN_HandleTypeDef *hcan, uint8_t *can_data);

#endif /* INC_FEB_CAN_H_ */
