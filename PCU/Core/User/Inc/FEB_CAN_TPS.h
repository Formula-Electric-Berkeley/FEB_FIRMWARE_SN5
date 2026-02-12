#ifndef INC_FEB_CAN_TPS_H_
#define INC_FEB_CAN_TPS_H_

#include "stm32f4xx_hal.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "TPS2482.h"
#include <string.h>

/* Conversion helper macros - TPS2482 conversion macros are in TPS2482.h */
#define SIGN_MAGNITUDE(n) ((int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF)))

/* Message structure */
typedef struct
{
  uint16_t bus_voltage_mv;  /* Bus voltage in millivolts */
  int16_t current_ma;       /* Current in milliamps */
  int32_t shunt_voltage_uv; /* Shunt voltage in microvolts */
} TPS_MESSAGE_TYPE;

/* Data type for console command access */
typedef struct
{
  uint16_t bus_voltage_mv;
  int16_t current_ma;
  int32_t shunt_voltage_uv;
} FEB_CAN_TPS_Data_t;

/* Global variable - defined in FEB_CAN_TPS.c */
extern TPS_MESSAGE_TYPE TPS_MESSAGE;

/* Function prototypes */
void FEB_CAN_TPS_Init(void);
void FEB_CAN_TPS_Update(I2C_HandleTypeDef *hi2c, uint8_t *i2c_addresses, uint8_t num_devices);
void FEB_CAN_TPS_Transmit(void);
void FEB_CAN_TPS_GetData(FEB_CAN_TPS_Data_t *data);

#endif /* INC_FEB_CAN_TPS_H_ */
