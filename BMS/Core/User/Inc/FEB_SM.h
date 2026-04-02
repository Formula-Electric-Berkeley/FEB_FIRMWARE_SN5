/**
 * @file FEB_SM.h
 * @brief BMS State Machine Interface
 * @author Formula Electric @ Berkeley
 *
 * State machine for Battery Management System control.
 * Manages transitions between operational states and controls
 * high-voltage relays (AIR+, precharge) based on system conditions.
 *
 * States:
 * - BOOT: Initial startup
 * - LV_POWER: Low voltage power OK, waiting for HV enable
 * - BUS_HEALTH_CHECK: Verifying shutdown loop and accumulator health
 * - PRECHARGE: Precharge relay closed, monitoring bus voltage
 * - ENERGIZED: HV bus energized, waiting for R2D
 * - DRIVE: Vehicle in drive mode
 * - BATTERY_FREE: Accumulator disconnected
 * - CHARGER_PRECHARGE: Precharging for charging mode
 * - CHARGING: Active charging
 * - BALANCE: Cell balancing active
 * - FAULT_*: Various fault states
 */

#ifndef INC_FEB_SM_H_
#define INC_FEB_SM_H_

#include "FEB_CAN_State.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * State Machine Interface
 * ============================================================================ */

/**
 * @brief Initialize the state machine
 * @note Call from main before scheduler starts
 * - Sets initial state to BOOT
 * - Opens all relays (safe state)
 * - Closes BMS shutdown relay (enables HV path)
 */
void FEB_SM_Init(void);

/**
 * @brief Get current state machine state
 * @return Current BMS_State_t value
 */
BMS_State_t FEB_SM_Get_Current_State(void);

/**
 * @brief Request a state transition
 * @param next_state Target state to transition to
 * @note Calls the appropriate transition function for current state
 */
void FEB_SM_Transition(BMS_State_t next_state);

/**
 * @brief Process state machine periodic checks
 * @note Call from 1ms timer or periodic task
 * - Checks transition conditions for current state
 * - Handles automatic transitions (e.g., precharge complete)
 * - Monitors safety conditions (shutdown loop, AIR sense)
 */
void FEB_SM_Process(void);

/**
 * @brief Enter fault state with specified fault type
 * @param fault_type One of BMS_STATE_FAULT_BMS, BMS_STATE_FAULT_BSPD,
 *                   BMS_STATE_FAULT_IMD, or BMS_STATE_FAULT_CHARGING
 */
void FEB_SM_Fault(BMS_State_t fault_type);

/* ============================================================================
 * State Query Functions
 * ============================================================================ */

/**
 * @brief Check if system is in a fault state
 * @return true if current state is any FAULT_* state
 */
bool FEB_SM_Is_Faulted(void);

/**
 * @brief Check if HV bus is energized
 * @return true if in ENERGIZED, DRIVE, CHARGING, or BALANCE state
 */
bool FEB_SM_Is_HV_Active(void);

/**
 * @brief Check if drive mode is allowed
 * @return true if in ENERGIZED or DRIVE state
 */
bool FEB_SM_Is_Drive_Ready(void);

#endif /* INC_FEB_SM_H_ */
