/**
 ******************************************************************************
 * @file           : FEB_StrainGauge.c
 * @brief          : Strain gauge driver implementation.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_StrainGauge.h"

#include "adc.h"
#include "main.h"
#include <stdint.h>

uint16_t sg_raw[FEB_SG_COUNT] = {0};
float sg_force_N[FEB_SG_COUNT] = {0.0f};

static const FEB_SG_Cal_t cal[FEB_SG_COUNT] = {
    {
        .adc_channel = ADC_CHANNEL_1,
        .resistance_offset = 0 // TODO calibrate
    },
};

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

static float mV_to_newtons(uint16_t raw, const FEB_SG_Cal_t *c) {}

void FEB_StrainGauge_Init(void)
{
  /* ADC1 is initialised by MX_ADC1_Init() in CubeMX-generated code. Both
   * LP_Wiper1 (PC3) and LP_Wiper2 (PB1) GPIOs are already configured as analog
   * inputs by HAL_ADC_MspInit(). No further setup is required. */
}

void read_StrainGauge(void)
{
  for (uint8_t i = 0; i < FEB_SG_COUNT; i++)
  {
    sg_raw[i] = read_channel(sg_cal[i].adc_channel);
    sg_force_N[i] = mV_to_newtons(sg_raw[i], &sg_cal[i]);
  }
}
