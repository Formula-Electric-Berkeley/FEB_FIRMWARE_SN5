#ifndef INC_FEB_RADIO_H_
#define INC_FEB_RADIO_H_

// **************************************** Includes ****************************************

#include "stm32f4xx_hal.h"

#include "FEB_CAN_ICS.h"

// **************************************** Functions ****************************************

void FEB_Radio_Setup(void);
void FEB_Radio_Transmit(void);

#endif /* INC_FEB_RADIO_H_ */
