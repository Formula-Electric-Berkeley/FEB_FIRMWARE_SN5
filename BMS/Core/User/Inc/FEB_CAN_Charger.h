/**
 * @file FEB_CAN_Charger.h
 * @brief Charger CAN interface (ported from SN4 BMS)
 * @author Formula Electric @ Berkeley
 *
 * Charger: Elcon / HK "TC" family, part HK-J-H650-12 GEN3 (170-650 VDC).
 * DBC vendored at common/FEB_CAN_Library_SN4/elcon.dbc and fused into the
 * generated CAN library, so pack/unpack go through feb_can.h:
 *   - charger -> BMS  : Charger_Status (0x18FF50E5) operating V / I / status flags
 *   - BMS  -> charger : Charger_Limits (0x1806E5F4) max V / max I / control
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

/**
 * @brief Diagnostic snapshot of the charger link: the latest decoded
 *        Charger_Status (charger -> BMS) plus the command the BMS is currently
 *        sending (BMS -> charger). Consumed by the `BMS|charger` console
 *        command. Flag fields use the generated *_CHOICE encodings
 *        (0 = OK/CHARGING/START, 1 = FAIL/FAULT/OFF/TIMEOUT/STOP).
 */
typedef struct
{
  /* Charger -> BMS (latest Charger_Status). */
  bool present;    /**< a frame seen within FEB_CHARGER_RX_TIMEOUT_MS */
  bool ever_seen;  /**< at least one frame received since boot */
  uint32_t age_ms; /**< ms since last RX (0 if never) */
  uint32_t rx_count;
  uint16_t op_voltage_dV;
  uint16_t op_current_dA;
  uint8_t hw_status;           /**< 0 OK, 1 FAIL */
  uint8_t temperature;         /**< 0 OK, 1 FAULT */
  uint8_t input_voltage;       /**< 0 OK, 1 FAULT */
  uint8_t state;               /**< 0 CHARGING, 1 OFF */
  uint8_t communication_state; /**< 0 OK, 1 TIMEOUT */

  /* BMS -> charger (current command). */
  uint16_t cmd_voltage_dV;
  uint16_t cmd_current_dA;
  uint8_t control;     /**< 0 START, 1 STOP */
  bool trickle_active; /**< near-full trickle window engaged */
  bool trickle_on;     /**< current trickle on/off phase */
  bool done_charging;
} FEB_Charger_Snapshot_t;

/**
 * @brief Fill @p out with the latest charger telemetry + current command.
 */
void FEB_CAN_Charger_GetSnapshot(FEB_Charger_Snapshot_t *out);

#endif /* INC_FEB_CAN_CHARGER_H_ */
