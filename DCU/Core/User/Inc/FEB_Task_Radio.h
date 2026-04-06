/**
 * @file    FEB_Task_Radio.h
 * @brief   Radio Task Interface
 * @author  Formula Electric @ Berkeley
 */

#ifndef FEB_TASK_RADIO_H
#define FEB_TASK_RADIO_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Radio task entry point
   * @param argument Not used
   */
  void StartRadioTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_TASK_RADIO_H */
