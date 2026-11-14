/**
 ******************************************************************************
 * @file           : PCU_ADC.cpp
 * @brief          : Analog to Digital Convesions
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "main.h"
#include "adc.h"
#include "tim.h"
#include <cstdint>
#include <math.h>
#include <algorithm>
#include "PCU_ADC.hpp"

extern ADC_HandleTypeDef hadc3;
extern TIM_HandleTypeDef htim2;

volatile uint16_t adc_buf[ADC_NUM_CHANNELS];

// Populate our buffer array with the data coming into the stm32 pin
void PCU_ADC_Init()
{
  if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc_buf, ADC_NUM_CHANNELS) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_TIM_Base_Start(&htim2);
}

/* APPS Sensor Class + Methods */

APPS_Sensor::APPS_Sensor(uint8_t id, uint16_t min, uint16_t max)
{
  apps_id = id;
  min_raw = min;
  max_raw = max;
}

uint16_t APPS_Sensor::getRaw()
{
  return adc_buf[apps_id];
}

uint16_t APPS_Sensor::get_torque()
{
  return get_percentage() * MAX_TORQUE;
}

float APPS_Sensor::get_percentage()
{
  // My reasoning for these checks is that the sensors
  // are suceptible to noise so its possible for the
  // calc to be sligtly less than 0 percent or slighlty more, yet
  // doesn't make sense when we multiply by max torque.  we don't want
  // something slightly higher than max torque.

  float percent = (float(getRaw()) - float(min_raw)) / (float(max_raw) - float(min_raw));
  if (percent < 0.0f)
  {
    return 0.0f;
  }
  if (percent > 1.0f)
  {
    return 1.0f;
  }
  return percent;
}

// This is for the open circuit rule.
bool APPS_Sensor::in_range()
{
  uint16_t low = std::min(min_raw, max_raw);
  uint16_t high = std::max(min_raw, max_raw);
  uint16_t margin = (high - low) * APPS_RANGE_MARGIN_PCT;
  uint16_t raw = getRaw();
  return raw + margin >= low && raw <= high + margin;
}

APPS_Sensor apps1(APPS1, MIN_APPS1, MAX_APPS1);
APPS_Sensor apps2(APPS2, MIN_APPS2, MAX_APPS2);

/* If both sensors are off by 10%, return false for a trip, true otherwise*/
bool validate_torque(APPS_Sensor &sensor1, APPS_Sensor &sensor2)
{
  return std::abs(sensor1.get_percentage() - sensor2.get_percentage()) <= APPS_IMPLAUSIBILITY_PCT;
}

/* Brakes Sensor Class + Methods */
Brake_Sensor::Brake_Sensor(uint8_t id, uint16_t min, uint16_t max)
{
  brake_id = id;
  min_raw = min;
  max_raw = max;
}

uint16_t Brake_Sensor::getRaw()
{
  return adc_buf[brake_id];
}

float Brake_Sensor::get_percentage()
{
  float percent = (float(getRaw()) - float(min_raw)) / (float(max_raw) - float(min_raw));
  if (percent < 0.0f)
  {
    return 0.0f;
  }
  if (percent > 1.0f)
  {
    return 1.0f;
  }
  return percent;
}

bool Brake_Sensor::is_pressed()
{
  return get_percentage() > BRAKE_PRESSED_PCT;
}

bool Brake_Sensor::in_range()
{
  uint16_t raw = getRaw();
  return raw >= BRAKE_MIN_VALID_RAW && raw <= BRAKE_MAX_VALID_RAW;
}

Brake_Sensor brake1(BRAKE1, MIN_BRAKE1, MAX_BRAKE1);
Brake_Sensor brake2(BRAKE2, MIN_BRAKE2, MAX_BRAKE2);

bool brakes_pressed(Brake_Sensor &sensor1, Brake_Sensor &sensor2)
{
  return sensor1.is_pressed() || sensor2.is_pressed();
}
