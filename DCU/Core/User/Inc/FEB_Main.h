/**
 ******************************************************************************
 * @file           : FEB_Main.h
 * @brief          : DCU Application - Console and Communication Header
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
   * @brief Pre-kernel initialization - called from MX_FREERTOS_Init()
   */
  void FEB_Init(void);

  /**
   * @brief Main loop processing - called from StartDefaultTask()
   */
  void FEB_Main_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */
