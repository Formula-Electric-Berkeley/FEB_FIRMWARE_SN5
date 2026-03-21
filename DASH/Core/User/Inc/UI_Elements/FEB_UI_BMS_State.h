/*
 * FEB_UI_BMS_State.h
 * Formula Electric Berkeley - DASH UI Helpers
 */

#ifndef FEB_UI_BMS_STATE_H
#define FEB_UI_BMS_STATE_H

#include "FEB_CAN_BMS.h"

// ── API ───────────────────────────────────────────────────────────────
void FEB_UI_Update_BMS_State(void);
void FEB_UI_Init_BMS_State(lv_obj_t *ui_Screen);
void FEB_UI_Destroy_BMS_State(void);
char *to_BMS_state_string(BMS_State_t state);

#endif /* FEB_UI_BMS_STATE_H */
