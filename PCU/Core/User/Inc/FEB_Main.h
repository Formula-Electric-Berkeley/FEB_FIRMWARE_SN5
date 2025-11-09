#include <stm32f4xx_hal.h>
#include <stdio.h>
#include "main.h"
#include "FEB_Printf_Redirect.h"
#include "FEB_CAN_TX.h"
#include "FEB_CAN_RX.h"
#include "FEB_ADC.h"
#include "FEB_RMS.h"
#include "FEB_CAN_RMS.h"
#include "FEB_CAN_Diagnostics.h"
#include "FEB_CAN_TPS.h"
#include "FEB_CAN_DASH.h"
#include "FEB_CAN_BMS.h"

void FEB_Main_Setup(void);
void FEB_Main_While(void);