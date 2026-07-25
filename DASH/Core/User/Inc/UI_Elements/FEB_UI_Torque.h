/*
 * FEB_UI_Torque.h
 * Formula Electric Berkeley - DASH UI Helpers
 */

#ifndef FEB_UI_TORQUE_H
#define FEB_UI_TORQUE_H

#ifdef __cplusplus
extern "C"
{
#endif

  // ── API ───────────────────────────────────────────────────────────────
  void FEB_UI_Update_Torque();
  void FEB_UI_Init_Torque(lv_obj_t *ui_Screen);
  void FEB_UI_Destroy_Torque(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_UI_TORQUE_H */
