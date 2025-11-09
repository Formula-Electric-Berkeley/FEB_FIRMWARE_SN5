#ifndef INC_FEB_TPS2482_H_
#define INC_FEB_TPS2482_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <string.h>

/* TPS2482 Register Addresses */
#define TPS2482_CONFIG      0x00
#define TPS2482_SHUNT_VOLT  0x01
#define TPS2482_BUS_VOLT    0x02
#define TPS2482_POWER       0x03
#define TPS2482_CURRENT     0x04
#define TPS2482_CAL         0x05
#define TPS2482_MASK        0x06
#define TPS2482_ALERT_LIM   0x07
#define TPS2482_ID          0xFF

/* TPS2482 Configuration Masks */
#define TPS2482_CONFIG_RST_MASK(config)  ((config) & 0x8000)

/* TPS2482 Mask Register Bits */
#define TPS2482_MASK_SOL    (1 << 15)
#define TPS2482_MASK_SUL    (1 << 14)
#define TPS2482_MASK_BOL    (1 << 13)
#define TPS2482_MASK_BUL    (1 << 12)
#define TPS2482_MASK_CNVR   (1 << 3)
#define TPS2482_MASK_POL    (1 << 1)

/* TPS2482 Conversion Constants */
#define TPS2482_CONV_VBUS           0.004f  /* 4mV per LSB */
#define TPS2482_CURRENT_LSB_EQ(shunt_mohm)  (0.0008192f / (shunt_mohm))

/* Configuration Structure */
typedef struct {
    uint16_t config;
    uint16_t cal;
    uint16_t mask;
    uint16_t alert_lim;
} TPS2482_Configuration;

/* Function Prototypes */
void TPS2482_Init(I2C_HandleTypeDef *hi2c, uint8_t *addresses, TPS2482_Configuration *configurations, uint16_t *ids, bool *res, uint8_t messageCount);

/* Register Read Functions */
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

/* Register Write Functions */
void TPS2482_Write_Register(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint8_t reg, uint16_t *transmit, uint8_t messageCount);
void TPS2482_Write_Config(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount);
void TPS2482_Write_CAL(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount);
void TPS2482_Write_Mask(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount);
void TPS2482_Write_Alert_Limit(I2C_HandleTypeDef *hi2c, uint8_t *addresses, uint16_t *transmit, uint8_t messageCount);

/* GPIO Functions */
void TPS2482_GPIO_Write(GPIO_TypeDef **GPIOx, uint16_t *GPIO_Pin, uint8_t *state, uint8_t messageCount);
void TPS2482_GPIO_Read(GPIO_TypeDef **GPIOx, uint16_t *GPIO_Pin, GPIO_PinState *result, uint8_t messageCount);
void TPS2482_Enable(GPIO_TypeDef **GPIOx, uint16_t *GPIO_Pin, uint8_t *en_dis, bool *result, uint8_t messageCount);

#endif /* INC_FEB_TPS2482_H_ */
