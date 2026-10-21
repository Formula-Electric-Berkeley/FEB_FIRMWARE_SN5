/**
 * @file FEB_CAN_State.h
 * @brief BMS CAN state publishing module
 */

#ifndef FEB_CAN_STATE_H
#define FEB_CAN_STATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

/* BMS_State_t and its helpers are generated from the DBC's bms_state value
 * table and shared with DASH and PCU (common/FEB_CAN_Library). */
#include "feb_bms_state.h"

  /**
   * @brief Initialize the BMS CAN state publisher
   */
  void FEB_CAN_State_Init(void);

  /**
   * @brief Periodic tick for CAN state publishing
   * @note Call from 1ms timer callback (e.g., HAL_TIM_PeriodElapsedCallback)
   */
  void FEB_CAN_State_Tick(void);

  /**
   * @brief Signal that CAN is initialized and ready for transmission
   * @note Call from CAN RX task after BMS_CAN_Init() completes
   */
  void FEB_CAN_State_SetReady(void);

  /**
   * @brief Get current BMS state
   * @return Current state value
   */
  BMS_State_t FEB_CAN_State_GetState(void);

  /**
   * @brief Set BMS state
   * @param state New state value
   * @return 0 on success, -1 if state is invalid
   */
  int FEB_CAN_State_SetState(BMS_State_t state);

  /**
   * @brief Get state name as string
   * @param state State to get name for
   * @return String representation of state, or "UNKNOWN" if invalid
   */
  const char *FEB_CAN_State_GetStateName(BMS_State_t state);

  /**
   * @brief Process automatic state transitions based on R2D signal
   *
   * @warning DO NOT CALL. The ENERGIZED<->DRIVE R2D logic now lives in
   *          FEB_SM.c (EnergizedTransition / DriveTransition). Calling this would
   *          double-drive those transitions. Retained only to avoid touching the
   *          generated wiring; remove once confirmed unused everywhere.
   */
  void FEB_CAN_State_ProcessTransitions(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_STATE_H */
