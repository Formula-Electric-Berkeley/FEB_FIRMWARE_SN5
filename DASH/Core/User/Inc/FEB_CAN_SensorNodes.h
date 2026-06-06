/**
 ******************************************************************************
 * @file           : FEB_CAN_SensorNodes.h
 * @brief          : CAN SensorNodes Receiving Module
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_SensorNodes_H
#define FEB_CAN_SensorNodes_H

#include <stdbool.h>
#include <stdint.h>

void FEB_CAN_SensorNodes_Init(void);
uint16_t FEB_CAN_SensorNodes_GetLastRearWheelSpeed(void);
bool FEB_CAN_SensorNodes_IsDataFresh(uint32_t timeout_ms);

#endif /* FEB_CAN_SensorNodes_H */
