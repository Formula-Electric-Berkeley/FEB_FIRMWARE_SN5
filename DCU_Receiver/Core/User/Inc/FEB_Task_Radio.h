/**
 * @file    FEB_Task_Radio.h
 * @brief   Radio Task Interface
 * @author  Formula Electric @ Berkeley
 */

#ifndef FEB_TASK_RADIO_H
#define FEB_TASK_RADIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Radio task entry point
   * @param argument Not used
   */
  void StartRadioTask(void *argument);

  /**
   * @brief Toggle listen-only mode. When enabled the radio task suspends its
   *        ping/pong role logic and only receives, logging packets as they
   *        arrive. Used by the dcu|radio|listen console command.
   */
  void FEB_Task_Radio_SetListenMode(bool enable);

  /**
   * @brief Query current listen-only mode state.
   */
  bool FEB_Task_Radio_GetListenMode(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_TASK_RADIO_H */
