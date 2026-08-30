#include "feb_log.h"
#include "lvgl.h"
#include "src/core/lv_obj.h"
#include "src/core/lv_obj_style.h"
#include "src/draw/lv_draw_rect.h"
#include "src/font/lv_font.h"
#include "src/misc/lv_area.h"
#include <math.h>
#include "DASH_UI_WSS.h"
#include "DASH_IO.h"
#include <stdio.h>
#include "feb_can_subscriber.hpp"

namespace fc = feb::can;
namespace fm = feb::can::msg;

static lv_obj_t *ui_Wheel_Speed_Text;

static uint16_t rear_speed_mph = 0;

static char buf[16];

void FEB_UI_Update_WSS()
{
  const auto &wss = fc::rx<fm::WssRearData>.v();
  rear_speed_mph = static_cast<uint16_t>((wss.wss_left_rear + wss.wss_right_rear) / 2);

  snprintf(buf, sizeof(buf), "%u", rear_speed_mph);
  lv_label_set_text(ui_Wheel_Speed_Text, buf);
}

void FEB_UI_Init_WSS(lv_obj_t *ui_Screen)
{
  ui_Wheel_Speed_Text = lv_label_create(ui_Screen);
  lv_obj_align(ui_Wheel_Speed_Text, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(ui_Wheel_Speed_Text, "--");
  lv_obj_set_style_text_font(ui_Wheel_Speed_Text, &lv_font_montserrat_digits_medium_164, 0);
  lv_obj_set_style_text_color(ui_Wheel_Speed_Text, lv_color_hex(0xFFFFFF), 0);
}

void FEB_UI_Destroy_WSS(void)
{
  ui_Wheel_Speed_Text = NULL;
}
