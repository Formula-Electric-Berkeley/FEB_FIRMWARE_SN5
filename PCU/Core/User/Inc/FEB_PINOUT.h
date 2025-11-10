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
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ========================================================================== */
/*                          ADC CHANNEL DEFINITIONS                          */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/*                            ADC1 CHANNELS                                  */
/* -------------------------------------------------------------------------- */
#define ADC1_BRAKE_INPUT_CHANNEL           ADC_CHANNEL_14  /* PC4 - Brake Input Signal */
#define ADC1_BRAKE_INPUT_PIN                GPIO_PIN_4
#define ADC1_BRAKE_INPUT_PORT               GPIOC

#define ADC1_ACCEL_PEDAL_1_CHANNEL         ADC_CHANNEL_0   /* PA0 - Accelerator Pedal Position 1 */
#define ADC1_ACCEL_PEDAL_1_PIN             GPIO_PIN_0
#define ADC1_ACCEL_PEDAL_1_PORT            GPIOA

#define ADC1_ACCEL_PEDAL_2_CHANNEL         ADC_CHANNEL_1   /* PA1 - Accelerator Pedal Position 2 */
#define ADC1_ACCEL_PEDAL_2_PIN             GPIO_PIN_1
#define ADC1_ACCEL_PEDAL_2_PORT            GPIOA

/* -------------------------------------------------------------------------- */
/*                            ADC2 CHANNELS                                  */
/* -------------------------------------------------------------------------- */
#define ADC2_CURRENT_SENSE_CHANNEL         ADC_CHANNEL_4   /* PA4 - Current Sensing */
#define ADC2_CURRENT_SENSE_PIN             GPIO_PIN_4
#define ADC2_CURRENT_SENSE_PORT            GPIOA

#define ADC2_PRE_TIMING_TRIP_CHANNEL       ADC_CHANNEL_7   /* PA7 - Pre-timing Trip Sense */
#define ADC2_PRE_TIMING_TRIP_PIN           GPIO_PIN_7
#define ADC2_PRE_TIMING_TRIP_PORT          GPIOA

#define ADC2_SHUTDOWN_IN_CHANNEL           ADC_CHANNEL_6   /* PA6 - Shutdown Circuit Input */
#define ADC2_SHUTDOWN_IN_PIN               GPIO_PIN_6
#define ADC2_SHUTDOWN_IN_PORT              GPIOA

/* -------------------------------------------------------------------------- */
/*                            ADC3 CHANNELS                                  */
/* -------------------------------------------------------------------------- */
#define ADC3_BSPD_INDICATOR_CHANNEL        ADC_CHANNEL_8   /* PC0 - BSPD Indicator */
#define ADC3_BSPD_INDICATOR_PIN            GPIO_PIN_0
#define ADC3_BSPD_INDICATOR_PORT           GPIOC

#define ADC3_BSPD_RESET_CHANNEL            ADC_CHANNEL_9   /* PC1 - BSPD Reset Signal */
#define ADC3_BSPD_RESET_PIN                GPIO_PIN_1
#define ADC3_BSPD_RESET_PORT               GPIOC

#define ADC3_BRAKE_PRESSURE_1_CHANNEL      ADC_CHANNEL_12  /* PC2 - Brake Pressure Sensor 1 */
#define ADC3_BRAKE_PRESSURE_1_PIN          GPIO_PIN_2
#define ADC3_BRAKE_PRESSURE_1_PORT         GPIOC

#define ADC3_BRAKE_PRESSURE_2_CHANNEL      ADC_CHANNEL_13  /* PC3 - Brake Pressure Sensor 2 */
#define ADC3_BRAKE_PRESSURE_2_PIN          GPIO_PIN_3
#define ADC3_BRAKE_PRESSURE_2_PORT         GPIOC

/* ========================================================================== */
/*                         ADC CONFIGURATION PARAMETERS                       */
/* ========================================================================== */

/* ADC Reference and Resolution */
#define ADC_REFERENCE_VOLTAGE_MV           3300    /* 3.3V in millivolts */
#define ADC_RESOLUTION_BITS                12      /* 12-bit ADC */
#define ADC_MAX_VALUE                      4095    /* 2^12 - 1 */
#define ADC_VREF_VOLTAGE                   3.3f    /* Reference voltage in volts */

