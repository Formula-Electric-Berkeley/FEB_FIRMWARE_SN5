/**
 ******************************************************************************
 * @file           : FEB_CAN_GPS.h
 * @brief          : CAN GPS Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_GPS_H
#define FEB_CAN_GPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Process GPS data transmissions
 *
 * Packs and transmits GPS data over CAN.
 */

void FEB_CAN_GPS_Tick(void);

#endif /* FEB_CAN_GPS_H */
