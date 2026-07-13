/**
 ******************************************************************************
 * @file           : pinout.h
 * @brief          : PCU hardware pinout definitions and ADC channel mappings
 ******************************************************************************
 * @attention
 *
 * This file contains all hardware pin definitions for the PCU board,
 * including ADC channel assignments, GPIO mappings, and sensor connections.
 *
 ******************************************************************************
 */

#ifndef __FEB_PINOUT_H
#define __FEB_PINOUT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32f4xx_hal.h"

/* ========================================================================== */
/*                          ADC CHANNEL DEFINITIONS                          */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/*                            ADC1 CHANNELS                                  */
/* -------------------------------------------------------------------------- */
#define ADC1_BRAKE_INPUT_CHANNEL ADC_CHANNEL_14 /* PC4 - Brake Input Signal */
#define ADC1_BRAKE_INPUT_PIN GPIO_PIN_4
#define ADC1_BRAKE_INPUT_PORT GPIOC

#define ADC1_BRAKE_PRESSURE_1_CHANNEL ADC_CHANNEL_1 /* PA1 - Brake Pressure Sensor 1 (BP1) */
#define ADC1_BRAKE_PRESSURE_1_PIN GPIO_PIN_1
#define ADC1_BRAKE_PRESSURE_1_PORT GPIOA

#define ADC1_BRAKE_PRESSURE_2_CHANNEL ADC_CHANNEL_0 /* PA0 - Brake Pressure Sensor 2 (BP2) */
#define ADC1_BRAKE_PRESSURE_2_PIN GPIO_PIN_0
#define ADC1_BRAKE_PRESSURE_2_PORT GPIOA

/* -------------------------------------------------------------------------- */
/*                            ADC2 CHANNELS                                  */
/* -------------------------------------------------------------------------- */
#define ADC2_CURRENT_SENSE_CHANNEL ADC_CHANNEL_4 /* PA4 - Current Sensing */
#define ADC2_CURRENT_SENSE_PIN GPIO_PIN_4
#define ADC2_CURRENT_SENSE_PORT GPIOA

#define ADC2_PRE_TIMING_TRIP_CHANNEL ADC_CHANNEL_7 /* PA7 - Pre-timing Trip Sense */
#define ADC2_PRE_TIMING_TRIP_PIN GPIO_PIN_7
#define ADC2_PRE_TIMING_TRIP_PORT GPIOA

#define ADC2_SHUTDOWN_IN_CHANNEL ADC_CHANNEL_6 /* PA6 - Shutdown Circuit Input */
#define ADC2_SHUTDOWN_IN_PIN GPIO_PIN_6
#define ADC2_SHUTDOWN_IN_PORT GPIOA

/* -------------------------------------------------------------------------- */
/*                            ADC3 CHANNELS                                  */
/* -------------------------------------------------------------------------- */
#define ADC3_BSPD_INDICATOR_CHANNEL ADC_CHANNEL_10 /* PC0 - BSPD Indicator (ADC123_IN10) */
#define ADC3_BSPD_INDICATOR_PIN GPIO_PIN_0
#define ADC3_BSPD_INDICATOR_PORT GPIOC

#define ADC3_BSPD_RESET_CHANNEL ADC_CHANNEL_11 /* PC1 - BSPD Reset Signal (ADC123_IN11) */
#define ADC3_BSPD_RESET_PIN GPIO_PIN_1
#define ADC3_BSPD_RESET_PORT GPIOC

#define ADC3_ACCEL_PEDAL_1_CHANNEL ADC_CHANNEL_13 /* PC3 - Accelerator Pedal Position 1 */
#define ADC3_ACCEL_PEDAL_1_PIN GPIO_PIN_3
#define ADC3_ACCEL_PEDAL_1_PORT GPIOC

#define ADC3_ACCEL_PEDAL_2_CHANNEL ADC_CHANNEL_12 /* PC2 - Accelerator Pedal Position 2 */
#define ADC3_ACCEL_PEDAL_2_PIN GPIO_PIN_2
#define ADC3_ACCEL_PEDAL_2_PORT GPIOC

/* ========================================================================== */
/*                         ADC CONFIGURATION PARAMETERS                       */
/* ========================================================================== */

/* ADC Reference and Resolution */
#define ADC_REFERENCE_VOLTAGE_MV 3300 /* 3.3V in millivolts */
#define ADC_RESOLUTION_BITS 12        /* 12-bit ADC */
#define ADC_MAX_VALUE 4095            /* 2^12 - 1 */
#define ADC_VREF_VOLTAGE 3.3f         /* Reference voltage in volts */

