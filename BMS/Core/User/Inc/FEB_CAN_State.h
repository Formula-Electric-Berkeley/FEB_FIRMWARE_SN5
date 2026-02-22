/**
 * @file FEB_CAN_State.h
 * @brief BMS CAN state publishing module
 */

#ifndef FEB_CAN_STATE_H
#define FEB_CAN_STATE_H

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

#endif /* FEB_CAN_STATE_H */
