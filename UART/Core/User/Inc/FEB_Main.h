/**
 ******************************************************************************
 * @file           : FEB_Main.h
 * @brief          : UART Application Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_MAIN_H
#define FEB_MAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Application setup - called once at startup
   */
  void FEB_Main_Setup(void);

  /**
   * @brief Application main loop - called repeatedly
   */
  void FEB_Main_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */
