/**
 * @file    FEB_Task_Radio.h
 * @brief   Radio Task Interface
 * @author  Formula Electric @ Berkeley
 */

#ifndef FEB_TASK_RADIO_H
#define FEB_TASK_RADIO_H

#include <stdbool.h>
#include <stdint.h>

/* Full struct lives in DCU_CAN_Log.h; forward-declared here so this header does
 * not drag the logger into every radio consumer. */
struct DCU_CAN_Frame;

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

  /**
   * @brief Enable/disable CAN-over-radio streaming (the TX forwarding mode).
   *        When enabled, frames passed to FEB_Task_Radio_ForwardCanFrame() are
   *        batched and transmitted. Used by the dcu|radio|stream console command.
   */
  void FEB_Task_Radio_SetStreamMode(bool enable);

  /** @brief Query current CAN-over-radio streaming state. */
  bool FEB_Task_Radio_GetStreamMode(void);

  /**
   * @brief Queue one captured CAN frame for radio transmission.
   *
   * Called from the CAN logger task for frames the filter accepted. No-op when
   * stream mode is off or the radio task has not created its queue yet. Safe to
   * call from task context (non-blocking, drop-oldest on a full queue).
   *
   * @param frame Frame to forward (never NULL). Copied into the queue.
   */
  void FEB_Task_Radio_ForwardCanFrame(const struct DCU_CAN_Frame *frame);

  /** @return Number of frames dropped from the radio forward queue (queue-full). */
  uint32_t FEB_Task_Radio_GetForwardDropCount(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_TASK_RADIO_H */
