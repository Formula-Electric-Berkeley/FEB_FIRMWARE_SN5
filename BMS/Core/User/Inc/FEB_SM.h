// #ifndef INC_FEB_SM_H_
// #define INC_FEB_SM_H_

// // ********************************** State Machine Definitions ******************
// // Stub file for BMS state machine definitions
// // Based on the state diagram provided, this defines the BMS operational states

// #include <stdint.h>
// #include <stdbool.h>

// typedef enum {
//     BMS_STATE_BOOT = 0,
//     BMS_STATE_ORIGIN,
//     BMS_STATE_LV_POWER,
//     BMS_STATE_BUS_HEALTH_CHECK,
//     BMS_STATE_PRECHARGE,
//     BMS_STATE_ENERGIZED,
//     BMS_STATE_DRIVE,
//     BMS_STATE_FAULT,
//     // Charger states
//     BMS_STATE_CHARGING,
//     BMS_STATE_BATTERY_FREE,
//     BMS_STATE_BALANCE
// } bms_state_t;

// // Fault state types
// typedef enum {
//     BMS_FAULT_NONE = 0,
//     BMS_FAULT_BMS = 9,
//     BMS_FAULT_BSPD = 10,
//     BMS_FAULT_IMD = 11,
//     BMS_FAULT_BMS_CHARGING = 12
// } bms_fault_t;

// // Additional state definitions for ADBMS compatibility
// typedef enum {
//     FEB_SM_ST_INIT = BMS_STATE_BOOT,
//     FEB_SM_ST_IDLE = BMS_STATE_ORIGIN,
//     FEB_SM_ST_PRECHARGE = BMS_STATE_PRECHARGE,
//     FEB_SM_ST_ACTIVE = BMS_STATE_ENERGIZED,
//     FEB_SM_ST_BALANCING = BMS_STATE_BALANCE,
//     FEB_SM_ST_CHARGING = BMS_STATE_CHARGING,
//     FEB_SM_ST_FAULT_BMS = BMS_STATE_FAULT
// } FEB_SM_State_t;

// // Transition result codes
// typedef enum {
//     FEB_SM_TRANS_OK = 0,
//     FEB_SM_TRANS_INVALID,
//     FEB_SM_TRANS_BLOCKED,
//     FEB_SM_TRANS_ERROR
// } FEB_SM_TransResult_t;

// // Global state variables (extern - define in implementation file)
// extern bms_state_t current_bms_state;
// extern bms_fault_t current_bms_fault;

// /**
//  * @brief Initialize the state machine
//  * @note Should be called before scheduler starts
//  */
// void FEB_SM_Init(void);

// /**
//  * @brief Attempt to transition to a new state
//  * @param new_state The target state to transition to
//  * @return Transition result code
//  * @note STUB: Always returns success for now
//  */
// FEB_SM_TransResult_t FEB_SM_Transition(FEB_SM_State_t new_state);

// /**
//  * @brief Get the current state of the state machine
//  * @return Current state
//  */
// FEB_SM_State_t FEB_SM_Get_Current_State(void);

// /**
//  * @brief Get state name as string (for debugging)
//  * @param state The state to get the name of
//  * @return String representation of the state
//  */
// const char* FEB_SM_Get_State_Name(FEB_SM_State_t state);

// #endif /* INC_FEB_SM_H_ */
