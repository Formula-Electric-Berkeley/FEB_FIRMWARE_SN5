/*
 * FEB_CAN_Heartbeat.h
 *
 *  Created on: Apr 13, 2025
 *      Author: samnesh
 */

#ifndef INC_FEB_CAN_HEARTBEAT_H_
#define INC_FEB_CAN_HEARTBEAT_H_

#include <string.h>
#include "FEB_CAN_Library_SN4/gen/feb_can.h"
#include "FEB_Main.h"
#include "FEB_CAN.h"
#include "main.h"

// uint8_t heartbeat[8];
void FEB_CAN_HEARTBEAT_Transmit();
void FEB_CAN_HEARTBEAT_Init();
 
typedef enum {
	FEB_HB_NULL,
	FEB_HB_DASH,
	FEB_HB_PCU,
	FEB_HB_LVPDB,
	FEB_HB_DCU,
	FEB_HB_FSN,
	FEB_HB_RSN
} FEB_HB_t;

typedef enum {
	FEB_SM_ST_BOOT,
	FEB_SM_ST_LV,
	FEB_SM_ST_HEALTH_CHECK,
	FEB_SM_ST_PRECHARGE,
	FEB_SM_ST_ENERGIZED,
	FEB_SM_ST_DRIVE,
	FEB_SM_ST_FREE,
	FEB_SM_ST_CHARGER_PRECHARGE,
	FEB_SM_ST_CHARGING,
	FEB_SM_ST_BALANCE,
	FEB_SM_ST_FAULT_BMS,
	FEB_SM_ST_FAULT_BSPD,
	FEB_SM_ST_FAULT_IMD,
	FEB_SM_ST_FAULT_CHARGING,
	FEB_SM_ST_DEFAULT
} FEB_SM_ST_t;

 #endif /* INC_FEB_CAN_HEARTBEAT_H_ */
 
