#ifndef INC_FEB_WSS_H_
#define INC_FEB_WSS_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FEB_Main.h"

extern uint8_t left_rpm;  // hetvi: extern
extern uint8_t right_rpm; // uint8_t = 1 byte for CAN payload

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
void WSS_Main(void);

#endif /* INC_FEB_WSS_H_ */
