#ifndef INC_TPS2482_H_
#define INC_TPS2482_H_

#include "stm32f4xx_hal.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"

// TPS2482 Datasheet: https://www.ti.com/lit/ds/symlink/tps2482.pdf


/*
 * 	Using:
 * 	I2C Device Address (Table 8.)
 * 	With A1 or A0 tied to:
 *		GND: 0x00
 *	 	V_s: 0x01
 *	  	SDA: 0x02
 *	  	SCL: 0x03
 * 	This define function abstracts the chip
 * 	away from its schematic's implementation
 */
#define TPS2482_I2C_ADDR_GND 	(uint8_t)(0x00)
#define TPS2482_I2C_ADDR_V_S 	(uint8_t)(0x01)
#define TPS2482_I2C_ADDR_SDA 	(uint8_t)(0x02)
#define TPS2482_I2C_ADDR_SCL 	(uint8_t)(0x03)

#define TPS2482_I2C_ADDR(a1,a0) (uint8_t)(0x40 | ((a1 << 2) | a0))

// The following are the conversion factors from register outputs to floats:
#define TPS2482_CONV_VSHUNT 	(double)(0.0025) 	// mV/LSB
#define TPS2482_CONV_VBUS	 	(double)(0.00125) 	// V/LSB

// The following are "magic equations" contained in current monitoring calculations:
// eq. 20, a = I_max (A), returns A/LSB
#define TPS2482_CURRENT_LSB_EQ(a)	(double)((double)(a) / (1.0 * 0x7FFF))

// eq. 21, a1 = I_LSB (A/bit), a0 = R_shunt (ohm)
#define TPS2482_CAL_EQ(a1,a0)		(uint16_t)((.00512) / ((1.0 * a1)*(1.0 * a0)))

// eq. 22, a1 = ShuntVoltRegValue, a0 = CAL
#define TPS2482_CURRENT_REG_VAL_EQ(a1,a0) \
									(uint16_t)((a1 * a0) / (2048))

// eq. 22 (modified), a1 = CurrentRegValue, a0 = CAL
#define TPS2482_SHUNT_VOLT_REG_VAL_EQ(a1,a0) \
									(uint16_t)((a1 * 2048) / a0)

// eq. 23, a1 = CurrentRegValue, a0 = BusVoltageRegValue
#define TPS2482_POWER_REG_VAL_EQ(a1,a0) \
									(uint16_t)((a1 * a0) / (20000))

// a = I_LSB, returns W/LSB
#define TPS2482_POWER_LSB_EQ(a) 	(double)(25 * a)


// I2C Register Address (Table 3.):
#define TPS2482_CONFIG 		(uint8_t)(0x00)	// R/W
#define TPS2482_SHUNT_VOLT 	(uint8_t)(0x01)	// R
#define TPS2482_BUS_VOLT 	(uint8_t)(0x02)	// R
#define TPS2482_POWER 		(uint8_t)(0x03)	// R
#define TPS2482_CURRENT		(uint8_t)(0x04)	// R
#define TPS2482_CAL			(uint8_t)(0x05)	// R/W
#define TPS2482_MASK		(uint8_t)(0x06)	// R/W
#define TPS2482_ALERT_LIM	(uint8_t)(0x07)	// R/W
#define TPS2482_ID			(uint8_t)(0xFF)	// R


// I2C Register Default Values (Table 3.):
#define TPS2482_CONFIG_DEFAULT 	(uint16_t)(0x4127)
// All other have default of 0, with exception of the unique ID register


// I2C Configuration Register Masks:
#define TPS2482_CONFIG_RST			(uint16_t)(0x0001 << 15)
#define TPS2482_CONFIG_AVG			(uint16_t)(0x0007 << 9) // Table 4.
#define TPS2482_CONFIG_VBUS_CT		(uint16_t)(0x0007 << 6) // Table 5.
#define TPS2482_CONFIG_VSHUNT_CT	(uint16_t)(0x0007 << 3) // Table 6.
#define TPS2482_CONFIG_MODE			(uint16_t)(0x0007) 		// Table 7.

// I2C Configuration Register Masking macros:
#define TPS2482_CONFIG_RST_MASK(a)			(a & TPS2482_CONFIG_RST) >> 15
#define TPS2482_CONFIG_AVG_MASK(a)			(a & TPS2482_CONFIG_AVG) >> 9
#define TPS2482_CONFIG_VBUS_CT_MASK(a)		(a & TPS2482_CONFIG_VBUS_CT) >> 6
#define TPS2482_CONFIG_VSHUNT_CT_MASK(a)	(a & TPS2482_CONFIG_VSHUNT_CT) >> 3
#define TPS2482_CONFIG_MODE_MASK(a)			(a & TPS2482_CONFIG_MODE)


