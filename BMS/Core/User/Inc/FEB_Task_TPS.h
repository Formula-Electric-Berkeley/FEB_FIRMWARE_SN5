/**
 ******************************************************************************
 * @file           : FEB_Task_TPS.h
 * @brief          : TPS2482 power monitoring task
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_TASK_TPS_H
#define FEB_TASK_TPS_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief TPS2482 task entry point
   * @param argument Not used
   * @note Registered with FreeRTOS as StartTPSTask
   */
  void StartTPSTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_TASK_TPS_H */
