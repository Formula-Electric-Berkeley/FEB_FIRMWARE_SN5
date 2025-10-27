#pragma once
#include <stdint.h>
#include "stm32f4xx_hal.h"

// 10ms hook for CAN 
typedef void (*timer10ms_cb_t)(void);

void timer_service_attach_10ms(timer10ms_cb_t cb);
void timer_service_on_irq(TIM_HandleTypeDef *htim); // call from HAL_TIM_PeriodElapsedCallback