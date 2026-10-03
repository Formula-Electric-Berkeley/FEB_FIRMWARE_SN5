/**
 ******************************************************************************
 * @file           : FEB_Thermistor.h
 * @brief          : Coolant NTC thermistor driver (ADC1, 3 channels).
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_THERMISTOR_H
#define FEB_THERMISTOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/*   [0] = Thermocouple1 (PC2 / ADC1_IN12)
 *   [1] = Thermocouple2 (PC1 / ADC1_IN11)
 *   [2] = Thermocouple3 (PC0 / ADC1_IN10) */
#define FEB_TH_COUNT 3

#define FEB_TH_STATUS_OPEN(i) (1u << (i))
#define FEB_TH_STATUS_SHORT(i) (1u << ((i) + FEB_TH_COUNT))

  typedef struct
  {
    uint32_t adc_channel;
    float r_pullup_ohm;
    float r0_ohm;
    float beta_k;
    float t0_c;
  } FEB_TH_Cal_t;

  extern uint16_t th_raw[FEB_TH_COUNT];
  extern float th_resistance_ohm[FEB_TH_COUNT];
  extern float th_temp_c[FEB_TH_COUNT];
  extern uint8_t th_status;

  void FEB_Thermistor_Init(void);
  void read_Thermistor(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_THERMISTOR_H */
