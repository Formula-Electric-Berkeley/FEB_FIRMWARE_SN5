/**
  ******************************************************************************
  * @file           : FEB_RMS_Config.h
  * @brief          : RMS motor controller configuration constants
  ******************************************************************************
  * @attention
  *
  * This file contains configuration parameters for the RMS motor controller
  * including torque limits, current limits, and conversion factors.
  *
  * IMPORTANT: Adjust these values based on your specific motor and accumulator
  * specifications. Incorrect values may damage the motor or battery pack.
  *
  ******************************************************************************
  */

#ifndef __FEB_RMS_CONFIG_H
#define __FEB_RMS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                           MOTOR CONFIGURATION                              */
/* ========================================================================== */

/**
 * @brief Maximum motor torque in tenths of Nm
 * @note RMS PM100DX typical max: 230 Nm = 2300 in tenths
 *       Adjust based on your motor specifications
 */
#define MAX_TORQUE           2300    /* 230.0 Nm in tenths */

/**
 * @brief Reduced torque limit at low pack voltage
 * @note Used when pack voltage drops below LOW_PACK_VOLTAGE threshold
 */
#define MAX_TORQUE_LOW_V     1500    /* 150.0 Nm in tenths */

/* ========================================================================== */
/*                        ACCUMULATOR CONFIGURATION                           */
/* ========================================================================== */

/**
 * @brief Peak current limit in Amps
 * @note Based on accumulator capability and FSAE rules
 *       Used for power limiting calculations
 */
#define PEAK_CURRENT         60.0f   /* 60 A peak current */

/**
 * @brief Low pack voltage threshold in decivolts (0.1V units)
 * @note Below this voltage, torque is limited to MAX_TORQUE_LOW_V
 *       Example: 4200 = 420.0V
 */
#define LOW_PACK_VOLTAGE     4200    /* 420.0V in decivolts */

/**
 * @brief Initial/nominal pack voltage in decivolts
 * @note Used for startup calculations
 *       Example: 5100 = 510.0V
 */
#define INIT_VOLTAGE         5100    /* 510.0V in decivolts */

/* ========================================================================== */
/*                         CONVERSION FACTORS                                 */
/* ========================================================================== */

/**
 * @brief Conversion factor from RPM to rad/s
 * @note Formula: rad/s = RPM * (2π / 60) = RPM * 0.10472
 */
#define RPM_TO_RAD_S         0.10472f

/* ========================================================================== */
/*                         SAFETY THRESHOLDS                                  */
/* ========================================================================== */

/**
 * @brief Minimum motor speed for torque/power calculations (rad/s)
 * @note Below this speed, use constant torque mode
 *       Prevents division by zero and handles negative speeds
 */
#define MIN_MOTOR_SPEED_RAD_S     15.0f

/**
 * @brief Minimum pack voltage for operation (volts)
 * @note Approximately 2.85V per cell for 140S pack
 */
#define MIN_PACK_VOLTAGE_V        400.0f

/**
 * @brief Assumed accumulator internal resistance (ohms)
 * @note Used for voltage drop estimation under load
 */
#define ACCUMULATOR_RESISTANCE_OHM  1.0f

/**
 * @brief Brake position threshold for torque cutoff (%)
 * @note If brake position exceeds this, acceleration is cut
 */
#define BRAKE_POSITION_THRESHOLD   15.0f

/**
 * @brief APPS threshold for plausibility reset (%)
 * @note Pedal must be below this to reset plausibility faults
 */
#define APPS_RESET_THRESHOLD       5.0f

#ifdef __cplusplus
}
#endif

#endif /* __FEB_RMS_CONFIG_H */
