#include <stm32f4xx_hal.h>
#include "FEB_Printf_Redirect.h"
#include "FEB_CAN_TX.h"
#include "FEB_CAN_RX.h"
#include "FEB_ADC.h"
#include "FEB_RMS.h"
#include "FEB_CAN_RMS.h"
#include "FEB_CAN_Diagnostics.h"
#include "FEB_CAN_TPS.h"

void FEB_Main_Setup(void);
void FEB_Main_While(void);
