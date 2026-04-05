/**
 ******************************************************************************
 * @file           : FEB_CAN_IMU.h
 * @brief          : CAN IMU Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_IMU_H
#define FEB_CAN_IMU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Process IMU data transmissions
 * @note Current values are sign-corrected by the TPS library
 */

void FEB_CAN_IMU_Tick(void);

#endif /* FEB_CAN_IMU_H */
