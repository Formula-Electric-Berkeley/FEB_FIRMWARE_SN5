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

int16_t cell_max_temperature = 67;
uint16_t accumulator_total_voltage = 67;
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
  lv_label_set_text(ui_BMS_State_String, to_BMS_state_string(state));
  lv_label_set_text(ui_BMS_HV_State_String, (hv_on(state) ? "HV_ON" : "HV_OFF"));

  cell_max_temperature = fc::rx<fm::BmsAccumulatorTemperature>.v().max_cell_temperature;
  accumulator_total_voltage = fc::rx<fm::BmsAccumulatorVoltage>.v().total_pack_voltage;
  low_voltage = fc::rx<fm::LvpdbLv24vBusAnd12vBusVoltages>.v().lv_24v_voltage;

  snprintf(buf, sizeof(buf), "%d.%d °C", cell_max_temperature / 10, cell_max_temperature % 10);
  lv_label_set_text(ui_BMS_Cell_Max_Temperature, buf);

  snprintf(buf, sizeof(buf), "%d.%d V", low_voltage / 1000, low_voltage / 100 % 10);
  lv_label_set_text(ui_LVPDB_low_voltage, buf);

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
