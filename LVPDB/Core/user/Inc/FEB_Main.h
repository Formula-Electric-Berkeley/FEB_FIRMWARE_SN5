#ifndef INC_FEB__MAIN_H_
#define INC_FEB__MAIN_H_

#include <FEB_CAN.h>
#include "FEB_CAN_APPS.h"
#include "FEB_CAN_ICS.h"
#include "FEB_CAN_Heartbeat.h"
#include "FEB_CAN_BMS.h"
#include <TPS2482.h>

#include <stm32f4xx_hal.h>
#include <stdint.h>
#include <stdio.h>

/*
 * The LVPDB has multiple TPS chips on the bus. These are the addresses of
 * each of the TPS chips. The naming conventions is as follows:
 * 		LV - Low voltage bus
 * 		CP - Coolant Pump
 * 		AF - Accumulator Fans
 * 		RF - Radiator Fans
 * 		SH - Shutdown Source
 * 		L - LIDAR
 * 		AS - Autonomous Steering
 * 		AB - Autonomous Braking
 */

#define NUM_TPS2482	8

#define LV_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND,TPS2482_I2C_ADDR_GND) // A1:GND 	A0:GND
#define CP_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_V_S,TPS2482_I2C_ADDR_V_S) // A1:Vs 	A0:Vs
#define AF_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_V_S,TPS2482_I2C_ADDR_GND) // A1:Vs 	A0:GND
#define RF_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND,TPS2482_I2C_ADDR_V_S) // A1:GND 	A0:Vs
#define SH_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_SDA,TPS2482_I2C_ADDR_SDA) // A1:SDA 	A0:SDA
#define L_ADDR 	TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_V_S,TPS2482_I2C_ADDR_SDA) // A1:Vs 	A0:SDA
#define AS_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_SDA,TPS2482_I2C_ADDR_GND) // A1:SDA	A0:GND
#define AB_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND,TPS2482_I2C_ADDR_SDA) // A1:GND	A0:SDA

// All TPS2482 implementation share the same WSR52L000FEA .002 ohm shunt resistor
#define R_SHUNT	(double)(.002) 	// Ohm

// Fuse max are the current ratings for the fuses, and are used as current maximums
// Fuse max can be exceeded safely in "peak current" scenarios so maybe not best metric
#define BATTERY_FUSE_MAX	(double)(30) 		// A from +24BAT -> +24GLV
#define LV_FUSE_MAX			(double)(5)			// A from +24GLV -> +24V
#define CP_FUSE_MAX			(double)(6.3)		// A
#define AF_FUSE_MAX			(double)(20) 		// A
#define RF_FUSE_MAX			(double)(3) 		// A
#define SH_FUSE_MAX			(double)(5) 		// A
#define L_FUSE_MAX			(double)(5) 		// A
#define AS_FUSE_MAX			(double)(20)		// A
#define AB_FUSE_MAX			(double)(5) 		// A

#define FLOAT_TO_UINT16_T(n)		((uint16_t)(n * 1000)) // for voltage (mV)
#define FLOAT_TO_INT16_T(n)			((int16_t)(n * 1000)) // for voltage (mV)
#define SIGN_MAGNITUDE(n)			(int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF)) // for current reg

// Todo figure out of fuse max is what we want to do as it gives resolution at the cost of range
#define LV_CURRENT_LSB	TPS2482_CURRENT_LSB_EQ(LV_FUSE_MAX)
#define CP_CURRENT_LSB	TPS2482_CURRENT_LSB_EQ(CP_FUSE_MAX)
#define AF_CURRENT_LSB	TPS2482_CURRENT_LSB_EQ(AF_FUSE_MAX)
#define RF_CURRENT_LSB	TPS2482_CURRENT_LSB_EQ(RF_FUSE_MAX)
#define SH_CURRENT_LSB	TPS2482_CURRENT_LSB_EQ(SH_FUSE_MAX)
#define L_CURRENT_LSB	TPS2482_CURRENT_LSB_EQ(L_FUSE_MAX)
#define AS_CURRENT_LSB	TPS2482_CURRENT_LSB_EQ(AS_FUSE_MAX)
#define AB_CURRENT_LSB	TPS2482_CURRENT_LSB_EQ(AB_FUSE_MAX)

