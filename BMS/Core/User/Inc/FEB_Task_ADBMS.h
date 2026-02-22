/**
 ******************************************************************************
 * @file           : FEB_Task_ADBMS.h
 * @brief          : ADBMS6830B monitoring task
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_TASK_ADBMS_H
#define FEB_TASK_ADBMS_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief ADBMS task entry point
   * @param argument Not used
   * @note Registered with FreeRTOS as StartADBMSTask
   */
  void StartADBMSTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_TASK_ADBMS_H */
