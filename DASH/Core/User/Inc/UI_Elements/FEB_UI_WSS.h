/*
 * FEB_UI_WSS.h
 * Formula Electric Berkeley - DASH UI Wheel Speed
 */

#ifndef FEB_UI_WSS_H
#define FEB_UI_WSS_H

#ifdef __cplusplus
extern "C"
{
#endif

  // ── API ───────────────────────────────────────────────────────────────
  void FEB_UI_Update_WSS();
  void FEB_UI_Init_WSS(lv_obj_t *ui_Screen);
  void FEB_UI_Destroy_WSS(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_UI_WSS_H */
