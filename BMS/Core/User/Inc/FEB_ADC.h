#ifndef INC_FEB_ADC_H_
#define INC_FEB_ADC_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* DMA circular buffer: 834 samples total (417 per half-buffer)
 * At 833.3 kSPS (1.2 us/sample), 417 samples = 500.4 us -> ~1,998 Hz ISR rate */
#define ADC_DMA_BUFFER_SIZE 834
#define ADC_WINDOW_TICKS 10

/* Resistor divider: 220k top (to 24V), 22k bottom (to GND)
 * Attenuation: 22k / (220k + 22k) = 1 / 11.0
 * Multiplier: (220k + 22k) / 22k = 11.0x */
#define VOLTAGE_DIVIDER_RATIO_SHUTDOWN ((220.0f + 22.0f) / 22.0f)

  typedef enum
  {
    ADC_STATUS_OK = 0,
    ADC_STATUS_ERROR,
    ADC_STATUS_BUSY,
    ADC_STATUS_TIMEOUT,
    ADC_STATUS_NOT_INITIALIZED
  } ADC_StatusTypeDef;

  ADC_StatusTypeDef FEB_ADC_Init(void);
  ADC_StatusTypeDef FEB_ADC_Start(void);
  ADC_StatusTypeDef FEB_ADC_Stop(void);
  void FEB_ADC_Tick(void);

  void FEB_ADC_GetShutdownNoiseVoltage(float *min_v, float *max_v);
  void FEB_ADC_GetShutdownNoiseRaw(uint16_t *min_raw, uint16_t *max_raw);
  float FEB_ADC_GetShutdownNoisePeakToPeak(void);
  float FEB_ADC_GetShutdownVoltage(void);

  float FEB_ADC_RawToVoltage(uint16_t raw_value);
  uint32_t FEB_ADC_RawToMillivolts(uint16_t raw_value);

  void FEB_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);
  void FEB_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc);
  void FEB_ADC_ErrorCallback(ADC_HandleTypeDef *hadc);

#ifdef __cplusplus
}
#endif

#endif /* INC_FEB_ADC_H_ */