/* ADC Sampling Configuration */
#define ADC_DEFAULT_SAMPLING_TIME ADC_SAMPLETIME_3CYCLES
#define ADC_EXTENDED_SAMPLING_TIME ADC_SAMPLETIME_15CYCLES
#define ADC_MAX_SAMPLING_TIME ADC_SAMPLETIME_480CYCLES

/* DMA Buffer Sizes */
#define ADC_DMA_BUFFER_SIZE 16  /* Circular buffer depth (samples per channel) */
#define ADC_AVERAGING_SAMPLES 8 /* Number of samples for averaging */

/* TIM2 hardware trigger rate for all three ADCs. Each TIM2 TRGO edge launches
 * one full scan per ADC, so this is the per-channel oversample rate and the
 * basis for time-coherent sampling (APPS1/APPS2, brake1/brake2 and APPS-vs-brake
 * all start on the same edge). The per-1ms control snapshot boxcar-averages
 * roughly ADC_OVERSAMPLE_HZ/1000 of these samples. Configured in PCU.ioc
 * (TIM2 PSC=89, ARR=99 on the 90 MHz APB1 timer clock => 10 kHz). */
#define ADC_OVERSAMPLE_HZ 10000

/* ========================================================================== */
/*                         FILTER CONFIGURATION PARAMETERS                    */
/* ========================================================================== */

/* Boxcar (moving-average) window per sensor class. ADC samples are hardware-
 * triggered by TIM2 (ADC_OVERSAMPLE_HZ), so the most-recent N samples per
 * channel are time-coherent across all three ADCs; the per-1ms snapshot
 * (FEB_ADC_TickSample) averages the most-recent SAMPLES values. NO IIR / low-
 * pass filtering is used — a boxcar is symmetric, so both members of a sensor
 * pair (APPS1/2, brake1/2) share one window with identical group delay and
 * filtering can never manufacture an artificial inter-sensor deviation. SAMPLES
 * is clamped to [1, ADC_DMA_BUFFER_SIZE]; ENABLED=0 forces a single most-recent
 * sample. */
#define FILTER_BRAKE_INPUT_ENABLED 1 /* Enable averaging for brake input */
#define FILTER_BRAKE_INPUT_SAMPLES 8 /* Boxcar window (samples) */

#define FILTER_BRAKE_PRESSURE_ENABLED 1  /* Enable averaging for pressure sensors */
#define FILTER_BRAKE_PRESSURE_SAMPLES 12 /* Both brake sensors share this window */

#define FILTER_ACCEL_PEDAL_ENABLED 1 /* Enable averaging for APPS */
#define FILTER_ACCEL_PEDAL_SAMPLES 6 /* Both APPS sensors share this window */

#define FILTER_CURRENT_SENSE_ENABLED 1  /* Enable averaging for current sensor */
#define FILTER_CURRENT_SENSE_SAMPLES 16 /* Maximum averaging for noisy current */

#define FILTER_SHUTDOWN_ENABLED 1 /* Enable averaging for shutdown monitoring */
#define FILTER_SHUTDOWN_SAMPLES 4 /* Light averaging for safety signals */

#define FILTER_BSPD_ENABLED 0 /* No averaging for digital BSPD signals */
#define FILTER_BSPD_SAMPLES 1 /* Single sample for digital signals */

/* ========================================================================== */
/*                         SENSOR CALIBRATION PARAMETERS                      */
/* ========================================================================== */

/* Default Calibration Values - Used until runtime calibration is performed */
/* These are DEFAULTS only - actual values are stored in calibration structs */

/* Single-APPS mode is now a runtime flag (FEB_APPS_SingleSensorMode in FEB_ADC.c).
 * Default at boot is OFF — dual-sensor plausibility enforced (FSAE T.4.2.2).
 * Toggle for bench testing only via PCU|apps|mode|single|<on|off> while not in
 * drive state. */

/* Accelerator Pedal Calibration (APPS) - per-car values, post-divider
 * (sensor-side) mV: the domain FEB_ADC maps against (APPS1 = ADC pin x1.168,
 * APPS2 = pin x1.0; see VOLTAGE_DIVIDER_RATIO_ACCEL* in FEB_ADC.c). */
