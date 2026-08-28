#ifndef INC_FEB_UI_H_
#define INC_FEB_UI_H_

#ifdef __cplusplus
extern "C"
{
#endif

  // **************************************** Includes ****************************************

  // #include "FEB_CAN_ICS.h"
  // #include "DASH_CAN.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "lvgl.h"
#include "screen_driver.h"
#include "touch_sensor_driver.h"

#include "stm32f4xx_hal.h"

  typedef struct
  {
    char *test_str;
  } Screen_Info_t;

  // **************************************** Functions ****************************************

  void FEB_UI_Init(void);

  void FEB_UI_Update(void);

  void UI_Demo_Mode(void);

  void FEB_UI_Set_Values(void);

  void BMS_State_Set(void);

  // char* get_bms_state_string(FEB_SM_ST_t state);

  void SOC_Set_Value(float ivt_voltage, float min_cell_voltage);

  uint8_t lookup_soc_from_voltage(float voltage);

  void TEMP_Set_Value(float acc_temp);

  void SPEED_Set_Value(float motor_speed_rpm);

  void LV_Set_Value(void);

  /* FreeRTOS task entry points. These override __weak stubs in Core/Src/freertos.c
   * by symbol name alone, so their linkage MUST be pinned here: if FEB_UI.c is
   * ever compiled as C++ without these declarations, the names mangle, the weak
   * stubs win silently, and the tasks become do-nothing loops. */
  void StartDisplayTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* INC_FEB_UI_H_ */
