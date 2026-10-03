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

/* Number of linear-pot wiper inputs wired on the board:
 *   [0] = LP_Wiper1 (PC3 / ADC1_IN13) — LEFT  corner
 *   [1] = LP_Wiper2 (PB1 / ADC1_IN9)  — RIGHT corner
 * FRONT and REAR are separate binaries, so two pots per node cover all four
 * corners (front-left/right on the FRONT build, rear-left/right on REAR). */
#define FEB_SG_COUNT 1
  // do i need to do this for the rear and back?

  typedef struct
  {
    uint32_t adc_channel; /* ADC1 channel feeding this strain guage   */
    float resistance_offset
  } FEB_SG_CAL_t;

  /* Latest readings, populated by read_LinearPotentiometer().
   * Indexed [0] = Left (PC3 / ADC1_IN13), [1] = Right (PB1 / ADC1_IN9). */
  extern uint16_t sg_raw[1];
  extern float sg_reading_N[1];

  void FEB_StrainGauge_Init(void);
  void read_StrainGauge(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_STRAINGAUGE_H */
