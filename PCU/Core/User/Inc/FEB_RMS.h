#include "FEB_CAN_RMS.h"
#include "FEB_CAN_BMS.h"
#include "FEB_ADC.h"

typedef struct
{
  int16_t torque;
  uint8_t enabled;
} RMS_CONTROL;

/* Global variables - defined in FEB_RMS.c */
extern RMS_CONTROL RMS_CONTROL_MESSAGE;
extern APPS_DataTypeDef APPS_Data;
extern Brake_DataTypeDef Brake_Data;

void FEB_RMS_Setup(void);
void FEB_RMS_Process(void);
void FEB_RMS_Disable(void);

float FEB_Get_Peak_Current_Delimiter(void);
float FEB_RMS_GetMaxTorque(void);
void FEB_RMS_Torque(void);