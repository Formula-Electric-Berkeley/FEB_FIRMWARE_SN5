/**
 ******************************************************************************
 * @file           : FEB_CAN_TPS.h
 * @brief          : CAN TPS Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_TPS_H
#define FEB_CAN_TPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Process TPS data transmissions
 * @note Current values are sign-corrected by the TPS library
 */
void FEB_CAN_TPS_Tick(int16_t *tps_current_raw, uint16_t *tps_bus_voltage_raw, size_t length);

#endif /* FEB_CAN_TPS_H */
