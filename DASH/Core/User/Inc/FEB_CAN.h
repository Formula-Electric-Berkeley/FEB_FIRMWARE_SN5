#ifndef INC_FEB_CAN_H_
#define INC_FEB_CAN_H_

// **************************************** Includes ****************************************

#include "stm32f4xx_hal.h"

#include "FEB_CAN_DASH.h"
#include "FEB_CAN_PCU.h"
#include "FEB_CAN_BMS.h"

// **************************************** Functions ****************************************

void FEB_CAN_Init(void);
void FEB_CAN_Filter_Config(void);
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);

#endif /* INC_FEB_CAN_H_ */
