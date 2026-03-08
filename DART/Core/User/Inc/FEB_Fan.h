#ifndef INC_FEB_FAN_H_
#define INC_FEB_FAN_H_

// ********************************** Includes & External **********************************

#include <stm32f0xx_hal.h>
#include <stdbool.h>
#include <math.h>

// ********************************** Defines **********************************

#define PRINT_DEBUG_TACH 0

#define TACH_FILTER_EXPONENT 2

#define TIMCLOCK (uint32_t)48000000
#define PRESCALAR (uint32_t)1
#define REF_CLOCK (float)TIMCLOCK / PRESCALAR
#define NUM_FANS (uint32_t)5
#define PWM_SIZE (uint32_t)40
#define PWM_COUNTER (uint32_t)(TIMCLOCK * PWM_SIZE / (1000.0f * 1000.0f))
#define PWM_START_PERCENT (float)1

#define TEMP_START_FAN 25
#define TEMP_END_FAN 45

#define ADC_FILTER_EXPONENT 2

// ********************************** Function Prototypes **********************************

void FEB_Fan_Init(void);

void FEB_Fan_CAN_Msg_Process(uint8_t *FEB_CAN_Rx_Data);

void FEB_Fan_PWM_Init(void);
void FEB_Fan_All_Speed_Set(uint8_t speed);

void FEB_Fan_TACH_Init(void);
void FEB_Fan_TACH_Callback(TIM_HandleTypeDef *htim);

#endif /* INC_FEB_FAN_H_ */
