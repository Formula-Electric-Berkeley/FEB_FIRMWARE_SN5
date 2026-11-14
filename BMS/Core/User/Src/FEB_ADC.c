#include "FEB_ADC.h"
#include "adc.h"
#include "feb_log.h"
#include <string.h>

#define TAG_ADC "[ADC]"

#define ADC_VREF_VOLTAGE 3.3f
#define ADC_MAX_VALUE 4095.0f
#define ADC_REFERENCE_VOLTAGE_MV 3300u

static volatile uint16_t adc1_dma_buffer[ADC_DMA_BUFFER_SIZE];

typedef struct
{
  uint16_t min;
  uint16_t max;
  uint32_t count;
} ADC_Accum_t;

typedef struct
{
  uint16_t min_raw;
  uint16_t max_raw;
} ADC_ChannelWindow_t;

static ADC_Accum_t adc_accum = {0xFFFFu, 0, 0};
static volatile ADC_ChannelWindow_t adc_window = {0, 0};
static uint32_t adc_window_ticks = 0;
static bool adc_initialized = false;
static uint32_t adc_error_count = 0;

static void ADC_AccumReset(ADC_Accum_t *acc)
{
  acc->min = 0xFFFFu;
  acc->max = 0;
  acc->count = 0;
}

static void ADC_AccumulateHalf(const volatile uint16_t *half)
{
  uint16_t local_min = 0xFFFFu;
  uint16_t local_max = 0;

  for (uint32_t s = 0; s < ADC_DMA_BUFFER_SIZE / 2; s++)
  {
    uint16_t v = half[s];
    if (v < local_min)
      local_min = v;
    if (v > local_max)
      local_max = v;
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (local_min < adc_accum.min)
    adc_accum.min = local_min;
  if (local_max > adc_accum.max)
    adc_accum.max = local_max;
  adc_accum.count += ADC_DMA_BUFFER_SIZE / 2;
  __set_PRIMASK(primask);
}

static void ADC_CloseWindow(void)
{
  ADC_Accum_t acc;

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  acc = adc_accum;
  ADC_AccumReset(&adc_accum);
  __set_PRIMASK(primask);

  if (acc.count == 0)
    return;

  primask = __get_PRIMASK();
  __disable_irq();
  adc_window.min_raw = acc.min;
  adc_window.max_raw = acc.max;
  __set_PRIMASK(primask);
}

static ADC_ChannelWindow_t ADC_GetWindowSnapshot(void)
{
  ADC_ChannelWindow_t snap;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  snap.min_raw = adc_window.min_raw;
  snap.max_raw = adc_window.max_raw;
  __set_PRIMASK(primask);
  return snap;
}

ADC_StatusTypeDef FEB_ADC_Init(void)
{
  memset((void *)adc1_dma_buffer, 0, sizeof(adc1_dma_buffer));
  ADC_AccumReset(&adc_accum);
  memset((void *)&adc_window, 0, sizeof(adc_window));
  adc_window_ticks = 0;
  adc_error_count = 0;
  adc_initialized = true;

  LOG_I(TAG_ADC, "BMS ADC oscilloscope initialized on PA1");
  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_Start(void)
{
  if (!adc_initialized)
  {
    return ADC_STATUS_NOT_INITIALIZED;
  }

  HAL_StatusTypeDef hal_status = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)(uintptr_t)adc1_dma_buffer, ADC_DMA_BUFFER_SIZE);
  if (hal_status != HAL_OK)
  {
    LOG_E(TAG_ADC, "Failed to start ADC1 DMA");
    return ADC_STATUS_ERROR;
  }

  LOG_I(TAG_ADC, "BMS ADC1 DMA started (834 samples, ~2 kHz IRQ rate)");
  return ADC_STATUS_OK;
}

ADC_StatusTypeDef FEB_ADC_Stop(void)
{
  HAL_ADC_Stop_DMA(&hadc1);
  return ADC_STATUS_OK;
}

void FEB_ADC_Tick(void)
{
  if (++adc_window_ticks >= ADC_WINDOW_TICKS)
  {
    adc_window_ticks = 0;
    ADC_CloseWindow();
  }
}

float FEB_ADC_RawToVoltage(uint16_t raw_value)
{
  return ((float)raw_value * ADC_VREF_VOLTAGE) / (float)ADC_MAX_VALUE;
}

uint32_t FEB_ADC_RawToMillivolts(uint16_t raw_value)
{
  return (uint32_t)((raw_value * ADC_REFERENCE_VOLTAGE_MV) / (uint32_t)ADC_MAX_VALUE);
}

void FEB_ADC_GetShutdownNoiseRaw(uint16_t *min_raw, uint16_t *max_raw)
{
  ADC_ChannelWindow_t snap = ADC_GetWindowSnapshot();
  if (min_raw)
    *min_raw = snap.min_raw;
  if (max_raw)
    *max_raw = snap.max_raw;
}

void FEB_ADC_GetShutdownNoiseVoltage(float *min_v, float *max_v)
{
  ADC_ChannelWindow_t snap = ADC_GetWindowSnapshot();
  if (min_v)
  {
    *min_v = FEB_ADC_RawToVoltage(snap.min_raw) * VOLTAGE_DIVIDER_RATIO_SHUTDOWN;
  }
  if (max_v)
  {
    *max_v = FEB_ADC_RawToVoltage(snap.max_raw) * VOLTAGE_DIVIDER_RATIO_SHUTDOWN;
  }
}

float FEB_ADC_GetShutdownNoisePeakToPeak(void)
{
  float min_v = 0.0f, max_v = 0.0f;
  FEB_ADC_GetShutdownNoiseVoltage(&min_v, &max_v);
  return max_v - min_v;
}

float FEB_ADC_GetShutdownVoltage(void)
{
  /* Return the most-recent sample from circular DMA buffer */
  if (hadc1.DMA_Handle == NULL)
    return 0.0f;
  uint32_t ndtr = __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
  if (ndtr == 0 || ndtr > ADC_DMA_BUFFER_SIZE)
    return 0.0f;
  uint32_t latest = (2 * ADC_DMA_BUFFER_SIZE - ndtr - 1) % ADC_DMA_BUFFER_SIZE;
  return FEB_ADC_RawToVoltage(adc1_dma_buffer[latest]) * VOLTAGE_DIVIDER_RATIO_SHUTDOWN;
}

void FEB_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    ADC_AccumulateHalf(&adc1_dma_buffer[ADC_DMA_BUFFER_SIZE / 2]);
  }
}

void FEB_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    ADC_AccumulateHalf(&adc1_dma_buffer[0]);
  }
}

void FEB_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
  adc_error_count++;
  if (hadc->Instance == ADC1)
  {
    __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_OVR);
    HAL_ADC_Stop_DMA(hadc);
    HAL_ADC_Start_DMA(hadc, (uint32_t *)(uintptr_t)adc1_dma_buffer, ADC_DMA_BUFFER_SIZE);
  }
}