#define APPS1_DEFAULT_MIN_VOLTAGE_MV 1375 /* APPS1 0% throttle */
#define APPS1_DEFAULT_MAX_VOLTAGE_MV 2356 /* APPS1 100% throttle */
#define APPS2_DEFAULT_MIN_VOLTAGE_MV 539  /* APPS2 0% throttle */
#define APPS2_DEFAULT_MAX_VOLTAGE_MV 1320 /* APPS2 100% throttle */
#define APPS_MIN_PHYSICAL_PERCENT 0.0f    /* Physical minimum: 0% throttle */
#define APPS_MAX_PHYSICAL_PERCENT 100.0f  /* Physical maximum: 100% throttle */
#define APPS_DEADZONE_PERCENT 5           /* Deadzone at pedal extremes (%) */
#define APPS_PLAUSIBILITY_TOLERANCE 10    /* Maximum deviation between sensors (%) */

/* FSAE T.4.2.3: the two APPS use different transfer functions, so their RAW
 * pin-domain outputs (raw/ADC_MAX_VALUE*100) stay >=~23% apart across the pedal
 * range on SN5. If the two signal lines short together that designed separation
 * collapses -> treat as implausibility (an "other failure defined in T.4.2").
 * Reference is ADC full scale (4095 cnt = 3300 mV at the pin); 10 here = 10
 * percentage points of full scale. Keep at 10 — larger needs ETC justification. */
#define APPS_MIN_SEPARATION_PERCENT 10        /* Min raw pin-domain separation, % of ADC FS */
#define APPS_SEPARATION_PEDAL_GATE_PERCENT 10 /* Arm the separation check above this pedal % */

/* Brake Pressure Sensor Default Calibration — per-sensor, sensor-side mV
 * (i.e. before the 5V->3.3V PCB divider; FEB_ADC_GetBrakePressureNVoltage()
 * already multiplies by VOLTAGE_DIVIDER_RATIO_BRAKE to give sensor-side V). */
#define BRAKE_PRESSURE_1_MIN_MV 465                         /* Sensor 1 @ 0% brake: */
#define BRAKE_PRESSURE_1_MAX_MV 1130                        /* Sensor 1 @ 100% brake: */
#define BRAKE_PRESSURE_2_MIN_MV 555                         /* Sensor 2 @ 0% brake: */
#define BRAKE_PRESSURE_2_MAX_MV 1455                        /* Sensor 2 @ 100% brake: */
#define BRAKE_PRESSURE_MIN_PHYSICAL_BAR 0.0f                /* Physical minimum: 0 bar */
#define BRAKE_PRESSURE_MAX_PHYSICAL_BAR 200.0f              /* Physical maximum: 200 bar */
#define BRAKE_PRESSURE_THRESHOLD_BAR 5                      /* Brake activation threshold */
#define BRAKE_PRESSURE_THRESHOLD_PERCENT 2.5f               /* Brake activation threshold in percent */
#define BRAKE_PRESSURE_PLAUSIBILITY_TOLERANCE_PERCENT 20.0f /* Max disagreement between brake pressure sensors */
#define BRAKE_PRESSED_POSITION_PERCENT 10.0f                /* Brake "pressed" when combined position exceeds this */

/* Brake Input/Switch Calibration */
#define BRAKE_INPUT_THRESHOLD_MV 1500 /* Threshold for brake switch activation */
#define BRAKE_INPUT_HYSTERESIS_MV 200 /* Hysteresis for noise immunity */

/* Current Sensor Calibration */
#define CURRENT_SENSOR_ZERO_MV 2500        /* Zero current voltage (bidirectional sensor) */
#define CURRENT_SENSOR_SENSITIVITY_MV_A 66 /* Sensor sensitivity in mV/A */
#define CURRENT_SENSOR_MAX_AMPS 50         /* Maximum measurable current */

/* Voltage Divider Ratios */
#define SHUTDOWN_VOLTAGE_DIVIDER_RATIO 11.0f /* (R1+R2)/R2 for shutdown circuit monitoring */
#define BSPD_VOLTAGE_DIVIDER_RATIO 5.5f      /* Voltage divider for BSPD signals */

/* ========================================================================== */
/*                            SAFETY THRESHOLDS                               */
/* ========================================================================== */

/* FSAE Rules Compliance */
#define APPS_IMPLAUSIBILITY_TIME_MS 100 /* T.4.2.4: APPS >10% disagreement must persist >100ms to fault */
#define BRAKE_PLAUSIBILITY_TIME_MS 100  /* T.4.3.4: BSE sensor open/short must persist >100ms to fault */
/* EV.4.7 (brakes engaged + APPS >25%) must stop motor power IMMEDIATELY — the
 * 100 ms grace above does NOT apply to it (that is T.4.2.4 / T.4.3.4 only). Use
 * a short noise-rejection debounce so a single noisy frame can't false-trip. */
