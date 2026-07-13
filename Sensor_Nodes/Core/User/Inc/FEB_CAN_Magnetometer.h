/**
 ******************************************************************************
 * @file           : FEB_CAN_Magnetometer.h
 * @brief          : CAN Magnetometer Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_Magnetometer_H
#define FEB_CAN_Magnetometer_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Process Magnetometer data transmissions
 *
 * Packs and transmits Magnetometer data over CAN.
 */

void FEB_CAN_Magnetometer_Tick(void);

#endif /* FEB_CAN_Magnetometer_H */
