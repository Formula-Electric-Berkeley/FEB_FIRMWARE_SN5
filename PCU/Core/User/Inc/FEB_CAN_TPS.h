#ifndef INC_FEB_CAN_TPS_H_
#define INC_FEB_CAN_TPS_H_

#include "stm32f4xx_hal.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "feb_tps.h"
#include <string.h>

/* Status flag bit definitions */
#define TPS_STATUS_VOLTAGE_OVERFLOW (1U << 0) /* Bus voltage exceeded uint16 max */
#define TPS_STATUS_CURRENT_OVERFLOW (1U << 1) /* Current exceeded int16 range */
#define TPS_STATUS_CURRENT_NEGATIVE (1U << 2) /* Current reading is negative */
#define TPS_STATUS_POLL_ERROR (1U << 3)       /* Poll failed, values may be stale */

/* Message structure */
typedef struct
{
  uint16_t bus_voltage_mv;  /* Bus voltage in millivolts */
  int16_t current_ma;       /* Current in milliamps */
  int32_t shunt_voltage_uv; /* Shunt voltage in microvolts */
  uint8_t status_flags;     /* Status flags for overflow/negative current */
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
