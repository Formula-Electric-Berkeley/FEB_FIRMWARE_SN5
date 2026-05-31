#include "FEB_CAN_RMS.h"
#include "FEB_CAN_BMS.h"
#include "FEB_ADC.h"
#include <stdbool.h>

typedef struct
{
  int16_t torque;
  uint8_t enabled;
} RMS_CONTROL;

/* Global variables - defined in FEB_RMS.c */
extern RMS_CONTROL RMS_CONTROL_MESSAGE;
extern APPS_DataTypeDef APPS_Data;
extern Brake_DataTypeDef Brake_Data;

void FEB_RMS_Process(void);
void FEB_RMS_Disable(void);

/* Runtime bench brake bypass (replaces the old PCU_BENCH_TEST compile flag).
 * SetBrakeBypass returns false if refused (a real BMS is active on the bus). */
bool FEB_RMS_SetBrakeBypass(bool enabled);
bool FEB_RMS_GetBrakeBypass(void);

/* Manual console inverter control. CommandEnable honors the drive gate and
 * returns whether the inverter actually enabled; CommandDisable is a sticky,
 * always-allowed fail-safe cleared only by CommandEnable. */
bool FEB_RMS_CommandEnable(void);
void FEB_RMS_CommandDisable(void);
bool FEB_RMS_IsForceDisabled(void);

float FEB_Get_Peak_Current_Delimiter(void);
float FEB_RMS_GetMaxTorque(void);
void FEB_RMS_Torque(void);
