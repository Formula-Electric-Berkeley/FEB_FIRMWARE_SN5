#ifndef INC_FEB_MAIN_H_
#define INC_FEB_MAIN_H_

#include <stm32f4xx_hal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Common library includes
#include "feb_can_lib.h"
#include "feb_console.h"
#include "feb_uart.h"
#include "feb_uart_log.h"

// PCU-specific includes
#include "FEB_ADC.h"
#include "FEB_RMS.h"
#include "FEB_CAN_RMS.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_Diagnostics.h"
#include "FEB_CAN_TPS.h"
#include "TPS2482.h"
#include "FEB_PCU_Commands.h"

/* Main loop functions */
void FEB_Main_Setup(void);
void FEB_Main_Loop(void);
void FEB_1ms_Callback(void);

#endif /* INC_FEB_MAIN_H_ */
