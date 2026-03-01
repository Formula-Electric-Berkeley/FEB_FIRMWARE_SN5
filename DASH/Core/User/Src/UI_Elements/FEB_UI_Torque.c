#include "FEB_CAN_PCU.h"
#include "lvgl.h"
#include "src/core/lv_obj.h"
#include "src/core/lv_obj_style.h"
#include "src/draw/lv_draw_rect.h"
#include <math.h>
#include "UI_Elements/FEB_UI_Torque.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

#define UI_DOT_COUNT 21
#define MAX_MOTOR_TORQUE 3000  // Max motor: +300.0 Nm
#define MAX_REGEN_TORQUE -3000 // Max regen: -300.0 Nm

static lv_obj_t *ui_TorqueCircles[UI_DOT_COUNT];

static lv_style_t style_TorqueCircles;

void FEB_UI_Update_Torque(int16_t torque)
{
  // char buf[16];
  // snprintf(buf, sizeof(buf), "%d", torque);
  // lv_label_set_text(ui_TorqueValue, buf);

  if (torque < 0)
  {
    lv_style_set_radius(&style_TorqueCircles, 0);
  }
  else
  {
    lv_style_set_radius(&style_TorqueCircles, LV_RADIUS_CIRCLE);
  }

  for (int i = 0; i < UI_DOT_COUNT; i++)
  {
    bool filled = torque >= 0 ? (i <= (torque * 21 / MAX_MOTOR_TORQUE))                   // Positive torques
                              : i >= UI_DOT_COUNT - (torque * 21 / MAX_REGEN_TORQUE) - 1; // Negative torques

    lv_obj_set_style_bg_opa(ui_TorqueCircles[i],
                            filled         ? LV_OPA_MAX
                            : (torque < 0) ? LV_OPA_10
                                           : LV_OPA_30,
                            0); // Set dot opacities based on if they should be filled

    lv_obj_set_style_bg_color(ui_TorqueCircles[i],
                              (torque < 0) ? lv_color_hex(0xFFFFFF) // If torque < 0 all dots are white
                              : (i < 12)   ? lv_color_hex(0x00FF00) // First 12 dots are green
                              : (i < 18)   ? lv_color_hex(0xFFFF00) // Next 6 dots are yellow
                                           : lv_color_hex(0xFF0000),  // Last 3 dots are red
                              0);
  }
}

void FEB_UI_Init_Torque(lv_obj_t *ui_Screen)
{
  // Init torque circle style
  lv_style_init(&style_TorqueCircles);
  lv_style_set_border_width(&style_TorqueCircles, 0);
  lv_style_set_border_color(&style_TorqueCircles, lv_color_black());
  lv_style_set_radius(&style_TorqueCircles, LV_RADIUS_CIRCLE);

  for (int i = 0; i < UI_DOT_COUNT; i++)
  {
    ui_TorqueCircles[i] = lv_obj_create(ui_Screen);
    lv_obj_add_style(ui_TorqueCircles[i], &style_TorqueCircles, 0);
    lv_obj_align(ui_TorqueCircles[i], LV_ALIGN_TOP_LEFT, (i * (SCREEN_WIDTH - 20)) / UI_DOT_COUNT + 20, 15);
    lv_obj_set_size(ui_TorqueCircles[i], 23, 23);
    lv_obj_set_style_bg_color(ui_TorqueCircles[i], lv_color_hex(0x00FF00), 0);
  }
}

void FEB_UI_Destroy_Torque(void)
{
  for (int i = 0; i < UI_DOT_COUNT; i++)
  {
    ui_TorqueCircles[i] = NULL;
  }
}
