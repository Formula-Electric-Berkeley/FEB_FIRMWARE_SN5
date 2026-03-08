/**
 ******************************************************************************
 * @file           : FEB_CAN_BMS.h
 * @brief          : CAN BMS Receiving Module
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_BMS_H
#define FEB_CAN_BMS_H

#include <stdint.h>

typedef enum
{
  BMS_STATE_BOOT = 0,
  BMS_STATE_LV_POWER,          // 1 - LV in SN4
  BMS_STATE_BUS_HEALTH_CHECK,  // 2 - HEALTH_CHECK in SN4
  BMS_STATE_PRECHARGE,         // 3
  BMS_STATE_ENERGIZED,         // 4
  BMS_STATE_DRIVE,             // 5
  BMS_STATE_BATTERY_FREE,      // 6 - FREE in SN4
  BMS_STATE_CHARGER_PRECHARGE, // 7
  BMS_STATE_CHARGING,          // 8
  BMS_STATE_BALANCE,           // 9
  BMS_STATE_FAULT_BMS,         // 10
  BMS_STATE_FAULT_BSPD,        // 11
  BMS_STATE_FAULT_IMD,         // 12
  BMS_STATE_FAULT_CHARGING,    // 13
  BMS_STATE_COUNT
} BMS_State_t;

void FEB_CAN_BMS_Init(void);
BMS_State_t FEB_CAN_BMS_GetLastState(void);
int16_t FEB_CAN_BMS_GetLastCellMaxTemperature(void);
uint16_t FEB_CAN_BMS_GetLastAccumulator_total_voltage(void);

#endif /* FEB_CAN_BMS_H */
