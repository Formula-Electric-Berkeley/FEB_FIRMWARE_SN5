/**
 ******************************************************************************
 * @file           : LVPDB_CAN.h
 * @brief          : LVPDB CAN bring-up
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Received values are read from feb::can::rx<M> at the point of use.
 */

#ifndef LVPDB_CAN_H
#define LVPDB_CAN_H

#include <stdbool.h>

/**
 * @brief Initialize the CAN library and every RX/ping-pong module
 */
void LVPDB_CAN_Init();

bool LVPDB_CAN_IsReady(void);

/**
 * @brief Changes the state of rails and brake light from the most recent received frames
 * @note Call after FEB_CAN_RX_Process() has drained the RX queue
 */
void LVPDB_CAN_ApplyRxState(void);

#endif /* LVPDB_CAN_H */
