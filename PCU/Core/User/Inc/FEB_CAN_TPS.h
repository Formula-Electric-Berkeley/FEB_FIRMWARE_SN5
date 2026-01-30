#ifndef INC_FEB_CAN_TPS_H_
#define INC_FEB_CAN_TPS_H_

#include "stm32f4xx_hal.h"
#include "FEB_CAN_TX.h"
#include "FEB_CAN_IDs.h"
#include "TPS2482.h"
#include <string.h>

/* Conversion helper macros - TPS2482 conversion macros are in TPS2482.h */
#define SIGN_MAGNITUDE(n) ((int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF)))

/* Message structure */
typedef struct
{
  uint16_t bus_voltage_mv; /* Bus voltage in millivolts */
  int16_t current_ma;      /* Current in milliamps */
} TPS_MESSAGE_TYPE;

/* Global variable - defined in FEB_CAN_TPS.c */
extern TPS_MESSAGE_TYPE TPS_MESSAGE;

/* Function prototypes */
void FEB_CAN_TPS_Init(void);
void FEB_CAN_TPS_Update(I2C_HandleTypeDef *hi2c, uint8_t *i2c_addresses, uint8_t num_devices);
void FEB_CAN_TPS_Transmit(void);

#endif /* INC_FEB_CAN_TPS_H_ */