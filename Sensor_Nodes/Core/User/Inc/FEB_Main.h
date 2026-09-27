#ifndef FEB_MAIN_H
#define FEB_MAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* Console/log bring-up. Call from MX_FREERTOS_Init after the .ioc mutexes and queues exist. */
  void SN_Init(void);

#ifdef __cplusplus
}
#endif

#endif
