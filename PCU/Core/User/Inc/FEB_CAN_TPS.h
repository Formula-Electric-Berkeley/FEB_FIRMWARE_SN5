#ifndef INC_FEB_CAN_TPS_H_
#define INC_FEB_CAN_TPS_H_

#include "stm32f4xx_hal.h"
#include "FEB_CAN_TX.h"
#include "FEB_CAN_IDs.h"
#include "FEB_TPS2482.h"
#include <string.h>

/* TPS2482 Conversion Macros */
#define FLOAT_TO_UINT16_T(n)    ((uint16_t)(n * 1000))  /* Voltage in mV */
#define FLOAT_TO_INT16_T(n)     ((int16_t)(n * 1000))   /* Current in mA */
#define SIGN_MAGNITUDE(n)       ((int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF)))

/* TPS2482 Constants (adjust based on your hardware) */
#define TPS2482_CONV_VBUS       0.004f  /* Voltage conversion factor */
#define TPS2482_CURRENT_LSB_EQ(shunt_mohm)  (0.0008192f / (shunt_mohm))  /* Current LSB equation */

/* Message structure */
typedef struct {
    uint16_t bus_voltage_mv;  /* Bus voltage in millivolts */
    int16_t current_ma;       /* Current in milliamps */
} TPS_MESSAGE_TYPE;

/* Global variable - defined in FEB_CAN_TPS.c */
extern TPS_MESSAGE_TYPE TPS_MESSAGE;

/* Function prototypes */
void FEB_CAN_TPS_Init(void);
void FEB_CAN_TPS_Update(I2C_HandleTypeDef *hi2c, uint8_t *i2c_addresses, uint8_t num_devices);
void FEB_CAN_TPS_Transmit(void);

#endif /* INC_FEB_CAN_TPS_H_ */