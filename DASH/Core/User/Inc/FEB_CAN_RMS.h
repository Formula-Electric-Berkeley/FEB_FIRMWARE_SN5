/**
 ******************************************************************************
 * @file           : FEB_CAN_RMS.h
 * @brief          : CAN RMS Receiving Module
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_RMS_H
#define FEB_CAN_RMS_H

#include <stdint.h>

void FEB_CAN_RMS_Init(void);
int16_t FEB_CAN_RMS_GetLastTorque(void);
int8_t FEB_CAN_RMS_GetLastDirection(void);
int8_t FEB_CAN_RMS_GetLastEnabled(void);

#endif /* FEB_CAN_RMS_H */
