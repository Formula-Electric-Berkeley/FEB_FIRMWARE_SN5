/**
 ******************************************************************************
 * @file           : FEB_CAN_PCU.h
 * @brief          : CAN PCU Receiving Module
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_PCU_H
#define FEB_CAN_PCU_H

#include <stdint.h>

void FEB_CAN_PCU_Init(void);
int16_t FEB_CAN_PCU_GetLastTorque(void);
int8_t FEB_CAN_PCU_GetLastDirection(void);
int8_t FEB_CAN_PCU_GetLastRMSEnabled(void);
uint16_t FEB_CAN_PCU_GetLastBreakPosition(void);

#endif /* FEB_CAN_PCU_H */
