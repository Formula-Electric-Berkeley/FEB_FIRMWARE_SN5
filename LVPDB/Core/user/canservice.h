// Abstraction layer for CAN (wrapping HAL CAN into human-readable functions)

#pragma once
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_can.h"   
#include <stdint.h>

#if !defined(HAL_CAN_MODULE_ENABLED)
#error "HAL_CAN_MODULE_ENABLED is not defined. Create Core/Inc/stm32f4xx_hal_conf.h and enable CAN."
#endif

typedef struct {
  CAN_HandleTypeDef *hcan;
} can_service_t;

HAL_StatusTypeDef can_service_init(can_service_t *svc, CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef can_service_set_basic_filter(can_service_t *svc, uint32_t id, uint32_t mask);
HAL_StatusTypeDef can_service_start(can_service_t *svc);
HAL_StatusTypeDef can_service_send(can_service_t *svc, uint32_t std_id, uint8_t *data, uint8_t dlc);
int  can_service_recv(can_service_t *svc, uint32_t *std_id, uint8_t *data, uint8_t *dlc);
