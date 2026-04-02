/*
 * FEB_UI_Helpers.h
 * Formula Electric Berkeley - DASH UI Helpers
 */

#ifndef FEB_UI_HELPERS_H
#define FEB_UI_HELPERS_H

#include "lvgl.h"

// ── Screen objects (extern for FEB_UI.c) ─────────────────────────────
extern lv_obj_t *ui_Screen1;

// ── API ───────────────────────────────────────────────────────────────
void ui_set_torque(int16_t torque);
void ui_init(void);
void ui_destroy(void);
void ui_update(void);

#endif /* FEB_UI_HELPERS_H */