/* ADC Sampling Configuration */
#define ADC_DEFAULT_SAMPLING_TIME          ADC_SAMPLETIME_3CYCLES
#define ADC_EXTENDED_SAMPLING_TIME         ADC_SAMPLETIME_15CYCLES
#define ADC_MAX_SAMPLING_TIME              ADC_SAMPLETIME_480CYCLES

/* DMA Buffer Sizes */
#define ADC_DMA_BUFFER_SIZE                16      /* Size of DMA circular buffer */
#define ADC_AVERAGING_SAMPLES              8       /* Number of samples for averaging */

/* ========================================================================== */
/*                         FILTER CONFIGURATION PARAMETERS                    */
/* ========================================================================== */

/* Default filter settings for different sensor types */
#define FILTER_BRAKE_INPUT_ENABLED        1       /* Enable filtering for brake input */
#define FILTER_BRAKE_INPUT_SAMPLES        8       /* Number of samples to average */
#define FILTER_BRAKE_INPUT_ALPHA          0.75f   /* Low-pass filter coefficient */

#define FILTER_BRAKE_PRESSURE_ENABLED     1       /* Enable filtering for pressure sensors */
#define FILTER_BRAKE_PRESSURE_SAMPLES     12      /* More samples for stable pressure reading */
#define FILTER_BRAKE_PRESSURE_ALPHA       0.85f   /* Higher alpha for smoother pressure */

#define FILTER_ACCEL_PEDAL_ENABLED        1       /* Enable filtering for APPS */
#define FILTER_ACCEL_PEDAL_SAMPLES        6       /* Moderate filtering for responsiveness */
#define FILTER_ACCEL_PEDAL_ALPHA          0.65f   /* Balance between response and smoothness */

#define FILTER_CURRENT_SENSE_ENABLED      1       /* Enable filtering for current sensor */
#define FILTER_CURRENT_SENSE_SAMPLES      16      /* Maximum filtering for noisy current */
#define FILTER_CURRENT_SENSE_ALPHA        0.9f    /* Heavy filtering for current measurement */

#define FILTER_SHUTDOWN_ENABLED            1       /* Enable filtering for shutdown monitoring */
#define FILTER_SHUTDOWN_SAMPLES            4       /* Light filtering for safety signals */
#define FILTER_SHUTDOWN_ALPHA              0.7f    /* Moderate smoothing */

#define FILTER_BSPD_ENABLED                0       /* No filtering for digital BSPD signals */
#define FILTER_BSPD_SAMPLES                1       /* Single sample for digital signals */
#define FILTER_BSPD_ALPHA                  1.0f    /* No low-pass filtering */

/* ========================================================================== */
/*                         SENSOR CALIBRATION PARAMETERS                      */
/* ========================================================================== */

/* Default Calibration Values - Used until runtime calibration is performed */
/* These are DEFAULTS only - actual values are stored in calibration structs */

/* Accelerator Pedal Default Calibration (APPS) */
#define APPS1_DEFAULT_MIN_VOLTAGE_MV       1530     /* Default min voltage for APPS1 (0% position) */
#define APPS1_DEFAULT_MAX_VOLTAGE_MV       2065    /* Default max voltage for APPS1 (100% position) */
#define APPS2_DEFAULT_MIN_VOLTAGE_MV       1530     /* Default min voltage for APPS2 (0% position) */
#define APPS2_DEFAULT_MAX_VOLTAGE_MV       2065    /* Default max voltage for APPS2 (100% position) */
#define APPS_MIN_PHYSICAL_PERCENT          0.0f    /* Physical minimum: 0% throttle */
#define APPS_MAX_PHYSICAL_PERCENT          100.0f  /* Physical maximum: 100% throttle */
#define APPS_DEADZONE_PERCENT              5       /* Deadzone at pedal extremes (%) */
#define APPS_PLAUSIBILITY_TOLERANCE        10      /* Maximum deviation between sensors (%) */

