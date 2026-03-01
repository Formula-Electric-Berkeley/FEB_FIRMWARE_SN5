/**
 * @file FEB_CAN_State.h
 * @brief BMS CAN state publishing module
 */

#ifndef FEB_CAN_STATE_H
#define FEB_CAN_STATE_H

#include <stdint.h>

/**
 * @brief BMS state machine states (aligned with SN4)
 * @note Values match CAN bms_state signal (5-bit, 0-31 valid range)
 * @note Values must match FEB_SM_ST_t in PCU/Core/User/Inc/FEB_CAN_BMS.h
 */
typedef enum
{
  BMS_STATE_BOOT = 0,
  BMS_STATE_LV_POWER,          // 1 - LV in SN4
  BMS_STATE_BUS_HEALTH_CHECK,  // 2 - HEALTH_CHECK in SN4
  BMS_STATE_PRECHARGE,         // 3
  BMS_STATE_ENERGIZED,         // 4
  BMS_STATE_DRIVE,             // 5
  BMS_STATE_BATTERY_FREE,      // 6 - FREE in SN4
  BMS_STATE_CHARGER_PRECHARGE, // 7
  BMS_STATE_CHARGING,          // 8
  BMS_STATE_BALANCE,           // 9
  BMS_STATE_FAULT_BMS,         // 10
  BMS_STATE_FAULT_BSPD,        // 11
  BMS_STATE_FAULT_IMD,         // 12
  BMS_STATE_FAULT_CHARGING,    // 13
  BMS_STATE_COUNT
} BMS_State_t;

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
 * @note Call from 1ms timer callback
 *
 * Handles:
 * - ENERGIZED -> DRIVE when R2D active
 * - DRIVE -> ENERGIZED when R2D inactive/timeout
 */
void FEB_CAN_State_ProcessTransitions(void);

#endif /* FEB_CAN_STATE_H */