#define LV_CAL_VAL	TPS2482_CAL_EQ(LV_CURRENT_LSB,R_SHUNT)
#define CP_CAL_VAL	TPS2482_CAL_EQ(CP_CURRENT_LSB,R_SHUNT)
#define AF_CAL_VAL	TPS2482_CAL_EQ(AF_CURRENT_LSB,R_SHUNT)
#define RF_CAL_VAL	TPS2482_CAL_EQ(RF_CURRENT_LSB,R_SHUNT)
#define SH_CAL_VAL	TPS2482_CAL_EQ(SH_CURRENT_LSB,R_SHUNT)
#define L_CAL_VAL	TPS2482_CAL_EQ(L_CURRENT_LSB,R_SHUNT)
#define AS_CAL_VAL	TPS2482_CAL_EQ(AS_CURRENT_LSB,R_SHUNT)
#define AB_CAL_VAL	TPS2482_CAL_EQ(AB_CURRENT_LSB,R_SHUNT)

#define LV_CAL_CORRECTED 	5838
#define CP_CAL_CORRECTED 	5838
#define AF_CAL_CORRECTED 	1459
#define RF_CAL_CORRECTED 	5838
#define SH_CAL_CORRECTED 	5838
#define L_CAL_CORRECTED 	5838
#define AS_CAL_CORRECTED 	5838
#define AB_CAL_CORRECTED 	5838

// This calculation is needed to have an alert go out when current exceeds fuse ratings
#define LV_ALERT_LIM_VAL	TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(LV_FUSE_MAX / LV_CURRENT_LSB),LV_CAL_VAL)
#define CP_ALERT_LIM_VAL	TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(CP_FUSE_MAX / CP_CURRENT_LSB),CP_CAL_VAL)
#define AF_ALERT_LIM_VAL	TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(AF_FUSE_MAX / AF_CURRENT_LSB),AF_CAL_VAL)
#define RF_ALERT_LIM_VAL	TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(RF_FUSE_MAX / RF_CURRENT_LSB),RF_CAL_VAL)
#define SH_ALERT_LIM_VAL	TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(SH_FUSE_MAX / SH_CURRENT_LSB),SH_CAL_VAL)
#define L_ALERT_LIM_VAL		TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(L_FUSE_MAX / L_CURRENT_LSB),L_CAL_VAL)
#define AS_ALERT_LIM_VAL	TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(AS_FUSE_MAX / AS_CURRENT_LSB),AS_CAL_VAL)
#define AB_ALERT_LIM_VAL	TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(AB_FUSE_MAX / AB_CURRENT_LSB),AB_CAL_VAL)

#define LV_POWER_LSB	TPS2482_POWER_LSB_EQ(LV_CURRENT_LSB)
#define CP_POWER_LSB	TPS2482_POWER_LSB_EQ(CP_CURRENT_LSB)
#define AF_POWER_LSB	TPS2482_POWER_LSB_EQ(AF_CURRENT_LSB)
#define RF_POWER_LSB	TPS2482_POWER_LSB_EQ(RF_CURRENT_LSB)
#define SH_POWER_LSB	TPS2482_POWER_LSB_EQ(SH_CURRENT_LSB)
#define L_POWER_LSB		TPS2482_POWER_LSB_EQ(L_CURRENT_LSB)
#define AS_POWER_LSB	TPS2482_POWER_LSB_EQ(AS_CURRENT_LSB)
#define AB_POWER_LSB	TPS2482_POWER_LSB_EQ(AB_CURRENT_LSB)

#define FEB_BREAK_THRESHOLD	(uint8_t)20

#define SLEEP_TIME 10

void FEB_Main_Setup(void);
void FEB_Main_Loop(void);
void FEB_1ms_Callback(void);
void FEB_CAN1_Rx_Callback(CAN_RxHeaderTypeDef *rx_header, void *data);

#endif
