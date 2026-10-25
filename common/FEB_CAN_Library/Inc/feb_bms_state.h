/**
 * @file feb_bms_state.h
 * @brief Helpers for the BMS state machine states, shared by every board that reads bms_state.
 * @author Formula Electric @ Berkeley
 */

#ifndef FEB_BMS_STATE_H
#define FEB_BMS_STATE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_can_db.h"
#include <assert.h>
#include <stdbool.h>

#define BMS_STATE_NOMINAL_COUNT (BMS_STATE_BALANCE + 1)
#define BMS_STATE_FAULT_FIRST BMS_STATE_FAULT_CELL_OVERVOLTAGE

  static_assert(BMS_STATE_BOOT == 0 && BMS_STATE_NOMINAL_COUNT <= BMS_STATE_FAULT_FIRST,
                "bms_state: nominal states must precede the fault block");

  static inline bool BMS_State_Is_Fault(BMS_State_t state)
  {
    return state >= BMS_STATE_FAULT_FIRST;
  }

#ifdef __cplusplus
}
#endif

#endif /* FEB_BMS_STATE_H */
