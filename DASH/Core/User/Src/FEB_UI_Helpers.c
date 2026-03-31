/*
 * FEB_UI_Helpers.c
 * Formula Electric Berkeley - DASH UI Helpers
 */

#include "FEB_UI_Helpers.h"
#include "FEB_CAN_PCU.h"
#include "FEB_RTD.h"
#include "UI_Elements/FEB_UI_BMS_State.h"
#include "lvgl.h"
#include "src/core/lv_obj_pos.h"
#include "src/core/lv_obj_style.h"
#include "src/draw/lv_draw_rect.h"
#include "src/misc/lv_area.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "UI_Elements/FEB_UI_Torque.h"
#include "UI_Elements/FEB_UI_IO_States.h"
#include "FEB_IO.h"

// ── UI objects ────────────────────────────────────────────────────────
lv_obj_t *ui_Screen1;

// ── ui_init ───────────────────────────────────────────────────────────
void ui_init(void)
{
  FEB_IO_Init();
  printf("initializing screen");
  FEB_IO_Set_Buzzer(false);

  // ── Screen ───────────────────────────────────────────────────────
  ui_Screen1 = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), 0); // near-black deep blue
  lv_obj_set_style_bg_opa(ui_Screen1, LV_OPA_COVER, 0);

  FEB_UI_Init_Torque(ui_Screen1);
  FEB_UI_Init_IO_States(ui_Screen1);
  FEB_UI_Init_BMS_State(ui_Screen1);

  // Load screen
  lv_disp_load_scr(ui_Screen1);
}

// ── ui_update ─────────────────────────────────────────────────────────
static int fake_torque = 0;
void ui_update(void)
{
  fake_torque += 1;

  FEB_UI_Update_Torque(FEB_CAN_PCU_GetLastTorque());

  FEB_IO_Update_GPIO();
  FEB_UI_Update_IO_States();

  FEB_UI_Update_BMS_State();

  FEB_State_Update_RTD();

  lv_timer_handler();
}

// ── ui_destroy ────────────────────────────────────────────────────────
void ui_destroy(void)
{
  if (ui_Screen1)
  {
    lv_obj_del(ui_Screen1);
    ui_Screen1 = NULL;

    FEB_UI_Destroy_Torque();
    FEB_UI_Destroy_IO_States();
    FEB_UI_Destroy_BMS_State();
  }
}
