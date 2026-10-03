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

  BMS_STATE_FAULT_CELL_OVERVOLTAGE = 20,     // 20
  BMS_STATE_FAULT_CELL_UNDERVOLTAGE,         // 21
  BMS_STATE_FAULT_CELL_OVERTEMP,             // 22
  BMS_STATE_FAULT_CELL_UNDERTEMP,            // 23
  BMS_STATE_FAULT_TEMP_SENSOR_LOSS,          // 24
  BMS_STATE_FAULT_ADBMS_INIT,                // 25
  BMS_STATE_FAULT_ADBMS_TIMEOUT,             // 26
  BMS_STATE_FAULT_IVT_TIMEOUT,               // 27
  BMS_STATE_FAULT_OVERCURRENT,               // 28
  BMS_STATE_FAULT_IMD,                       // 29
  BMS_STATE_FAULT_BSPD,                      // 30
  BMS_STATE_FAULT_CONTACTOR_MISMATCH,        // 31
  BMS_STATE_FAULT_BALANCE_HV_ACTIVE,         // 32
  BMS_STATE_FAULT_PRECHARGE_TIMEOUT,         // 33
  BMS_STATE_FAULT_PRECHARGE_TOO_FAST,        // 34
  BMS_STATE_FAULT_CHARGER_PRECHARGE_TIMEOUT, // 35
  BMS_STATE_FAULT_SHUTDOWN_OPEN,             // 36
  BMS_STATE_FAULT_AIR_MINUS_OPEN,            // 37
  BMS_STATE_FAULT_CHARGER_HW,                // 38
  BMS_STATE_FAULT_MANUAL,                    // 39
  BMS_STATE_COUNT
} BMS_State_t;

void DASH_CAN_Init();

bool DASH_CAN_IsReady(void);

#endif /* DASH_CAN_H */
