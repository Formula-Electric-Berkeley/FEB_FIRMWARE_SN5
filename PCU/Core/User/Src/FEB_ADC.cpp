#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_tim.h"
#include "adc.h"
#include "tim.h"

extern ADC_HandleTypeDef hadc3;
extern TIM_HandleTypeDef htim2;
// extern CAN_HandleTypeDef hcan1; Complete upon CAN addition.

volatile uint16_t adc_buf[2];

void init()
{
  HAL_ADC_Start_DMA(&hadc3, adc_buf, 2);
  HAL_TIM_Base_Start(&htim2);
}

uint16_t getAPPS1()
{
  return adc_buf[0]; // following more conversion
}

uint16_t getAPPS2()
{
  return adc_buf[1]; // following more conversion
}
