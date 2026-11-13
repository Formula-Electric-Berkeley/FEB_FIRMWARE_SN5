/**
 ******************************************************************************
 * @file           : DASH_CAN.h
 * @brief          : DASH CAN readiness and the BMS state enum
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef PCU_CAN_H
#define PCU_CAN_H

#include <stdbool.h>
#include <stdint.h>

void PCU_CAN_Init();

bool PCU_CAN_IsReady(void);

#endif /* PCU_CAN_H */
