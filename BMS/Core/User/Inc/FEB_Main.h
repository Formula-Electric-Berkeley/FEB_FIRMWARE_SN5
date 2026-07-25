/**
 ******************************************************************************
 * @file           : FEB_Main.h
 * @brief          : BMS Application Header
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
   * @brief Pre-kernel initialization - called from main() before osKernelStart()
   */
  void FEB_Init(void);

  void StartSMTask(void *argument);
  void StartUartRxTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */
