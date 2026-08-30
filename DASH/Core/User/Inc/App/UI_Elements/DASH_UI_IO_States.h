/*
 * FEB_UI_IO_States.h
 * Formula Electric Berkeley - DASH UI Helpers
 */

#ifndef DASH_UI_IO_STATES_H
#define DASH_UI_IO_STATES_H

// ── API ───────────────────────────────────────────────────────────────
void FEB_UI_Update_IO_States(void);
void FEB_UI_Init_IO_States(lv_obj_t *ui_Screen);
void FEB_UI_Destroy_IO_States(void);
void helper_set_text_color(bool state_condition, int index);

#endif /* DASH_UI_IO_STATES_H */
