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

/* Number of linear-pot wiper inputs wired on the board:
 *   [0] = LP_Wiper1 (PC3 / ADC1_IN13) — LEFT  corner
 *   [1] = LP_Wiper2 (PB1 / ADC1_IN9)  — RIGHT corner
 * FRONT and REAR are separate binaries, so two pots per node cover all four
 * corners (front-left/right on the FRONT build, rear-left/right on REAR). */
#define FEB_LP_COUNT 2

/* ============================================================================
 * Per-pot calibration — one struct per wiper, edited in FEB_LinearPotentiometer.c.
 *
 * The wiper is calibrated by two points: a start and an end. For each point you
 * record the raw 12-bit ADC count and the physical position (mm) it represents.
 * Firmware linearly interpolates between them and clamps to the pot's mechanical
 * stroke (total_length_mm), so the value published on CAN is an absolute
 * position in millimetres.
 *
 *   raw_at_start / start_mm : ADC count + physical position at the start point.
 *   raw_at_end   / end_mm   : ADC count + physical position at the end point.
 *   total_length_mm         : full mechanical stroke; output is clamped to
 *                             [0, total_length_mm].
 *
 * A reversed wiper (raw decreases as it extends) is handled automatically — just
 * record raw_at_start > raw_at_end. For plain "displacement from one end" set
 * start_mm = 0 and end_mm = total_length_mm.
 * ============================================================================ */
typedef struct
{
  uint32_t adc_channel;  /* ADC1 channel feeding this wiper          */
  uint16_t raw_at_start; /* ADC count at the start position          */
  float start_mm;        /* physical position at raw_at_start [mm]   */
  uint16_t raw_at_end;   /* ADC count at the end position            */
  float end_mm;          /* physical position at raw_at_end [mm]      */
  float total_length_mm; /* full mechanical stroke [mm] (output clamp)*/
} FEB_LP_Cal_t;

/* Latest readings, populated by read_LinearPotentiometer().
 * Indexed [0] = Left (PC3 / ADC1_IN13), [1] = Right (PB1 / ADC1_IN9). */
extern uint16_t lp_raw[FEB_LP_COUNT];
extern float lp_position_mm[FEB_LP_COUNT];

void FEB_LinearPotentiometer_Init(void);
void read_LinearPotentiometer(void);

#endif /* FEB_LINEAR_POTENTIOMETER_H */
