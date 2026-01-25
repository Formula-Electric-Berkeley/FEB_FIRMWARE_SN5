#ifndef INC_FEB__MAIN_H_
#define INC_FEB__MAIN_H_

#include <FEB_CAN.h>
#include "FEB_CAN_Heartbeat.h"
#include <TPS2482.h>

#include <stm32f4xx_hal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * The LVPDB has multiple TPS chips on the bus. These are the addresses of
 * each of the TPS chips. The naming conventions is as follows:
 *		LV - Low Voltage Source (scl-sda)
 *		SH - Shutdown Source (sda-sda)
 *		LT - Laptop Branch (gnd-gnd)
 *		BM_L - Braking Servo, Lidar (gnd-scl) 
 *		SM - Steering Motor (gnd-sda) 
 *		AF1_AF2 - Accumulator Fans 1 Branch (gnd-vs)
 *		CP_RF - Coolant Pump + Radiator Fans Branch (vs-scl)
 */

#define NUM_TPS2482	7
#define NUM_TPS2482	7

#define LV_ADDR         TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_SCL, TPS2482_I2C_ADDR_SDA) // A1:SCL 	A0:SDA
#define SH_ADDR         TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_SDA, TPS2482_I2C_ADDR_SDA) // A1:SDA 	A0:SDA
#define LT_ADDR         TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_GND) // A1:GND 	A0:GND
#define BM_L_ADDR         TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_SCL) // A1:GND 	A0:SCL
#define SM_ADDR         TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_SDA) // A1:GND 	A0:SDA
#define AF1_AF2_ADDR    TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_V_S) // A1:GND 	A0:VS
#define CP_RF_ADDR      TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_V_S, TPS2482_I2C_ADDR_SCL) // A1:VS 	A0:SCL

// All TPS2482 implementation share the same WSR52L000FEA .002 ohm shunt resistor
#define R_SHUNT	(double)(.002) 	// Ohm

// Fuse max are the current ratings for the fuses, and are used as current maximums
// Fuse max can be exceeded safely in "peak current" scenarios so maybe not best metric
#define BATTERY_FUSE_MAX	    (double)(30) 		// A from +24BAT -> +24GLV
#define LV_FUSE_MAX				(double)(5)			// A from +24GLV -> +24V
#define SH_FUSE_MAX				(double)(5)
#define LT_FUSE_MAX				(double)(6.3)
#define BM_L_FUSE_MAX				(double)(16)
#define SM_FUSE_MAX				(double)(12)
#define AF1_AF2_FUSE_MAX	    (double)(20)
#define CP_RF_FUSE_MAX		    (double)(10)

#define FLOAT_TO_UINT16_T(n)		((uint16_t)(n * 1000)) // for voltage (mV)
#define FLOAT_TO_INT16_T(n)			((int16_t)(n * 1000)) // for voltage (mV)
#define SIGN_MAGNITUDE(n)			(int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF)) // for current reg

// Todo figure out of fuse max is what we want to do as it gives resolution at the cost of range
#define LV_CURRENT_LSB				TPS2482_CURRENT_LSB_EQ(LV_FUSE_MAX)
#define SH_CURRENT_LSB				TPS2482_CURRENT_LSB_EQ(SH_FUSE_MAX)
#define LT_CURRENT_LSB				TPS2482_CURRENT_LSB_EQ(LT_FUSE_MAX)
#define BM_L_CURRENT_LSB				TPS2482_CURRENT_LSB_EQ(BM_L_FUSE_MAX)
#define SM_CURRENT_LSB				TPS2482_CURRENT_LSB_EQ(SM_FUSE_MAX)
#define AF1_AF2_CURRENT_LSB		    TPS2482_CURRENT_LSB_EQ(AF1_AF2_FUSE_MAX)
#define CP_RF_CURRENT_LSB			TPS2482_CURRENT_LSB_EQ(CP_RF_FUSE_MAX)

#define LV_CAL_VAL			TPS2482_CAL_EQ(LV_CURRENT_LSB, R_SHUNT)
#define SH_CAL_VAL			TPS2482_CAL_EQ(SH_CURRENT_LSB, R_SHUNT)
#define LT_CAL_VAL			TPS2482_CAL_EQ(LT_CURRENT_LSB, R_SHUNT)
#define BM_L_CAL_VAL			TPS2482_CAL_EQ(BM_L_CURRENT_LSB, R_SHUNT)
#define SM_CAL_VAL			TPS2482_CAL_EQ(SM_CURRENT_LSB, R_SHUNT)
#define AF1_AF2_CAL_VAL	    TPS2482_CAL_EQ(AF1_AF2_CURRENT_LSB, R_SHUNT)
#define CP_RF_CAL_VAL		TPS2482_CAL_EQ(CP_RF_CURRENT_LSB, R_SHUNT)

// This calculation is needed to have an alert go out when current exceeds fuse ratings
#define LV_ALERT_LIM_VAL		TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(LV_FUSE_MAX / LV_CURRENT_LSB),             LV_CAL_VAL)
#define SH_ALERT_LIM_VAL		TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(SH_FUSE_MAX / SH_CURRENT_LSB),             SH_CAL_VAL)
#define LT_ALERT_LIM_VAL		TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(LT_FUSE_MAX / LT_CURRENT_LSB),             LT_CAL_VAL)
#define BM_L_ALERT_LIM_VAL		TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(BM_L_FUSE_MAX / BM_L_CURRENT_LSB),             BM_L_CAL_VAL)
#define SM_ALERT_LIM_VAL		TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(SM_FUSE_MAX / SM_CURRENT_LSB),             SM_CAL_VAL)
#define AF1_AF2_ALERT_LIM_VAL	TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(AF1_AF2_FUSE_MAX / AF1_AF2_CURRENT_LSB),   AF1_AF2_CAL_VAL)
#define CP_RF_ALERT_LIM_VAL		TPS2482_SHUNT_VOLT_REG_VAL_EQ((uint16_t)(CP_RF_FUSE_MAX / CP_RF_CURRENT_LSB),       CP_RF_CAL_VAL)

#define LV_POWER_LSB	    TPS2482_POWER_LSB_EQ(LV_CURRENT_LSB)
#define SH_POWER_LSB	    TPS2482_POWER_LSB_EQ(SH_CURRENT_LSB)
#define LT_POWER_LSB	    TPS2482_POWER_LSB_EQ(LT_CURRENT_LSB)
#define BM_L_POWER_LSB	    TPS2482_POWER_LSB_EQ(BM_L_CURRENT_LSB)
#define SM_POWER_LSB		TPS2482_POWER_LSB_EQ(SM_CURRENT_LSB)
#define AF1_AF2_POWER_LSB	TPS2482_POWER_LSB_EQ(AF1_AF2_CURRENT_LSB)
#define CP_RF_POWER_LSB     TPS2482_POWER_LSB_EQ(CP_RF_CURRENT_LSB)

#define FEB_BREAK_THRESHOLD	(uint8_t)20

#define SLEEP_TIME 10

void FEB_Main_Setup(void);
void FEB_Main_Loop(void);
void FEB_1ms_Callback(void);
void FEB_CAN1_Rx_Callback(CAN_RxHeaderTypeDef *rx_header, void *data);

#endif
