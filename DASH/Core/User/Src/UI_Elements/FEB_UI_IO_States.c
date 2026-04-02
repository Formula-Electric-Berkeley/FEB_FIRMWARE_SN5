#include "FEB_CAN_PCU.h"
#include "lvgl.h"
#include "src/core/lv_obj.h"
#include "src/core/lv_obj_style.h"
#include "src/draw/lv_draw_rect.h"
#include "src/misc/lv_area.h"
#include <math.h>
#include "UI_Elements/FEB_UI_IO_States.h"
#include "FEB_IO.h"

static lv_obj_t *ui_IO_States[4];

void FEB_UI_Update_IO_States()
{
  IO_Switch_States_t states = FEB_IO_GetLastIOStates();

  helper_set_text_color(states.switch_coolant_pump_radiator_fan, 0);
  helper_set_text_color(states.switch_accumulator_fans, 1);
  helper_set_text_color(states.switch_logging, 2);
  helper_set_text_color(states.button_rtd, 3);
}

// 0x00FF00 = Green, 0x565656 = Gray
void helper_set_text_color(bool state_condition, int index)
{
  if (state_condition)
  {
    lv_obj_set_style_text_color(ui_IO_States[index], lv_color_hex(0x00FF00), 0);
  }
  else
  {
    lv_obj_set_style_text_color(ui_IO_States[index], lv_color_hex(0x565656), 0);
  }
}

void FEB_UI_Init_IO_States(lv_obj_t *ui_Screen)
{
  for (int i = 0; i < 4; i++)
  {
    ui_IO_States[i] = lv_label_create(ui_Screen);
    lv_obj_align(ui_IO_States[i], LV_ALIGN_LEFT_MID, 15, (i - 1) * 45);
    lv_label_set_text(ui_IO_States[i], i == 0 ? "CP_RF" : i == 1 ? "ACC_FAN" : i == 2 ? "LOGGING" : "RTD");
    lv_obj_set_style_text_font(ui_IO_States[i], &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(ui_IO_States[i], lv_color_hex(0x565656), 0);
  }
}

void FEB_UI_Destroy_IO_States(void)
{
  for (int i = 0; i < 4; i++)
  {
    ui_IO_States[i] = NULL;
  }
}