/* Brake Pressure Sensor Default Calibration */
#define BRAKE_PRESSURE_DEFAULT_MIN_MV      500     /* Default voltage at 0 bar */
#define BRAKE_PRESSURE_DEFAULT_MAX_MV      4500    /* Default voltage at max pressure */
#define BRAKE_PRESSURE_MIN_PHYSICAL_BAR    0.0f    /* Physical minimum: 0 bar */
#define BRAKE_PRESSURE_MAX_PHYSICAL_BAR    200.0f  /* Physical maximum: 200 bar */
#define BRAKE_PRESSURE_THRESHOLD_BAR       5       /* Brake activation threshold */
#define BRAKE_PRESSURE_THRESHOLD_PERCENT   2.5f    /* Brake activation threshold in percent */

/* Brake Input/Switch Calibration */
#define BRAKE_INPUT_THRESHOLD_MV           1500    /* Threshold for brake switch activation */
#define BRAKE_INPUT_HYSTERESIS_MV          200     /* Hysteresis for noise immunity */

/* Current Sensor Calibration */
#define CURRENT_SENSOR_ZERO_MV             2500    /* Zero current voltage (bidirectional sensor) */
#define CURRENT_SENSOR_SENSITIVITY_MV_A    66      /* Sensor sensitivity in mV/A */
#define CURRENT_SENSOR_MAX_AMPS            50      /* Maximum measurable current */

/* Voltage Divider Ratios */
#define SHUTDOWN_VOLTAGE_DIVIDER_RATIO     11.0f   /* (R1+R2)/R2 for shutdown circuit monitoring */
#define BSPD_VOLTAGE_DIVIDER_RATIO         5.5f    /* Voltage divider for BSPD signals */

/* ========================================================================== */
/*                            SAFETY THRESHOLDS                               */
/* ========================================================================== */

/* FSAE Rules Compliance */
#define APPS_IMPLAUSIBILITY_TIME_MS        100     /* Time before APPS implausibility fault */
#define BRAKE_PLAUSIBILITY_TIME_MS         100     /* Time before brake plausibility fault */
#define APPS_SHORT_CIRCUIT_DETECT_MV       100     /* Voltage below this indicates short */
#define APPS_OPEN_CIRCUIT_DETECT_MV        4900    /* Voltage above this indicates open */

/* Brake Over Travel Protection (BOTS) */
#define BOTS_ACTIVATION_PERCENT            25      /* Brake travel % that triggers BOTS */
#define BOTS_RESET_PERCENT                 5       /* Brake must be below this to reset */

/* ADC Watchdog Thresholds */
#define ADC_WATCHDOG_LOW_THRESHOLD         100     /* Minimum valid ADC reading */
#define ADC_WATCHDOG_HIGH_THRESHOLD        4000    /* Maximum valid ADC reading */

/* ========================================================================== */
/*                           GPIO PIN DEFINITIONS                             */
/* ========================================================================== */

/* Digital Outputs */
#define BSPD_LED_PIN                       GPIO_PIN_5
#define BSPD_LED_PORT                      GPIOB

#define READY_TO_DRIVE_LED_PIN             GPIO_PIN_6
#define READY_TO_DRIVE_LED_PORT            GPIOB

/* Digital Inputs */
#define DRIVE_BUTTON_PIN                   GPIO_PIN_7
#define DRIVE_BUTTON_PORT                  GPIOB
#define BSPD_RESET_PIN                     GPIO_PIN_1
#define BSPD_RESET_PORT                    GPIOC

/* CAN Bus Pins */
#define CAN1_RX_PIN                        GPIO_PIN_11
#define CAN1_RX_PORT                       GPIOA
#define CAN1_TX_PIN                        GPIO_PIN_12
#define CAN1_TX_PORT                       GPIOA

/* I2C Pins (for external sensors) */
#define I2C1_SCL_PIN                       GPIO_PIN_8
#define I2C1_SCL_PORT                      GPIOB
#define I2C1_SDA_PIN                       GPIO_PIN_9
#define I2C1_SDA_PORT                      GPIOB

/* UART Debug Port */
#define UART2_TX_PIN                       GPIO_PIN_2
#define UART2_TX_PORT                      GPIOA
#define UART2_RX_PIN                       GPIO_PIN_3
#define UART2_RX_PORT                      GPIOA

#ifdef __cplusplus
}
#endif

#endif /* __FEB_PINOUT_H */