#ifndef INC_FEB_WSS_H_
#define INC_FEB_WSS_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#include "FEB_Main.h"

// Wheel ground speed in 0.01 mph units (saturates at 65535 = 655.35 mph).
extern uint16_t left_mph_x100;
extern uint16_t right_mph_x100;

// Most recent direction sign per wheel: +1 = forward, -1 = reverse, 0 = unknown/stopped.
extern int8_t left_dir;
extern int8_t right_dir;

// Initializes wheel-state ring buffers and ensures TIM5 is started for µs timestamping.
void FEB_WSS_Init(void);

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

// Recomputes left_mph_x100/right_mph_x100 from the most recent edges.
void WSS_Main(void);

#endif /* INC_FEB_WSS_H_ */
