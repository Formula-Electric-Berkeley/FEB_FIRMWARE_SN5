/**
 ******************************************************************************
 * @file           : DASH_CAN.h
 * @brief          : DASH CAN readiness and the BMS state enum
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Received values are read from feb::can::rx<M> at the point of use.
 */

#ifndef DASH_CAN_H
#define DASH_CAN_H

#include <stdbool.h>
#include <stdint.h>

/* BMS_State_t is generated from the DBC's bms_state value table and shared
 * with BMS and PCU (common/FEB_CAN_Library). */
#include "feb_bms_state.h"

void DASH_CAN_Init();

bool DASH_CAN_IsReady(void);

#endif /* DASH_CAN_H */
