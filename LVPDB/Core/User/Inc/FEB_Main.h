#ifndef INC_FEB__MAIN_H_
#define INC_FEB__MAIN_H_

#include "FEB_CAN.h"
#include "FEB_CAN_Heartbeat.h"
#include "feb_tps.h"
#include "feb_console.h"
#include "feb_uart.h"
#include "feb_uart_log.h"

#include <stm32f4xx_hal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * The LVPDB has multiple TPS chips on the bus. These are the addresses of
 * each of the TPS chips. The naming conventions is as follows:
 *		LV - Low Voltage Source (sda-scl)
 *		SH - Shutdown Source (sda-sda)
 *		LT - Laptop Branch (gnd-gnd)
 *		BM_L - Braking Servo, Lidar (gnd-scl)
 *		SM - Steering Motor (gnd-sda)
 *		AF1_AF2 - Accumulator Fans 1 Branch (gnd-vs)
 *		CP_RF - Coolant Pump + Radiator Fans Branch (vs-scl)
 */

#define NUM_TPS2482 7

#define LV_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL)     // A1:SDA  A0:SCL
#define SH_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SDA)     // A1:SDA  A0:SDA
#define LT_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND)     // A1:GND  A0:GND
#define BM_L_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_SCL, FEB_TPS_PIN_SCL)   // A1:SCL  A0:SCL
#define SM_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SDA)     // A1:GND  A0:SDA
#define AF1_AF2_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_VS) // A1:GND  A0:VS
#define CP_RF_ADDR FEB_TPS_ADDR(FEB_TPS_PIN_VS, FEB_TPS_PIN_SCL)   // A1:VS   A0:SCL

// All TPS2482 implementation share the same WSR52L000FEA .002 ohm shunt resistor
#define R_SHUNT (double)(.002) // Ohm

// Fuse max are the current ratings for the fuses, and are used as current maximums
// Fuse max can be exceeded safely in "peak current" scenarios so maybe not best metric
#define BATTERY_FUSE_MAX (double)(30) // A from +24BAT -> +24GLV
#define LV_FUSE_MAX (double)(5)       // A from +24GLV -> +24V
#define SH_FUSE_MAX (double)(5)
#define LT_FUSE_MAX (double)(6.3)
#define BM_L_FUSE_MAX (double)(16)
#define SM_FUSE_MAX (double)(12)
#define AF1_AF2_FUSE_MAX (double)(20)
#define CP_RF_FUSE_MAX (double)(10)

#define FLOAT_TO_UINT16_T(n) ((uint16_t)(n * 1000))                                           // for voltage (mV)
#define FLOAT_TO_INT16_T(n) ((int16_t)(n * 1000))                                             // for voltage (mV)
#define SIGN_MAGNITUDE(n) (int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF)) // for current reg

// Current LSB values (fuse max gives resolution at cost of range)
#define LV_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(LV_FUSE_MAX)
#define SH_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(SH_FUSE_MAX)
#define LT_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(LT_FUSE_MAX)
#define BM_L_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(BM_L_FUSE_MAX)
#define SM_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(SM_FUSE_MAX)
#define AF1_AF2_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(AF1_AF2_FUSE_MAX)
#define CP_RF_CURRENT_LSB FEB_TPS_CALC_CURRENT_LSB(CP_RF_FUSE_MAX)

// Power LSB values
#define LV_POWER_LSB FEB_TPS_CALC_POWER_LSB(LV_CURRENT_LSB)
#define SH_POWER_LSB FEB_TPS_CALC_POWER_LSB(SH_CURRENT_LSB)
#define LT_POWER_LSB FEB_TPS_CALC_POWER_LSB(LT_CURRENT_LSB)
#define BM_L_POWER_LSB FEB_TPS_CALC_POWER_LSB(BM_L_CURRENT_LSB)
#define SM_POWER_LSB FEB_TPS_CALC_POWER_LSB(SM_CURRENT_LSB)
#define AF1_AF2_POWER_LSB FEB_TPS_CALC_POWER_LSB(AF1_AF2_CURRENT_LSB)
#define CP_RF_POWER_LSB FEB_TPS_CALC_POWER_LSB(CP_RF_CURRENT_LSB)

#define FEB_BREAK_THRESHOLD (uint8_t)20

#define SLEEP_TIME 10

void FEB_Main_Setup(void);
void FEB_Main_Loop(void);
void FEB_1ms_Callback(void);
void FEB_CAN1_Rx_Callback(CAN_RxHeaderTypeDef *rx_header, void *data);

#endif
