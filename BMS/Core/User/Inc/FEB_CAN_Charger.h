/**
 * @file FEB_CAN_Charger.h
 * @brief Charger CAN interface (ported from SN4 BMS)
 * @author Formula Electric @ Berkeley
 *
 * Talks to the CCS charger over extended-ID CAN:
 *   - BMS  -> charger : 0x1806E5F4 (max voltage / max current / control)
 *   - charger -> BMS  : 0x18FF50E5 (operating voltage / current / status)
 *
 * Behaviour mirrors SN4 (FEB_CAN_Charger.c) but uses the SN5 feb_can_lib
 * RX/TX API (extended-ID aware, FreeRTOS-safe queues) instead of raw HAL.
 */

#ifndef INC_FEB_CAN_CHARGER_H_
#define INC_FEB_CAN_CHARGER_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Register charger CAN reception. Call from the CAN RX task before
 *        FEB_CAN_Filter_UpdateFromRegistry().
 */
void FEB_CAN_Charger_Init(void);

/**
 * @brief True if a charger frame was received within FEB_CHARGER_RX_TIMEOUT_MS.
 *        Used to gate BATTERY_FREE -> CHARGER_PRECHARGE (6->7).
 */
bool FEB_CAN_Charger_Received(void);

/**
 * @brief SN4 charge-decision function (uses existing cell V/T telemetry).
 * @return  1  charge complete / soft V or T limit hit -> stop, return to FREE
 *         -1  hard pack/cell over-voltage or over-temp -> FAULT_CHARGING
 *          0  keep charging
 */
int8_t FEB_CAN_Charging_Status(void);

/** @brief Command the charger to start (control byte = 0). */
void FEB_CAN_Charger_Start_Charge(void);

/** @brief Command the charger to stop (control byte = 1, marks done). */
void FEB_CAN_Charger_Stop_Charge(void);

/**
 * @brief Periodic charger command TX + trickle-charge logic.
 *        No-op unless the state machine is in CHARGING. Call ~every 100 ms.
 */
void FEB_CAN_Charger_Process(void);

#endif /* INC_FEB_CAN_CHARGER_H_ */
