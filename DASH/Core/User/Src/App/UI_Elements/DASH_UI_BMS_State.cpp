#include "DASH_CAN.h"
#include "feb_log.h"
#include "lvgl.h"
#include "src/core/lv_obj.h"
#include "src/core/lv_obj_style.h"
#include "src/draw/lv_draw_rect.h"
#include "src/misc/lv_area.h"
#include <math.h>
#include "DASH_UI_BMS_State.h"
#include "DASH_IO.h"
#include <stdio.h>
#include "feb_can_subscriber.hpp"

namespace fc = feb::can;
namespace fm = feb::can::msg;

static lv_obj_t *ui_BMS_State_String;
static lv_obj_t *ui_BMS_Cell_Max_Temperature;
static lv_obj_t *ui_BMS_Accumulator_Total_Voltage;
static lv_obj_t *ui_BMS_HV_State_String;
static lv_obj_t *ui_LVPDB_low_voltage;

uint16_t low_voltage = 67;

static char buf[16];

namespace
{
bool hv_on(BMS_State_t s)
{
  return s == BMS_STATE_DRIVE || s == BMS_STATE_ENERGIZED || s == BMS_STATE_PRECHARGE ||
         s == BMS_STATE_CHARGER_PRECHARGE || s == BMS_STATE_CHARGING || s == BMS_STATE_BALANCE;
}
} // namespace

void FEB_UI_Update_BMS_State()
{
  BMS_State_t state = static_cast<BMS_State_t>(fc::rx<fm::BmsState>.v().bms_state);
  lv_label_set_text(ui_BMS_State_String, BMS_State_Name(state));
  lv_label_set_text(ui_BMS_HV_State_String, (hv_on(state) ? "HV_ON" : "HV_OFF"));

  int16_t cell_max_temperature = fc::rx<fm::BmsAccumulatorTemperature>.v().max_cell_temperature;
  uint32_t accumulator_total_voltage = fc::rx<fm::BmsState>.v().total_pack_voltage;
  low_voltage = fc::rx<fm::LvpdbLv24vBusAnd12vBusVoltages>.v().lv_24v_voltage;

  if (cell_max_temperature == -4096)
    snprintf(buf, sizeof(buf), "--.- °C");
  else
    snprintf(buf, sizeof(buf), "%.1f °C", cell_max_temperature * 0.05f);
  lv_label_set_text(ui_BMS_Cell_Max_Temperature, buf);

  snprintf(buf, sizeof(buf), "%d.%d V", low_voltage / 1000, low_voltage / 100 % 10);
  lv_label_set_text(ui_LVPDB_low_voltage, buf);

  if (accumulator_total_voltage == 0x3FFFFF)
    snprintf(buf, sizeof(buf), "---.- V");
  else
    snprintf(buf, sizeof(buf), "%.1f V", accumulator_total_voltage * 0.00015f);
  lv_label_set_text(ui_BMS_Accumulator_Total_Voltage, buf);
}

void FEB_UI_Init_BMS_State(lv_obj_t *ui_Screen)
{
  ui_BMS_State_String = lv_label_create(ui_Screen);
  lv_obj_align(ui_BMS_State_String, LV_ALIGN_BOTTOM_RIGHT, -15, -15);
  lv_label_set_text(ui_BMS_State_String, "---");
  lv_obj_set_style_text_font(ui_BMS_State_String, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(ui_BMS_State_String, lv_color_hex(0xFFFFFF), 0);

  ui_BMS_HV_State_String = lv_label_create(ui_Screen);
  lv_obj_align(ui_BMS_HV_State_String, LV_ALIGN_BOTTOM_RIGHT, -15, -55);
  lv_label_set_text(ui_BMS_HV_State_String, "---");
  lv_obj_set_style_text_font(ui_BMS_HV_State_String, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(ui_BMS_HV_State_String, lv_color_hex(0xFFFFFF), 0);

  ui_BMS_Cell_Max_Temperature = lv_label_create(ui_Screen);
  lv_obj_align(ui_BMS_Cell_Max_Temperature, LV_ALIGN_TOP_LEFT, 15, 55);
  lv_label_set_text(ui_BMS_Cell_Max_Temperature, "--.- °C");
  lv_obj_set_style_text_font(ui_BMS_Cell_Max_Temperature, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(ui_BMS_Cell_Max_Temperature, lv_color_hex(0xFFFFFF), 0);

  ui_BMS_Accumulator_Total_Voltage = lv_label_create(ui_Screen);
  lv_obj_align(ui_BMS_Accumulator_Total_Voltage, LV_ALIGN_TOP_RIGHT, -15, 55);
  lv_label_set_text(ui_BMS_Accumulator_Total_Voltage, "---.- V");
  lv_obj_set_style_text_font(ui_BMS_Accumulator_Total_Voltage, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(ui_BMS_Accumulator_Total_Voltage, lv_color_hex(0xFFFFFF), 0);

  ui_LVPDB_low_voltage = lv_label_create(ui_Screen);
  lv_obj_align(ui_LVPDB_low_voltage, LV_ALIGN_TOP_RIGHT, -15, 95);
  lv_label_set_text(ui_LVPDB_low_voltage, "--.- V");
  lv_obj_set_style_text_font(ui_LVPDB_low_voltage, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(ui_LVPDB_low_voltage, lv_color_hex(0xFFFFFF), 0);
}

void FEB_UI_Destroy_BMS_State(void)
{
  ui_BMS_State_String = NULL;
  ui_BMS_HV_State_String = NULL;
  ui_BMS_Cell_Max_Temperature = NULL;
  ui_BMS_Accumulator_Total_Voltage = NULL;
  ui_LVPDB_low_voltage = NULL;
}