// I2C Mask Register Masks:
#define TPS2482_MASK_SOL	(uint16_t)(0x0001 << 15) 	// Shunt Voltage Over-Voltage
#define TPS2482_MASK_SUL	(uint16_t)(0x0001 << 14) 	// Shunt Voltage Under-Voltage
#define TPS2482_MASK_BOL	(uint16_t)(0x0001 << 13) 	// Bus Voltage Over-Voltage
#define TPS2482_MASK_BUL	(uint16_t)(0x0001 << 12) 	// Bus Voltage Under-Voltage
#define TPS2482_MASK_POL	(uint16_t)(0x0001 << 11) 	// Power Over-Limit
#define TPS2482_MASK_CNVR	(uint16_t)(0x0001 << 10)	// Conversion Ready
#define TPS2482_MASK_AFF	(uint16_t)(0x0001 << 4)		// Alert Function Flag
#define TPS2482_MASK_CVRF	(uint16_t)(0x0001 << 3) 	// Conversion Ready Flag
#define TPS2482_MASK_OVF	(uint16_t)(0x0001 << 2) 	// Math Overflow Flag
#define TPS2482_MASK_APOL	(uint16_t)(0x0001 << 1) 	// Alert Polarity
#define TPS2482_MASK_LEN	(uint16_t)(0x0001) 			// Alert Latch Enable

// I2C Mask Register Masking macros:
#define TPS2482_MASK_SOL_MASK(a)	(a & TPS2482_MASK_SOL) >> 15
#define TPS2482_MASK_SUL_MASK(a)	(a & TPS2482_MASK_SUL) >> 14
#define TPS2482_MASK_BOL_MASK(a)	(a & TPS2482_MASK_BOL) >> 13
#define TPS2482_MASK_BUL_MASK(a)	(a & TPS2482_MASK_BUL) >> 12
#define TPS2482_MASK_POL_MASK(a)	(a & TPS2482_MASK_POL) >> 11
#define TPS2482_MASK_CNVR_MASK(a)	(a & TPS2482_MASK_CNVR) >> 10
#define TPS2482_MASK_AFF_MASK(a)	(a & TPS2482_MASK_AFF) >> 4
#define TPS2482_MASK_CVRF_MASK(a)	(a & TPS2482_MASK_CVRF) >> 3
#define TPS2482_MASK_OVF_MASK(a)	(a & TPS2482_MASK_OVF) >> 2
#define TPS2482_MASK_APOL_MASK(a)	(a & TPS2482_MASK_APOL) >> 1
#define TPS2482_MASK_LEN_MASK(a)	(a & TPS2482_MASK_LEN)


typedef struct {
	uint16_t config;
	uint16_t cal;
	uint16_t mask;
	uint16_t alert_lim;
} TPS2482_Configuration;

typedef struct {
	uint16_t vshunt;
	uint16_t vbus;
	uint16_t power;
	uint16_t current;
} TPS2482_Data;

/*
 * @brief  Apply a given configuration to all TPS2482 specified in addresses
 * @param *hi2c: the I2C handler
 * @param *addresses: array of results of calling TPS2482_I2C_ADDR
 * @param *configurations: TPS2482_Configuration array
 * @param *ids: buffer where function appends IC ids
 * @param messageCount: size of address array
 * @retval whether the configuration worked
 */
void TPS2482_Init(I2C_HandleTypeDef *hi2c, uint8_t *addresses, TPS2482_Configuration *configurations, \
																uint16_t *ids, bool *result, uint8_t messageCount);

/*
 * @brief Functions to read from a certain register
 * @param *hi2c: the I2C handler
 * @param *addresses: array of results of calling TPS2482_I2C_ADDR
 * @param reg: register number from macros
 * @param *results: uint16_t array of read values
 * @param messageCount: size of address array
 */
void TPS2482_Get_Register(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint8_t reg, uint16_t *results, uint8_t messageCount);

void TPS2482_Get_Config(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount);
void TPS2482_Poll_Shunt_Voltage(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount);
void TPS2482_Poll_Bus_Voltage(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount);
void TPS2482_Poll_Power(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount);
void TPS2482_Poll_Current(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount);
void TPS2482_Get_CAL(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount);
void TPS2482_Get_Mask(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount);
void TPS2482_Get_Alert_Limit(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount);
void TPS2482_Get_ID(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *results, uint8_t messageCount);

/*
 * @brief Functions to write to a certain register
 * @param *hi2c: the I2C handler
 * @param *addresses: array of results of calling TPS2482_I2C_ADDR
 * @param reg: register number from macros
 * @param *transmit: uint16_t array of written values
 * @param messageCount: size of address array
 */
void TPS2482_Write_Register(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint8_t reg, uint16_t *transmit, uint8_t messageCount);

void TPS2482_Write_Config(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount);
void TPS2482_Write_CAL(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount);
void TPS2482_Write_Mask(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount);
void TPS2482_Write_Alert_Limit(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount);

void TPS2482_GPIO_Write(GPIO_TypeDef **GPIOx, uint16_t *GPIO_Pin, uint8_t *state, uint8_t messageCount);
void TPS2482_GPIO_Read(GPIO_TypeDef **GPIOx, uint16_t *GPIO_Pin, GPIO_PinState *result, uint8_t messageCount);

void TPS2482_Enable(GPIO_TypeDef **GPIOx, uint16_t *GPIO_Pin, uint8_t *en_dis, bool *result, uint8_t messageCount);

#endif /* INC_TPS2482_H_ */
