/*
 * FEB_UI_Helpers.c
 * Formula Electric Berkeley - DASH UI Helpers
 */

#include "FEB_UI_Helpers.h"
#include "FEB_CAN_PCU.h"
#include "lvgl.h"
#include "src/core/lv_obj_pos.h"
#include "src/core/lv_obj_style.h"
#include "src/draw/lv_draw_rect.h"
#include "src/misc/lv_area.h"
#include <stdio.h>
#include <string.h>

#define UI_DOT_COUNT 21
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
#define MAX_TORQUE 300

// ── UI objects ────────────────────────────────────────────────────────
lv_obj_t *ui_Screen1;

static lv_obj_t *ui_Title;                       // "FORMULA ELECTRIC" header
static lv_obj_t *ui_Berkeley;                    // "BERKELEY" subheader
static lv_obj_t *ui_TorqueLabel;                 // "TORQUE" caption
static lv_obj_t *ui_TorqueValue;                 // live torque number
static lv_obj_t *ui_TorqueUnit;                  // "Nm" unit
static lv_obj_t *ui_Divider;                     // horizontal rule
static lv_obj_t *ui_TorqueCircles[UI_DOT_COUNT]; // torque dots

void ui_set_torque(int16_t torque)
{
  // char buf[16];
  // snprintf(buf, sizeof(buf), "%d", torque);
  // lv_label_set_text(ui_TorqueValue, buf);

  for (int i = 0; i < UI_DOT_COUNT; i++)
  {
    bool filled = i <= (150 * 21 / MAX_TORQUE);
    if (i < 12)
    {
      lv_obj_set_style_bg_color(ui_TorqueCircles[i], filled ? lv_color_hex(0x00FF00) : lv_color_hex(0x004D00),
                                0); // green
    }
    else if (i < 18)
    {
      lv_obj_set_style_bg_color(ui_TorqueCircles[i], filled ? lv_color_hex(0xFFDB00) : lv_color_hex(0x4D4900),
                                0); // yellow
    }
    else
    {
      lv_obj_set_style_bg_color(ui_TorqueCircles[i], filled ? lv_color_hex(0xFF0000) : lv_color_hex(0x4D0000),
                                0); // red
    }
  }
}

// ── ui_init ───────────────────────────────────────────────────────────
void ui_init(void)
{
  // ── Screen ───────────────────────────────────────────────────────
  ui_Screen1 = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(0x000000), 0); // near-black deep blue
  lv_obj_set_style_bg_opa(ui_Screen1, LV_OPA_COVER, 0);

  for (int i = 0; i < UI_DOT_COUNT; i++)
  {
    ui_TorqueCircles[i] = lv_obj_create(ui_Screen1);
    lv_obj_align(ui_TorqueCircles[i], LV_ALIGN_TOP_LEFT, (i * (SCREEN_WIDTH - 20)) / UI_DOT_COUNT + 20, 15);
    lv_obj_set_style_radius(ui_TorqueCircles[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(ui_TorqueCircles[i], 23, 23);
    lv_obj_set_style_border_width(ui_TorqueCircles[i], 0, 0);
    lv_obj_set_style_bg_color(ui_TorqueCircles[i], lv_color_hex(0x00FF00), 0);
  }

  // ── "FORMULA ELECTRIC" title ──────────────────────────────────────
  ui_Title = lv_label_create(ui_Screen1);
  lv_label_set_text(ui_Title, "FORMULA ELECTRIC");
  lv_obj_set_style_text_font(ui_Title, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(ui_Title, lv_color_hex(0xFDB515), 0); // Cal Gold
  lv_obj_align(ui_Title, LV_ALIGN_TOP_MID, 0, 20);

  // ── "BERKELEY" subheader ──────────────────────────────────────────
  ui_Berkeley = lv_label_create(ui_Screen1);
  lv_label_set_text(ui_Berkeley, "BERKELEY");
  lv_obj_set_style_text_font(ui_Berkeley, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(ui_Berkeley, lv_color_hex(0x4A90D9), 0); // electric blue
  lv_obj_align(ui_Berkeley, LV_ALIGN_TOP_MID, 0, 80);

  // ── Divider line ──────────────────────────────────────────────────
  ui_Divider = lv_obj_create(ui_Screen1);
  lv_obj_set_size(ui_Divider, 700, 2);
  lv_obj_set_style_bg_color(ui_Divider, lv_color_hex(0xFDB515), 0);
  lv_obj_set_style_border_width(ui_Divider, 0, 0);
  lv_obj_align(ui_Divider, LV_ALIGN_TOP_MID, 0, 120);

  // ── "TORQUE" caption ─────────────────────────────────────────────
  ui_TorqueLabel = lv_label_create(ui_Screen1);
  lv_label_set_text(ui_TorqueLabel, "TORQUE");
  lv_obj_set_style_text_font(ui_TorqueLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(ui_TorqueLabel, lv_color_hex(0xAAAAAA), 0); // grey
  lv_obj_align(ui_TorqueLabel, LV_ALIGN_CENTER, 0, -40);

  // ── Live torque value ─────────────────────────────────────────────
  ui_TorqueValue = lv_label_create(ui_Screen1);
  lv_label_set_text(ui_TorqueValue, "---");
  lv_obj_set_style_text_font(ui_TorqueValue, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(ui_TorqueValue, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(ui_TorqueValue, LV_ALIGN_CENTER, 0, 10);

  // ── "Nm" unit label ───────────────────────────────────────────────
  ui_TorqueUnit = lv_label_create(ui_Screen1);
  lv_label_set_text(ui_TorqueUnit, "Nm");
  lv_obj_set_style_text_font(ui_TorqueUnit, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(ui_TorqueUnit, lv_color_hex(0x4A90D9), 0);
  lv_obj_align(ui_TorqueUnit, LV_ALIGN_CENTER, 0, 60);

  // Load screen
  lv_disp_load_scr(ui_Screen1);
}

// ── ui_update ─────────────────────────────────────────────────────────
void ui_update(void)
{
  ui_set_torque(FEB_CAN_PCU_GetLastTorque());
  lv_timer_handler();
}

// ── ui_destroy ────────────────────────────────────────────────────────
void ui_destroy(void)
{
  if (ui_Screen1)
  {
    lv_obj_del(ui_Screen1);
    ui_Screen1 = NULL;
    ui_Title = NULL;
    ui_Berkeley = NULL;
    ui_TorqueLabel = NULL;
    ui_TorqueValue = NULL;
    ui_TorqueUnit = NULL;
    ui_Divider = NULL;

    for (int i = 0; i < UI_DOT_COUNT; i++)
    {
      ui_TorqueCircles[i] = NULL;
    }
  }
}