#define EV47_PLAUSIBILITY_DEBOUNCE_MS 10 /* EV.4.7 brake+throttle debounce (effectively immediate) */
#define APPS_RELEASE_PERCENT 5.0f        /* EV.4.7.2.b / T.4.2.4 release: APPS travel below this clears the latch */

/* APPS short/open detection (T.4.2.10/.11/.13): a sensor has "failed" when its
 * post-divider voltage is outside its normal operating range. Thresholds are
 * PER-SENSOR — the two ranges differ, and APPS2's 0% sits at ~0.40 V (below the
 * rule's generic <0.5 V example), so a single global floor cannot be used.
 * Floors sit below each valid min, ceilings above each valid max, both with
 * margin and inside the recoverable range (APPS1 ceiling 3.3V*1.168=3854mV,
 * APPS2 3300mV). High-side over-range can't make phantom torque because the
 * torque command uses MIN(APPS1,APPS2). RE-VERIFY after every recalibration so
 * the margins still clear filter ripple at 0% and 100% pedal. */
#define APPS1_SHORT_CIRCUIT_DETECT_MV 1000 /* APPS1 below this = short/under-range */
#define APPS2_SHORT_CIRCUIT_DETECT_MV 250  /* APPS2 below this = short/under-range */
#define APPS1_OPEN_CIRCUIT_DETECT_MV 2800  /* APPS1 above this = open/over-range */
#define APPS2_OPEN_CIRCUIT_DETECT_MV 1300  /* APPS2 above this = open/over-range */

/* BSE (brake) open/short detection (T.4.3.4/.5): per-sensor, sensor-side mV
 * (post 5V->3.3V divider; recoverable ceiling ~5000 mV). Floors catch a short-
 * to-ground or a lost shared 5 V supply (both sensors read ~0 V — the dangerous
 * "looks like no brake" case); ceilings catch a short to 5 V. Open-circuit
 * detectability REQUIRES a pull resistor on each brake line (confirm in HW).
 * RE-VERIFY against the brake sensors' real 0%/100% voltages. */
#define BRAKE_PRESSURE_1_UNDER_MV 350 /* Brake P1 below this = short/lost supply */
#define BRAKE_PRESSURE_1_OVER_MV 4600 /* Brake P1 above this = open/short-to-5V */
#define BRAKE_PRESSURE_2_UNDER_MV 300 /* Brake P2 below this = short/lost supply */
#define BRAKE_PRESSURE_2_OVER_MV 4600 /* Brake P2 above this = open/short-to-5V */

/* Brake Over Travel Protection (BOTS) */
#define BOTS_ACTIVATION_PERCENT 25 /* Brake travel % that triggers BOTS */
#define BOTS_RESET_PERCENT 5       /* Brake must be below this to reset */

/* ADC Watchdog Thresholds */
#define ADC_WATCHDOG_LOW_THRESHOLD 100   /* Minimum valid ADC reading */
#define ADC_WATCHDOG_HIGH_THRESHOLD 4000 /* Maximum valid ADC reading */

/* ========================================================================== */
/*                           GPIO PIN DEFINITIONS                             */
/* ========================================================================== */

/* Digital Outputs */
#define BSPD_LED_PIN GPIO_PIN_5
#define BSPD_LED_PORT GPIOB

#define READY_TO_DRIVE_LED_PIN GPIO_PIN_6
#define READY_TO_DRIVE_LED_PORT GPIOB

/* Digital Inputs */
#define DRIVE_BUTTON_PIN GPIO_PIN_7
#define DRIVE_BUTTON_PORT GPIOB
#define BSPD_RESET_PIN GPIO_PIN_1
#define BSPD_RESET_PORT GPIOC

/* CAN Bus Pins */
#define CAN1_RX_PIN GPIO_PIN_11
#define CAN1_RX_PORT GPIOA
#define CAN1_TX_PIN GPIO_PIN_12
#define CAN1_TX_PORT GPIOA

/* I2C Pins (for external sensors) */
#define I2C1_SCL_PIN GPIO_PIN_8
#define I2C1_SCL_PORT GPIOB
#define I2C1_SDA_PIN GPIO_PIN_9
#define I2C1_SDA_PORT GPIOB

/* UART Debug Port */
#define UART2_TX_PIN GPIO_PIN_2
#define UART2_TX_PORT GPIOA
#define UART2_RX_PIN GPIO_PIN_3
#define UART2_RX_PORT GPIOA

#ifdef __cplusplus
}
#endif

#endif /* __FEB_PINOUT_H */
