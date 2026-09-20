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

  /**
   * @brief FreeRTOS task entry points. These override the weak stubs in the
   *        CubeMX-generated freertos.c, so they MUST have C linkage: a C++
   *        definition without it is silently name-mangled and the weak stub
   *        wins at link time.
   */
  void StartUartRxTask(void *argument);
  void StartSMTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */
