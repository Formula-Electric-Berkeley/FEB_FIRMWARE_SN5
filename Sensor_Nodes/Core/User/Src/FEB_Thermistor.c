/**
 ******************************************************************************
 * @file           : FEB_Thermistor.c
 * @brief          : Coolant NTC thermistor driver implementation.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Thermistor.h"

#include "adc.h"
#include "main.h"

#include <math.h>

#define TH_ADC_MAX 4095u
#define TH_RAW_OPEN 4050u
#define TH_RAW_SHORT 50u
#define TH_KELVIN_OFFSET 273.15f

uint16_t th_raw[FEB_TH_COUNT] = {0};
float th_resistance_ohm[FEB_TH_COUNT] = {0.0f};
float th_temp_c[FEB_TH_COUNT] = {0.0f};
uint8_t th_status = 0;

static const FEB_TH_Cal_t th_cal[FEB_TH_COUNT] = {
    {.adc_channel = ADC_CHANNEL_12, .r_pullup_ohm = 5000.0f, .r0_ohm = 5000.0f, .beta_k = 3950.0f, .t0_c = 25.0f},
    {.adc_channel = ADC_CHANNEL_11, .r_pullup_ohm = 5000.0f, .r0_ohm = 5000.0f, .beta_k = 3950.0f, .t0_c = 25.0f},
    {.adc_channel = ADC_CHANNEL_10, .r_pullup_ohm = 5000.0f, .r0_ohm = 5000.0f, .beta_k = 3950.0f, .t0_c = 25.0f},
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

/* resistance of the NTC rail = R_pullup * raw / (max - raw) */
static float raw_to_resistance_ohm(uint16_t raw, const FEB_TH_Cal_t *c)
{
  return c->r_pullup_ohm * (float)raw / (float)(TH_ADC_MAX - raw);
}

static float resistance_to_temp_c(float r_ohm, const FEB_TH_Cal_t *c)
{
  float inv_t = 1.0f / (c->t0_c + TH_KELVIN_OFFSET) + logf(r_ohm / c->r0_ohm) / c->beta_k;
  return 1.0f / inv_t - TH_KELVIN_OFFSET;
}

void FEB_Thermistor_Init(void)
{
  /* just kept for sensor nodes cpp file  */
}

void read_Thermistor(void)
{
  uint8_t status = 0;
  for (uint8_t i = 0; i < FEB_TH_COUNT; i++)
  {
    th_raw[i] = read_channel(th_cal[i].adc_channel);

    if (th_raw[i] >= TH_RAW_OPEN)
    {
      status |= FEB_TH_STATUS_OPEN(i);
      th_resistance_ohm[i] = 0.0f;
      th_temp_c[i] = 0.0f;
      continue;
    }
    if (th_raw[i] <= TH_RAW_SHORT)
    {
      status |= FEB_TH_STATUS_SHORT(i);
      th_resistance_ohm[i] = 0.0f;
      th_temp_c[i] = 0.0f;
      continue;
    }

    th_resistance_ohm[i] = raw_to_resistance_ohm(th_raw[i], &th_cal[i]);
    th_temp_c[i] = resistance_to_temp_c(th_resistance_ohm[i], &th_cal[i]);
  }
  th_status = status;
}
