/*
 * FEB_CAN_ID.h
 *
 *  Created on: Apr 29, 2025
 *      Author: root
 */

#ifndef INC_FEB_CAN_ID_H_
#define INC_FEB_CAN_ID_H_

// **************************************** Static CAN IDs ****************************************

// Charger
#define FEB_CAN_ID_CHARGER_BMS 0x1806e5f4
#define FEB_CAN_ID_CHARGER_CCS 0x18ff50e5

// IVT
#define FEB_CAN_ID_IVT_COMMAND 0x411
#define FEB_CAN_ID_IVT_DEBUG 0x510
#define FEB_CAN_ID_IVT_RESPONSE 0x511
#define FEB_CAN_ID_IVT_CURRENT 0x521
#define FEB_CAN_ID_IVT_VOLTAGE_1 0x522
#define FEB_CAN_ID_IVT_VOLTAGE_2 0x523
#define FEB_CAN_ID_IVT_VOLTAGE_3 0x524
#define FEB_CAN_ID_IVT_TEMPERATURE 0x525
#define FEB_CAN_ID_IVT_POWER 0x526
#define FEB_CAN_ID_IVT_CURRENT_COUNTER 0x527
#define FEB_CAN_ID_IVT_ENERGY_COUNTER 0x528

// RMS
#define FEB_CAN_ID_RMS_VOLTAGE 0xa7
#define FEB_CAN_ID_RMS_MOTOR 0xa5
#define FEB_CAN_ID_RMS_COMMAND 0xc0
#define FEB_CAN_ID_RMS_PARAMETER 0xc1

// PCU
#define FEB_CAN_ID_PCU_TPS 0x80

// **************************************** Dynamic CAN IDs ****************************************

// BMS
#define FEB_CAN_ID_BMS_TEMPERATURE 0x0
#define FEB_CAN_ID_BMS_ENABLED_TEMPERATURE_SENSORS 0x1
#define FEB_CAN_ID_BMS_VOLTAGE 0x2
#define FEB_CAN_ID_BMS_STATE 0x3
#define FEB_CAN_ID_BMS_BALANCE 0x4
#define FEB_CAN_ID_BMS_DART1_REQUESTED_FAN_SPEEDS 0x5
#define FEB_CAN_ID_BMS_DART2_REQUESTED_FAN_SPEEDS 0x6

// DASH
#define FEB_CAN_ID_DASH_BUTTON_STATE 0x7
#define FEB_CAN_ID_DASH_TEST 0x8

// PCU
#define FEB_CAN_ID_PCU_BRAKE_PEDAL 0x9
#define FEB_CAN_ID_PCU_BSPD 0xa

//DART
#define FEB_CAN_ID_DART_DART1_MEASURED_FAN_SPEEDS 0xb
#define FEB_CAN_ID_DART_DART2_MEASURED_FAN_SPEEDS 0xc

#endif /* INC_FEB_CAN_ID_H_ */
