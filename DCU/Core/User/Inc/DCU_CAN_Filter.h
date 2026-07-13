/**
 ******************************************************************************
 * @file           : DCU_CAN_Filter.h
 * @brief          : Per-frame radio-forwarding decision hook
 * @author         : Formula Electric @ Berkeley
 *
 * The CSV logger calls DCU_CAN_Filter_ShouldForwardToRadio() for every CAN
 * frame it sees. A `true` return tells the logger that frame should also be
 * placed on the radio transmit path. The actual radio TX wiring is added
 * separately; this header isolates the predicate so the policy can evolve
 * (ID allow-list, throttling, payload-dependent rules) without touching the
 * capture, queueing, or SD code.
 ******************************************************************************
 */

#ifndef DCU_CAN_FILTER_H
#define DCU_CAN_FILTER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>

  /* Forward-declared in DCU_CAN_Log.h; filter takes a const pointer only. */
  struct DCU_CAN_Frame;
  typedef struct DCU_CAN_Frame DCU_CAN_Frame_t;

  /**
   * @brief Decide whether a received CAN frame should be forwarded over radio.
   *
   * Called once per frame, in the canLogTask context (no ISR, no SD lock held).
   * Implementations must be fast and non-blocking — they run on every frame.
   *
   * @param frame Frame just captured from CAN1 or CAN2 (never NULL).
   * @return true to also enqueue this frame for radio TX, false to log only.
   */
  bool DCU_CAN_Filter_ShouldForwardToRadio(const DCU_CAN_Frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* DCU_CAN_FILTER_H */
