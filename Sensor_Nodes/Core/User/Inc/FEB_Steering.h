/**
 ******************************************************************************
 * @file           : FEB_Steering.h
 * @brief          : AS5600L magnetic steering encoder driver.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_STEERING_H
#define FEB_STEERING_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Globals — read by FEB_CAN_Steering.c
 * ============================================================================ */

extern uint16_t steer_angle;     /* 12-bit filtered angle   (0–4095) */
extern uint16_t steer_raw_angle; /* 12-bit raw angle        (0–4095) */
extern uint8_t steer_status;     /* magnet status bits [2:0]: MD|ML|MH */
extern uint8_t steer_agc;        /* AGC gain value          (0–255)  */
extern uint16_t steer_magnitude; /* 12-bit CORDIC magnitude (0–4095) */

/* ============================================================================
 * API
 * ============================================================================ */

bool FEB_Steering_Init(void);
void FEB_Steering_Read(void);

#endif /* FEB_STEERING_H */
