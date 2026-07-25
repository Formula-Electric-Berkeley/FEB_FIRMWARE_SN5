/**
 ******************************************************************************
 * @file           : FEB_Main.h
 * @brief          : DASH Application Header
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

  /* FreeRTOS task entry points. These override __weak stubs in Core/Src/freertos.c
   * by symbol name alone, so their linkage MUST be pinned here: if FEB_Main.c is
   * ever compiled as C++ without these declarations, the names mangle, the weak
   * stubs win silently, and the tasks become do-nothing loops. */
  void StartUartRxTask(void *argument);
  void StartUartTxTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAIN_H */
