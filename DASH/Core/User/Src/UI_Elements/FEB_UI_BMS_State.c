#include "FEB_CAN_BMS.h"
#include "FEB_CAN_PCU.h"
#include "lvgl.h"
#include "src/core/lv_obj.h"
#include "src/core/lv_obj_style.h"
#include "src/draw/lv_draw_rect.h"
#include "src/misc/lv_area.h"
#include <math.h>
#include "UI_Elements/FEB_UI_BMS_State.h"
#include "FEB_IO.h"
#include <stdio.h>

static lv_obj_t *ui_BMS_State_String;
static lv_obj_t *ui_BMS_Cell_Max_Temperature;
static lv_obj_t *ui_BMS_Accumulator_Total_Voltage;

int16_t cell_max_temperature = 67;
uint16_t accumulator_total_voltage = 67;

char buf[16];

void FEB_UI_Update_BMS_State()
{
  BMS_State_t state = FEB_CAN_BMS_GetLastState();
  lv_label_set_text(ui_BMS_State_String, to_BMS_state_string(state));

  cell_max_temperature = FEB_CAN_BMS_GetLastCellMaxTemperature();
  accumulator_total_voltage = FEB_CAN_BMS_GetLastAccumulatorTotalVoltage();

  snprintf(buf, sizeof(buf), "%d.%d °C", cell_max_temperature / 10, cell_max_temperature % 10);
  lv_label_set_text(ui_BMS_Cell_Max_Temperature, buf);

  snprintf(buf, sizeof(buf), "%d.%d V", accumulator_total_voltage / 10, accumulator_total_voltage % 10);
  lv_label_set_text(ui_BMS_Accumulator_Total_Voltage, buf);
}

void FEB_UI_Init_BMS_State(lv_obj_t *ui_Screen)
{
  ui_BMS_State_String = lv_label_create(ui_Screen);
  lv_obj_align(ui_BMS_State_String, LV_ALIGN_BOTTOM_RIGHT, -15, -15);
  lv_label_set_text(ui_BMS_State_String, "---");
  lv_obj_set_style_text_font(ui_BMS_State_String, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(ui_BMS_State_String, lv_color_hex(0xFFFFFF), 0);

  ui_BMS_Cell_Max_Temperature = lv_label_create(ui_Screen);
  lv_obj_align(ui_BMS_Cell_Max_Temperature, LV_ALIGN_TOP_LEFT, 15, 60);
  lv_label_set_text(ui_BMS_Cell_Max_Temperature, "--.- °C");
  lv_obj_set_style_text_font(ui_BMS_Cell_Max_Temperature, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(ui_BMS_Cell_Max_Temperature, lv_color_hex(0xFFFFFF), 0);

  ui_BMS_Accumulator_Total_Voltage = lv_label_create(ui_Screen);
  lv_obj_align(ui_BMS_Accumulator_Total_Voltage, LV_ALIGN_TOP_RIGHT, -15, 60);
  lv_label_set_text(ui_BMS_Accumulator_Total_Voltage, "---.- V");
  lv_obj_set_style_text_font(ui_BMS_Accumulator_Total_Voltage, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(ui_BMS_Accumulator_Total_Voltage, lv_color_hex(0xFFFFFF), 0);
}

void FEB_UI_Destroy_BMS_State(void)
{
  ui_BMS_State_String = NULL;
  ui_BMS_Cell_Max_Temperature = NULL;
  ui_BMS_Accumulator_Total_Voltage = NULL;
}

char *to_BMS_state_string(BMS_State_t state)
{
  switch (state)
  {
  case (BMS_STATE_BOOT):
    return "BOOT";
  case (BMS_STATE_LV_POWER):
    return "LV_POWER"; // 1 - LV in SN4
  case (BMS_STATE_BUS_HEALTH_CHECK):
    return "HEALTH_CHECK"; // 2 - HEALTH_CHECK in SN4
  case (BMS_STATE_PRECHARGE):
    return "PRECHARGE"; // 3
  case (BMS_STATE_ENERGIZED):
    return "ENERGIZED"; // 4
  case (BMS_STATE_DRIVE):
    return "DRIVE"; // 5
  case (BMS_STATE_BATTERY_FREE):
    return "BATT_FREE"; // 6 - FREE in SN4
  case (BMS_STATE_CHARGER_PRECHARGE):
    return "CHARGER_PRECHARGE"; // 7
  case (BMS_STATE_CHARGING):
    return "CHARGING"; // 8
  case (BMS_STATE_BALANCE):
    return "BALANCE"; // 9
  case (BMS_STATE_FAULT_BMS):
    return "BMS_FAULT"; // 10
  case (BMS_STATE_FAULT_BSPD):
    return "BSPD_FAULT"; // 11
  case (BMS_STATE_FAULT_IMD):
    return "IMD_FAULT"; // Insulation fault between
  case (BMS_STATE_FAULT_CHARGING):
    return "CHARGING_FAULT"; // 13
  case (BMS_STATE_COUNT):
    return "COUNT";
  default:
    return "UNKNOWN";
  }
}
