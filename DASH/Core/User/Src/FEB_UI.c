// **************************************** Includes ****************************************

#include "FEB_UI.h"
#include "cmsis_os.h"
#include "main.h"
#include "portmacro.h"
#include "stm32469i_discovery.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32f4xx_hal_uart.h"
#include <stdio.h>

// **************************************** Variables ****************************************

Screen_Info_t screen_info;

extern UART_HandleTypeDef huart3;

// extern ICS_UI_Values_t ICS_UI_Values;
// extern uint16_t lv_voltage;

//const char* BMS_STATE_LABELS[] = {"PRECHARGE", "CHARGE", "BALANCE", "DRIVE", "SHUTDOWN", "NULL"};
//const int BMS_STATE_COLORS[] = {0xFAFF00, 0x33FF00, 0x00F0FF, 0xFA00FF, 0xFF0000, 0xC2C2C2};

// const char* HV_STATUS_LABELS[] = {"HV OFF", "HV ON"};
// const int HV_STATUS_COLORS[] = {0xFF0000, 0xFF8A00};
uint8_t iter;

#define NUM_SOC_POINTS 21

// typedef struct {
//     uint8_t soc;
//     float voltage;
// } SOC_Voltage_Lookup;

// SOC_Voltage_Lookup soc_voltage_table[NUM_SOC_POINTS] = {
//     {100, 4.10},
//     {95, 4.00},
//     {90, 3.97},
//     {85, 3.95},
//     {80, 3.92},
//     {75, 3.85},
//     {70, 3.81},
//     {65, 3.77},
//     {60, 3.73},
//     {55, 3.70},
//     {50, 3.64},
//     {45, 3.60},
//     {40, 3.55},
//     {35, 3.50},
//     {30, 3.45},
//     {25, 3.40},
//     {20, 3.34},
//     {15, 3.25},
//     {10, 3.1},
//     {5,  2.95},
//     {0,  2.80}
// };

// **************************************** Functions ****************************************

// static lv_obj_t *main_screen;
// static lv_obj_t *label_hello;

void FEB_UI_Init(void) {
    // Critical section only for initialization that truly requires it
    // LVGL operations don't need critical sections - they're designed to be thread-safe

    lv_init();              // LVGL core
    screen_driver_init();   // LCD + Framebuffer

    ui_init();
}

void FEB_UI_Update(void) {
    lv_timer_handler(); // (lv_task_handler is deprecated)
}

void StartDisplayTask(void *argument)
{
    FEB_UI_Init();
    uint32_t last_blink = 0;

    for (;;) {
        FEB_UI_Update();

        // LVGL timer handler should NOT be called in critical section
        // It needs interrupts enabled for DMA2D and LTDC to work properly
        if (lv_tick_get() - last_blink >= 1000) { // 1 second passed
            HAL_GPIO_TogglePin(LED4_GPIO_Port, LED4_Pin);
            last_blink = lv_tick_get();
        }

        osDelay(pdMS_TO_TICKS(5)); // VERY IMPORTANT (lets LVGL run properly)
    }
}


// void FEB_UI_Set_Values(void) {

// 	BMS_State_Set();

	
//     LV_Set_Value();
// 	SOC_Set_Value(ICS_UI_Values.pack_voltage, ICS_UI_Values.min_voltage);
// 	TEMP_Set_Value(ICS_UI_Values.max_acc_temp);
// 	SPEED_Set_Value(ICS_UI_Values.motor_speed);
// }

// // **************************************** Helper Functions ****************************************
// void BMS_State_Set(void) {
//     FEB_SM_ST_t bms_state = FEB_CAN_BMS_Get_State();
// 	//FEB_SM_ST_t bms_state = FEB_SM_ST_PRECHARGE; //If you want to actually see if you change the UI.

//     char* bms_str = get_bms_state_string(bms_state);
//     lv_label_set_text(ui_BMStateNumerical, bms_str);

//     lv_color_t bms_color;

//     switch (bms_state) {
//         case FEB_SM_ST_DRIVE:
//         case FEB_SM_ST_ENERGIZED:
//             bms_color = lv_color_hex(0x019F02); // green
//             break;
//         case FEB_SM_ST_PRECHARGE:
//         case FEB_SM_ST_CHARGING:
//             bms_color = lv_color_hex(0xFFFF00); // yellow
//             break;
//         case FEB_SM_ST_FAULT_BMS:
//         case FEB_SM_ST_FAULT_BSPD:
//         case FEB_SM_ST_FAULT_IMD:
//         case FEB_SM_ST_FAULT_CHARGING:
//             bms_color = lv_color_hex(0xFF0000); // red
//             break;
//         default:
//             bms_color = lv_color_hex(0xFFFFFF); // white/default
//             break;
//     }

