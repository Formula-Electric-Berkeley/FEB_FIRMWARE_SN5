/**
 * @file FEB_CAN_IVT.h
 * @brief IVT (Isabellenhutte) Current/Voltage sensor CAN reception for the PCU
 * @author Formula Electric @ Berkeley
 *
 * The ISAscale IVT-MODULAR broadcasts on CAN1 (the vehicle bus the PCU is on):
 * - 0x521: Current
 * - 0x522: Voltage 1
 * - 0x523: Voltage 2 (pack voltage on SN5)
 * - 0x524: Voltage 3
 * - 0x525: Temperature
 *
 * Frame IDs and byte layout live in the shared CAN library
 * (common/FEB_CAN_Library_SN4); decoding uses the generated FEB_CAN_IVT_*_FRAME_ID
 * macros and feb_can_ivt_*_unpack() functions, like every other PCU CAN package.
 *
 * The PCU uses the IVT's measured pack voltage and current for RMS torque/power
 * limiting (see FEB_RMS.c).
 */

#ifndef INC_FEB_CAN_IVT_H_
#define INC_FEB_CAN_IVT_H_

#include <stdint.h>
#include <stdbool.h>

/* IVT voltage channel wired to the pack terminals (1, 2, or 3). SN5 uses 2. */
#define FEB_CAN_IVT_PACK_VOLTAGE_CHANNEL 2

/* Data is considered stale after this many ms without an IVT frame. */
#define FEB_CAN_IVT_DATA_TIMEOUT_MS 1000

/**
 * @brief Register IVT CAN RX callbacks on CAN1
 */
void FEB_CAN_IVT_Init(void);

/**
 * @brief Get pack voltage from the IVT (channel FEB_CAN_IVT_PACK_VOLTAGE_CHANNEL)
 * @return Pack voltage in volts; 0.0 if data is stale or never received
 */
float FEB_CAN_IVT_GetVoltage(void);

/**
 * @brief Get pack current from the IVT
 * @return Pack current in amps (may be stale — gate with FEB_CAN_IVT_IsDataFresh)
 */
float FEB_CAN_IVT_GetCurrent(void);

/**
 * @brief Check whether IVT data is fresh
 * @param timeout_ms Maximum acceptable age in milliseconds
 * @return true if a frame arrived within timeout_ms, false otherwise
 */
bool FEB_CAN_IVT_IsDataFresh(uint32_t timeout_ms);

#endif /* INC_FEB_CAN_IVT_H_ */
