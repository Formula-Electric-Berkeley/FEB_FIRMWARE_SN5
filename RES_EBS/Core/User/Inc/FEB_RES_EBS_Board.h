#ifndef FEB_RES_EBS_BOARD_H
#define FEB_RES_EBS_BOARD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

typedef struct
{
  bool initialized;
  uint8_t i2c_address;
  uint16_t device_id;
  uint16_t config;
  uint16_t cal;
  uint16_t mask;
  uint16_t alert_limit;
  uint16_t shunt_voltage_raw;
  uint16_t bus_voltage_raw;
  uint16_t current_raw;
  float shunt_voltage_mv;
  float bus_voltage_v;
  float current_a;
  GPIO_PinState power_good;
  GPIO_PinState alert;
} RES_EBS_TPS_Status_t;

void RES_EBS_Board_Init(void);

bool RES_EBS_TPS_Init(void);
bool RES_EBS_TPS_IsInitialized(void);
bool RES_EBS_TPS_Read(RES_EBS_TPS_Status_t *status);
void RES_EBS_TPS_GetPinStates(GPIO_PinState *power_good, GPIO_PinState *alert);

void RES_EBS_Relay_Set(bool enabled);
bool RES_EBS_Relay_Get(void);
const char *RES_EBS_Relay_GetName(void);
bool RES_EBS_TSActivation_Get(void);
const char *RES_EBS_TSActivation_GetName(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_RES_EBS_BOARD_H */
