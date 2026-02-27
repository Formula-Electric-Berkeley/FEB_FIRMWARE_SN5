/**
 * @file FEB_CAN_State.h
 * @brief BMS CAN state publishing module
 */

#ifndef FEB_CAN_STATE_H
#define FEB_CAN_STATE_H

#include <stdint.h>

/**
 * @brief BMS state machine states
 * @note Values match CAN bms_state signal (5-bit, 0-31 valid range)
 */
typedef enum
{
  BMS_STATE_BOOT = 0,
  BMS_STATE_ORIGIN,
  BMS_STATE_LV_POWER,
  BMS_STATE_BUS_HEALTH_CHECK,
  BMS_STATE_PRECHARGE,
  BMS_STATE_ENERGIZED,
  BMS_STATE_DRIVE,
  BMS_STATE_FAULT,
  BMS_STATE_CHARGING,
  BMS_STATE_BATTERY_FREE,
  BMS_STATE_BALANCE,
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

#endif /* FEB_CAN_STATE_H */
