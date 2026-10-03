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

static const FEB_SG_CAL_t sg_cal[FEB_SG_COUNT] = {
    {/*
       Per strain gauge documentation:
       SGX_Ref = 1.625 - 10 * (SGX_OUT - 1.625)
       ADC Read: uint32_t mv = (raw_value * 3300) / 4095;
     */
     .adc_channel = ADC_CHANNEL_7,
     .resistance_offset = 0,
     .start_voltage_mv = 0,
     .excitation_voltage_mv = 0,
     .sensitivity_rating = 0},
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

/*https://app.notion.com/p/SN5-Sensor-Nodes-27a538db02df80a8b309d9dc003f8fd4?source=copy_link#319538db02df80308f4dd6ed84c4d664

force = (measured voltage / (excitation voltage * sensitivity rating)) * full scale capacity
*/
static float mV_to_newtons(uint16_t raw, const FEB_SG_CAL_t *c)
{
  return raw;
}

void read_StrainGauge(void)
{
  for (uint8_t i = 0; i < FEB_SG_COUNT; i++)
  {
    sg_raw[i] = read_channel(sg_cal[i].adc_channel);
    sg_force_N[i] = mV_to_newtons(sg_raw[i], &sg_cal[i]);
  }
}

float get_strain_gauge(int i)
{
  return sg_force_N[i];
}