//     lv_obj_set_style_text_color(ui_BMStateNumerical, bms_color, LV_PART_MAIN | LV_STATE_DEFAULT);
// }

// //Might need it later
// char* get_bms_state_string(FEB_SM_ST_t state) {
// 	switch (state) {
// 		case FEB_SM_ST_BOOT: return "Boot";
// 		case FEB_SM_ST_LV: return "LV Power";
// 		case FEB_SM_ST_HEALTH_CHECK: return "Health Check";
// 		case FEB_SM_ST_PRECHARGE: return "Precharge";
// 		case FEB_SM_ST_ENERGIZED: return "Energized";
// 		case FEB_SM_ST_DRIVE: return "Drive";
// 		case FEB_SM_ST_FREE: return "Battery Free";
// 		case FEB_SM_ST_CHARGER_PRECHARGE: return "Precharge";
// 		case FEB_SM_ST_CHARGING: return "Charging";
// 		case FEB_SM_ST_BALANCE: return "Balance";
// 		case FEB_SM_ST_FAULT_BMS: return "Fault - BMS";
// 		case FEB_SM_ST_FAULT_BSPD: return "Fault - BSPD";
// 		case FEB_SM_ST_FAULT_IMD: return "Fault - IMD";
// 		case FEB_SM_ST_FAULT_CHARGING: return "Fault - Charging";
// 		case FEB_SM_ST_DEFAULT: return "Default";
// 		default: return "Unknown";
// 	}
// }

// // Temp Gradient: Yellow (low) to Red (high)
// static lv_color_t get_temp_gradient_color(uint8_t value) {
//     uint8_t r = 255;
//     uint8_t g = 255 - (value * 255 / 100);  // fade green out
//     return lv_color_make(r, g, 0);
// }

// // SoC Gradient: Red (low) to Green (high)
// static lv_color_t get_soc_gradient_color(uint8_t value) {
//     uint8_t r = 255 - (value * 255 / 100);
//     uint8_t g = value * 255 / 100;
//     return lv_color_make(r, g, 0);
// }

// void SOC_Set_Value(float ivt_voltage, float min_cell_voltage) {
//     // Calculate SoC using your original logic
//     // Option 1: Based on ivt_voltage
//     //uint8_t soc_value = abs((100 - (((int)((ivt_voltage / 600.0) * 100)) % 600)) % 100);

//     // Option 2: Based on min cell voltage (commented out)
//     // uint8_t soc_value = (uint8_t)(((min_cell_voltage - 25) / 20.0) * 100.0);

// 	// Option 3: Linear (TERRIBLE) (avg_cell_voltage-cell_min) / (cell_max - cell_min)
// 	// float avg_cell_voltage = ivt_voltage / 140;
// 	// float soc_f = (avg_cell_voltage - 250) / (420 - 250) * 100;

// 	// if (soc_f > 100.0) soc_f = 100.0;
// 	// if (soc_f < 0.0) soc_f = 0.0;

//     // uint8_t soc_value = (uint8_t) (soc_f + 0.5f);

//     // Option 3: Lookup Table
//     float avg_cell_voltage = ICS_UI_Values.pack_voltage / 100.0f;
//     float min_voltage = 390.0f;
//     float max_voltage = 588.0f;
// ;
//     uint8_t soc_value = (avg_cell_voltage - min_voltage) / (max_voltage - min_voltage);
//     uint8_t soc_percent = (uint8_t)(soc_value * 100.0f + 0.5f);
// //    uint8_t soc_value = lookup_soc_from_voltage(avg_cell_voltage);

//     // Update UI
//     //char soc_label[10];
//     //sprintf(soc_label, "%d%%", soc_value);

//     char soc_label[32];
//     sprintf(soc_label, "%.1f V", avg_cell_voltage);

//     lv_label_set_text(ui_PackVoltageText, soc_label);
//     lv_bar_set_value(ui_PackNumerical, 100, LV_ANIM_OFF);
//     // lv_bar_set_value(ui_hvSoC, 100, LV_ANIM_OFF);
//     lv_obj_set_style_bg_color(ui_hvSoC, get_soc_gradient_color(soc_percent), LV_PART_INDICATOR | LV_STATE_DEFAULT);
// }

