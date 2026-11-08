#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_tim.h"
#include "adc.h"
#include "tim.h"

extern ADC_HandleTypeDef hadc3;
extern TIM_HandleTypeDef htim2;
// extern CAN_HandleTypeDef hcan1; Complete upon CAN addition.

volatile uint16_t adc_buf[2];
uint8_t apps_percent[2];

#define APPS1 0
#define APPS2 1
#define MAX_TORQUE 225  //
#define MAX_VOLTAGE 3.3 // for stm chip
#define IMPLAUSIBILITY 0.1

void init()
{
  HAL_ADC_Start_DMA(&hadc3, adc_buf, 2);
  HAL_TIM_Base_Start(&htim2);
}

uint16_t getAPPS1()
{ // returns voltage 0-3.3

  // 0 -3.3v   2022
  return adc_buf[0]; // following more conversion
}

uint16_t getAPPS2()
{                    // returns voltage 0-3.3
  return adc_buf[1]; // following more conversion
}

/*Retrieves sensor and inverse sensor torque calculations*/
uint8_t get_torque(uint8_t voltage, uint8_t apps_id)
{
  if (apps_id == APPS1)
  {
    return ((voltage / MAX_VOLTAGE) * MAX_TORQUE);
  }
  if (apps_id == APPS2)
    return ((MAX_VOLTAGE - voltage) * MAX_TORQUE);
}

/*Validates torque calculations based on 10% threshold*/
bool validate_torque(uint8_t voltage1, uint8_t voltage2)
{
  if (abs(voltage1 - voltage2) / (MAX_VOLTAGE) > IMPLAUSIBILITY)
  {
    return 1; // Differences greater than 10% are rendered false.
  }
  return 0;
}

/*
  while true
    compare sensor1 and sensor2:
      get torque values
        validate torque values:
            error or send torque package
*/
