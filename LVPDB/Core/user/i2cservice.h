// Hardware abstraction for I2c, which will hadnle transactions and mux 

#pragma once
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "main.h" 
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"  


typedef struct {
  I2C_HandleTypeDef *hi2c;
} i2c_service_t;

HAL_StatusTypeDef i2c_service_init(i2c_service_t *svc, I2C_HandleTypeDef *hi2c);

// TMUX1208 helpers
HAL_StatusTypeDef tmux1208_select(i2c_service_t *svc, uint8_t mux_addr_7bit, uint8_t channel); // 0..7

// reg helpers
HAL_StatusTypeDef i2c_rd(i2c_service_t *svc, uint8_t dev7, uint8_t reg, uint8_t *buf, uint16_t len);
HAL_StatusTypeDef i2c_wr(i2c_service_t *svc, uint8_t dev7, uint8_t reg, const uint8_t *buf, uint16_t len);