// void LV_Set_Value(void) {
//     // Calculate SoC using your original logic
//     // Option 1: Based on ivt_voltage
//     //uint8_t soc_value = abs((100 - (((int)((ivt_voltage / 600.0) * 100)) % 600)) % 100);

//     // Option 2: Based on min cell voltage (commented out)
//     // uint8_t soc_value = (uint8_t)(((min_cell_voltage - 25) / 20.0) * 100.0);

// 	// Option 3: Linear (TERRIBLE) (avg_cell_voltage-cell_min) / (cell_max - cell_min)
// 	// float avg_cell_voltage = ivt_voltage / 140;
// 	// float soc_f = (avg_cell_voltage - 250) / (420 - 250) * 100;

// 	// if (soc_f > 100.0) soc_f = 100.0;
// 	// if (soc_f < 0.0) soc_f = 0.0;

//     // uint8_t soc_value = (uint8_t) (soc_f + 0.5f);

//     // Option 3: Lookup Table
//     float avg_voltage = lv_voltage / 1000.0f;
//     float min_voltage = 21.0f;
//     float max_voltage = 27.0f;

//     float soc_value = (avg_voltage - min_voltage) / (max_voltage - min_voltage);
//     uint8_t soc_percent = (uint8_t)(soc_value * 100.0f + 0.5f);
    

// //    uint8_t soc_value = lookup_soc_from_voltage(avg_cell_voltage);

//     // Update UI
//     char soc_label[32];
//     sprintf(soc_label, "%.1f V", avg_voltage);
//     lv_label_set_text(ui_LVText1, soc_label);
//     lv_label_set_text(ui_LVNumerical, "");
//     lv_bar_set_value(ui_lvSoC, soc_percent, LV_ANIM_OFF);
//     lv_obj_set_style_bg_color(ui_lvSoC, get_soc_gradient_color(soc_percent), LV_PART_INDICATOR | LV_STATE_DEFAULT);
// }

// uint8_t lookup_soc_from_voltage(float voltage) {
//     for (int i = 0; i < NUM_SOC_POINTS - 1; i++) {
//         if (voltage <= soc_voltage_table[i].voltage && voltage > soc_voltage_table[i+1].voltage) {
//             float v1 = soc_voltage_table[i].voltage;
//             float v2 = soc_voltage_table[i+1].voltage;
//             uint8_t soc1 = soc_voltage_table[i].soc;
//             uint8_t soc2 = soc_voltage_table[i+1].soc;

//             float soc = soc1 + (voltage - v1) * (soc2 - soc1) / (v2 - v1);
//             return (uint8_t)(soc + 0.5f);
//         }
//     }

//     if (voltage > soc_voltage_table[0].voltage) {
//         return soc_voltage_table[0].soc;
//     } else {
//         return soc_voltage_table[NUM_SOC_POINTS-1].soc;
//     }
// }

// void TEMP_Set_Value(float max_acc_temp) {
//     // Clamp temperature to expected range
// 	float max_temp_C = max_acc_temp / 100.0f;
//     int max_temp = (int)(max_temp_C);
//     // if (max_temp % 60 < 30) max_temp = 30.0;

//     // Scale: 0–60°C and 0–100%
//     uint8_t temp_value = (uint8_t)(((fminf(max_temp, 60.0f)) / 60.0f) * 100.0f);

//     if (temp_value > 100) temp_value = 100;

//     // Update UI
//     char temp_label[10];
//     sprintf(temp_label, "%d°C", (int)max_temp);
//     lv_label_set_text(ui_tempNumerical, temp_label);

//     lv_bar_set_value(ui_BarTemp, temp_value, LV_ANIM_OFF);
//     lv_obj_set_style_bg_color(ui_BarTemp, get_temp_gradient_color(temp_value), LV_PART_INDICATOR | LV_STATE_DEFAULT);
// }

// void SPEED_Set_Value(float motor_speed_rpm) {
//     // Convert RPM to MPH using the provided formula
//     float mph = (((motor_speed_rpm / 3.545f) * 1.6358f) / 60.0f) * 2.237f;

//     // Clamp to 3-digit value for display
//     if (mph < 0) mph = 0;
//     if (mph > 999) mph = 999;

//     // Format and update label
//     char speed_str[10];
//     sprintf(speed_str, "%d", (int)(mph + 0.5f));  // Round to nearest int
//     lv_label_set_text(ui_speedNumerical, speed_str);
// }



