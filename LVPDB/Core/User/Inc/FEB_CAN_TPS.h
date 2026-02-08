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
#include <stddef.h>

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Process TPS data transmissions
 */
void FEB_CAN_TPS_Tick(uint16_t *tps_current_raw, uint16_t *tps_bus_voltage_raw, size_t length);

#endif /* FEB_CAN_TPS_H */
