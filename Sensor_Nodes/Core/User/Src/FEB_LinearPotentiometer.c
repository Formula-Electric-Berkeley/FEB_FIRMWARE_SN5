/**
 ******************************************************************************
 * @file           : FEB_LinearPotentiometer.c
 * @brief          : Linear potentiometer driver implementation.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_LinearPotentiometer.h"

#include "adc.h"
#include "main.h"

uint16_t lp_raw[FEB_LP_COUNT] = {0};
float lp_position_mm[FEB_LP_COUNT] = {0.0f};

/* Per-pot calibration table — the single place to tune the pots. Replace the
 * placeholder endpoints with real values captured on the bench: drive the
 * suspension to each end of travel, read the raw ADC count with the `LP|raw`
 * console command, and record (raw, mm) for the start and end points. See
 * FEB_LP_Cal_t in the header for field semantics. */
static const FEB_LP_Cal_t lp_cal[FEB_LP_COUNT] = {
    /* [0] LP_Wiper1 / PC3 / ADC1_IN13 — LEFT */
    {
        .adc_channel = ADC_CHANNEL_13,
        .raw_at_start = 0u,
        .start_mm = 0.0f,
        .raw_at_end = 4095u,
        .end_mm = 75.0f,
        .total_length_mm = 75.0f,
    },
    /* [1] LP_Wiper2 / PB1 / ADC1_IN9 — RIGHT */
    {
        .adc_channel = ADC_CHANNEL_9,
        .raw_at_start = 0u,
        .start_mm = 0.0f,
        .raw_at_end = 4095u,
        .end_mm = 75.0f,
        .total_length_mm = 75.0f,
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

/* Map a raw ADC count to an absolute position in mm by interpolating between the
 * calibrated start and end points, clamped to the pot's mechanical stroke. The
 * signed denominator transparently handles a reversed wiper (raw_at_start >
 * raw_at_end): the fraction is clamped to [0, 1] either way. */
static float raw_to_position_mm(uint16_t raw, const FEB_LP_Cal_t *c)
{
  if (c->raw_at_end == c->raw_at_start)
  {
    return c->start_mm;
  }

  float fraction =
      (float)((int32_t)raw - (int32_t)c->raw_at_start) / (float)((int32_t)c->raw_at_end - (int32_t)c->raw_at_start);
  if (fraction < 0.0f)
  {
    fraction = 0.0f;
  }
  if (fraction > 1.0f)
  {
    fraction = 1.0f;
  }

  float mm = c->start_mm + fraction * (c->end_mm - c->start_mm);
  if (mm < 0.0f)
  {
    mm = 0.0f;
  }
  if (mm > c->total_length_mm)
  {
    mm = c->total_length_mm;
  }
  return mm;
}

void FEB_LinearPotentiometer_Init(void)
{
  /* ADC1 is initialised by MX_ADC1_Init() in CubeMX-generated code. Both
   * LP_Wiper1 (PC3) and LP_Wiper2 (PB1) GPIOs are already configured as analog
   * inputs by HAL_ADC_MspInit(). No further setup is required. */
}

void read_LinearPotentiometer(void)
{
  for (uint8_t i = 0; i < FEB_LP_COUNT; i++)
  {
    lp_raw[i] = read_channel(lp_cal[i].adc_channel);
    lp_position_mm[i] = raw_to_position_mm(lp_raw[i], &lp_cal[i]);
  }
}
