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

    BMS_STATE_FAULT_CELL_OVERVOLTAGE = 20,     // 20
    BMS_STATE_FAULT_CELL_UNDERVOLTAGE,         // 21
    BMS_STATE_FAULT_CELL_OVERTEMP,             // 22
    BMS_STATE_FAULT_CELL_UNDERTEMP,            // 23
    BMS_STATE_FAULT_TEMP_SENSOR_LOSS,          // 24 - too few valid temp reads
    BMS_STATE_FAULT_ADBMS_INIT,                // 25 - cell monitor never produced a scan
    BMS_STATE_FAULT_ADBMS_TIMEOUT,             // 26 - cell monitor data stale
    BMS_STATE_FAULT_IVT_TIMEOUT,               // 27
    BMS_STATE_FAULT_OVERCURRENT,               // 28
    BMS_STATE_FAULT_IMD,                       // 29
    BMS_STATE_FAULT_BSPD,                      // 30
    BMS_STATE_FAULT_CONTACTOR_MISMATCH,        // 31 - AIR+/precharge sense != command
    BMS_STATE_FAULT_BALANCE_HV_ACTIVE,         // 32
    BMS_STATE_FAULT_PRECHARGE_TIMEOUT,         // 33
    BMS_STATE_FAULT_PRECHARGE_TOO_FAST,        // 34
    BMS_STATE_FAULT_CHARGER_PRECHARGE_TIMEOUT, // 35
    BMS_STATE_FAULT_SHUTDOWN_OPEN,             // 36 - shutdown loop opened while HV live
    BMS_STATE_FAULT_AIR_MINUS_OPEN,            // 37 - AIR- opened while HV live
    BMS_STATE_FAULT_CHARGER_HW,                // 38 - charger reported hardware failure
    BMS_STATE_FAULT_MANUAL,                    // 39 - latched from the console
    BMS_STATE_COUNT
  } BMS_State_t;

#define BMS_STATE_NOMINAL_COUNT (BMS_STATE_BALANCE + 1)
#define BMS_STATE_FAULT_FIRST BMS_STATE_FAULT_CELL_OVERVOLTAGE

  static inline bool BMS_State_Is_Fault(BMS_State_t state)
  {
    return state >= BMS_STATE_FAULT_FIRST && state < BMS_STATE_COUNT;
  }

  static inline bool BMS_State_Is_Valid(BMS_State_t state)
  {
    return state < BMS_STATE_NOMINAL_COUNT || BMS_State_Is_Fault(state);
  }

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
