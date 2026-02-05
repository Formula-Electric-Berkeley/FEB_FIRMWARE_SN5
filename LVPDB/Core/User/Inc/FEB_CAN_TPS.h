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
#include <stdint.h>

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize the ping/pong module
 * @note Must be called after FEB_CAN_Init()
 */
void FEB_CAN_PingPong_Init(void);

/**
 * @brief Process ping transmissions (call from timer, e.g., every 100ms)
 */
void FEB_CAN_TPS_Tick(uint16_t *tps_current_raw, uint16_t *tps_bus_voltage_raw, uint16_t *uint_shunt_voltage,
                      size_t length);

#endif /* FEB_CAN_TPS_H */
