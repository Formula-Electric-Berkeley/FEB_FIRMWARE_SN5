/**
 * @file FEB_CAN_Tasks.h
 * @brief DASH CAN task entry points
 * @author Formula Electric @ Berkeley
 *
 * NOTE: deliberately not named FEB_CAN.h. DASH's include path lists
 * common/FEB_CAN_Library_SN4/gen ahead of Core/User/Inc, and on a
 * case-insensitive filesystem (macOS) "FEB_CAN.h" resolves to that
 * directory's generated feb_can.h instead of the board's own header.
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
  void StartDASHTaskRx(void *argument);
  void StartDASHTaskTx(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_TASKS_H */
