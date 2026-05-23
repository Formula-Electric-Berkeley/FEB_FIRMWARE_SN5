/**
 ******************************************************************************
 * @file           : FEB_LinearPotentiometer.h
 * @brief          : Linear potentiometer driver (ADC1, 2 channels).
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_LINEAR_POTENTIOMETER_H
#define FEB_LINEAR_POTENTIOMETER_H

#include <stdint.h>

/* ============================================================================
 * Per-pot calibration — placeholder defaults; tune to physical pots.
 *
 *   RAW_MIN   : ADC count when pot is fully retracted (corresponds to 0 mm).
 *   RAW_MAX   : ADC count when pot is fully extended  (corresponds to LENGTH_MM).
 *   LENGTH_MM : usable travel length in millimetres.
 *
 * Swap RAW_MIN / RAW_MAX if the wired-up direction is reversed.
 * Displacement is clamped to [0, LENGTH_MM] before publication.
 * ============================================================================ */
#define FEB_LP_1_RAW_MIN 0u
#define FEB_LP_1_RAW_MAX 4095u
#define FEB_LP_1_LENGTH_MM 75.0f

#define FEB_LP_2_RAW_MIN 0u
#define FEB_LP_2_RAW_MAX 4095u
#define FEB_LP_2_LENGTH_MM 75.0f

/* Latest readings, populated by read_LinearPotentiometer().
 * Indexed [0] = LP_Wiper1 (PC3 / ADC1_IN13), [1] = LP_Wiper2 (PB1 / ADC1_IN9). */
extern uint16_t lp_raw[2];
extern float lp_displacement_mm[2];

void FEB_LinearPotentiometer_Init(void);
void read_LinearPotentiometer(void);

#endif /* FEB_LINEAR_POTENTIOMETER_H */
