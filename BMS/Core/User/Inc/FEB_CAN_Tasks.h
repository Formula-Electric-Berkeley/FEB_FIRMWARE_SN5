/**
 * @file FEB_CAN_Tasks.h
 * @brief BMS CAN task entry points
 * @author Formula Electric @ Berkeley
 */

#ifndef FEB_CAN_TASKS_H
#define FEB_CAN_TASKS_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* FreeRTOS task entry points. These override __weak stubs in Core/Src/freertos.c
   * by symbol name alone, so their linkage MUST be pinned here: if FEB_CAN.c is
   * ever compiled as C++ without these declarations, the names mangle, the weak
   * stubs win silently, and the tasks become do-nothing loops. */
  void StartBMSTaskRx(void *argument);
  void StartBMSTaskTx(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_TASKS_H */
