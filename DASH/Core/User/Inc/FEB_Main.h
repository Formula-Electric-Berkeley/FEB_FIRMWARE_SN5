#ifndef INC_FEB_MAIN_H_
#define INC_FEB_MAIN_H_

// **************************************** Includes ****************************************

#include <FEB_TPS2482.h>
#include <FEB_TPS2482.h>
#include "FEB_UI.h"
#include "FEB_UART_Transmit.h"
#include "FEB_CAN_ICS.h"
#include "FEB_CAN.h"
#include "FEB_IO_ICS.h"
#include "FEB_Radio.h"
#include "stm32f4xx_hal.h"
#include "FEB_CAN_Heartbeat.h"
#include "FEB_CAN_TPS.h"

// **************************************** Functions ****************************************

void FEB_Main_Setup(void);
void FEB_Main_Loop(void);

#endif /* INC_FEB_MAIN_H_ */
