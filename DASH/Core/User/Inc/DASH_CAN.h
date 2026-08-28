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

typedef enum
{
  BMS_STATE_BOOT = 0,
  BMS_STATE_LV_POWER,
  BMS_STATE_BUS_HEALTH_CHECK,
  BMS_STATE_PRECHARGE,
  BMS_STATE_ENERGIZED,
  BMS_STATE_DRIVE,
  BMS_STATE_BATTERY_FREE,
  BMS_STATE_CHARGER_PRECHARGE,
  BMS_STATE_CHARGING,
  BMS_STATE_BALANCE,
  BMS_STATE_FAULT_BMS,
  BMS_STATE_FAULT_BSPD,
  BMS_STATE_FAULT_IMD,
  BMS_STATE_FAULT_CHARGING,
  BMS_STATE_COUNT
} BMS_State_t;

void DASH_CAN_Init();

bool DASH_CAN_IsReady(void);

#endif /* DASH_CAN_H */
