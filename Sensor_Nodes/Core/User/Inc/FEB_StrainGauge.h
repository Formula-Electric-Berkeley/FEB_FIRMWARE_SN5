/**
 ******************************************************************************
 * @file           : FEB_StrainGauge.h
 * @brief          : Strain Gauge driver (ADC1, 2 channels).
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_STRAIN_GAUGE_H
#define FEB_STRAIN_GAUGE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/*
  Control statement to determine number of strain gauges
*/
#if FEB_SN_IS_FRONT
#define FEB_SG_COUNT 4
#else
#define FEB_SG_COUNT 3
#endif

  typedef struct
  {
    uint32_t adc_channel; /* ADC1 channel feeding this strain guage   */
    float resistance_offset;
    float start_voltage_mv;
    float excitation_voltage_mv;
    float sensitivity_rating;
  } FEB_SG_CAL_t;

  extern uint16_t sg_raw[FEB_SG_COUNT];
  extern float sg_reading_N[FEB_SG_COUNT];

  void FEB_StrainGauge_Init(void);
  void read_StrainGauge(void);
  float get_strain_gauge(int i);

#ifdef __cplusplus
}
#endif

#endif /* FEB_STRAINGAUGE_H */
