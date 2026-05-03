/**
 ******************************************************************************
 * @file           : FEB_CAN_WSS.h
 * @brief          : CAN WSS (Wheel Speed Sensor) Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_WSS_H
#define FEB_CAN_WSS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Process WSS data transmissions
 *
 * Packs and transmits WSS data over CAN.
 */

void FEB_CAN_WSS_Tick(void);

#endif /* FEB_CAN_WSS_H */
