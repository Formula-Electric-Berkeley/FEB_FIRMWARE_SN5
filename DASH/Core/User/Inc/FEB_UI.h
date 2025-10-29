#ifndef INC_FEB_UI_H_
#define INC_FEB_UI_H_

// **************************************** Includes ****************************************

//#include "FEB_CAN_ICS.h"
//#include "FEB_CAN_BMS.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "lvgl.h"
#include "screen_driver.h"
#include "touch_sensor_driver.h"
#include "ui.h"

#include "stm32f4xx_hal.h"

typedef struct {
	char * test_str;
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

#endif /* INC_FEB_UI_H_ */
