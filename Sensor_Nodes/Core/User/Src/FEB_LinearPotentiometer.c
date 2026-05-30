/**
 ******************************************************************************
 * @file           : FEB_LinearPotentiometer.c
 * @brief          : Linear potentiometer driver implementation.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_LinearPotentiometer.h"

#include <stdbool.h>

#include "adc.h"
#include "main.h"

uint16_t lp_raw[2] = {0, 0};
float lp_displacement_mm[2] = {0.0f, 0.0f};

static const uint32_t LP_ADC_CHANNELS[2] = {
    ADC_CHANNEL_13, /* PC3 / LP_Wiper1 */
    ADC_CHANNEL_9,  /* PB1 / LP_Wiper2 */
};

static const uint16_t LP_RAW_MIN[2] = {FEB_LP_1_RAW_MIN, FEB_LP_2_RAW_MIN};
static const uint16_t LP_RAW_MAX[2] = {FEB_LP_1_RAW_MAX, FEB_LP_2_RAW_MAX};
static const float LP_LENGTH_MM[2] = {FEB_LP_1_LENGTH_MM, FEB_LP_2_LENGTH_MM};

static uint16_t read_channel(uint32_t adc_channel)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = adc_channel;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    return 0;
  }

  if (HAL_ADC_Start(&hadc1) != HAL_OK)
  {
    return 0;
  }

  uint16_t value = 0;
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
  {
    value = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }
  HAL_ADC_Stop(&hadc1);
  return value;
}

static float raw_to_mm(uint16_t raw, uint16_t raw_min, uint16_t raw_max, float length_mm)
{
  uint16_t lo = raw_min;
  uint16_t hi = raw_max;
  bool reversed = false;
  if (lo > hi)
  {
    lo = raw_max;
    hi = raw_min;
    reversed = true;
  }
  if (hi == lo)
  {
    return 0.0f;
  }
  if (raw <= lo)
  {
    return reversed ? length_mm : 0.0f;
  }
  if (raw >= hi)
  {
    return reversed ? 0.0f : length_mm;
  }

  float fraction = (float)(raw - lo) / (float)(hi - lo);
  if (reversed)
  {
    fraction = 1.0f - fraction;
  }
  return fraction * length_mm;
}

void FEB_LinearPotentiometer_Init(void)
{
  /* ADC1 is initialised by MX_ADC1_Init() in CubeMX-generated code.
   * Both LP_Wiper1 (PC3) and LP_Wiper2 (PB1) GPIOs are already configured
   * as analog inputs by HAL_ADC_MspInit(). No further setup is required. */
}

void read_LinearPotentiometer(void)
{
  for (uint8_t i = 0; i < 2; i++)
  {
    lp_raw[i] = read_channel(LP_ADC_CHANNELS[i]);
    lp_displacement_mm[i] = raw_to_mm(lp_raw[i], LP_RAW_MIN[i], LP_RAW_MAX[i], LP_LENGTH_MM[i]);
  }
}
