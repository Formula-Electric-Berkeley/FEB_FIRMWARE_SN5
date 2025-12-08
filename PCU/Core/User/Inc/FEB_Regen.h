/**
  ******************************************************************************
  * @file           : FEB_Regen.h
  * @brief          : Regenerative braking (SN3-style implementation)
  ******************************************************************************
  * Based on SN3 regen system with filters for speed, SOC, and temperature
  */

#ifndef INC_FEB_REGEN_H_
#define INC_FEB_REGEN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "FEB_ADC.h"
#include "FEB_CAN_RMS.h"
#include "FEB_CAN_BMS.h"
#include "FEB_RMS_Config.h"
#include <stdbool.h>
#include <math.h>

/* ========================================================================== */
/*                          REGEN CONSTANTS (SN3)                            */
/* ========================================================================== */

/* Torque and Current Limits */
#define MAX_TORQUE_REGEN       230.0f   /* Maximum regen torque (Nm) */
#define PEAK_CURRENT_REGEN     20.0f    /* 20A charging limit */

/* Speed Filter */
#define FADE_SPEED_RPM         200      /* No regen below this speed */

/* SOC Filter - Linear interpolation */
#define START_REGEN_SOC        0.95f    /* Start reducing regen at 95% SOC */
#define MAX_REGEN_SOC          0.80f    /* Full regen available at/below 80% SOC */

/* Temperature Filter - Exponential decay */
#define MAX_CELL_TEMP          45       /* Vertical asymptote at 45°C */
#define TEMP_FILTER_SHARPNESS  1.0f     /* Controls exponential curve steepness */

/* User and Brake Settings */
#define USER_REGEN_FILTER      1.0f     /* User preference multiplier (0.0-1.0) */
#define REGEN_BRAKE_POS_THRESH 0.20f    /* 20% brake position to activate regen */

/* Conversion Factors */
#define RPM_TO_RAD_S           0.10472f /* RPM to rad/s conversion (2π/60) */

/* ========================================================================== */
/*                          FUNCTION PROTOTYPES                               */
/* ========================================================================== */

/**
 * @brief Calculate maximum regenerative torque based on electrical limits
 * Uses 20A current limit: max_torque = (V * 20A) / ω
 * @return Maximum regen torque in Nm
 */
float FEB_Regen_GetElecMaxRegenTorque(void);

/**
 * @brief Apply speed filter (step function at FADE_SPEED_RPM)
 * @param unfiltered_regen_torque: Input torque before filtering
 * @return Filtered torque (0 if below speed, unchanged if above)
 */
float FEB_Regen_FilterSpeed(float unfiltered_regen_torque);

/**
 * @brief Apply SOC filter (saturated linear function)
 * Maps from (START_REGEN_SOC, 0) to (MAX_REGEN_SOC, 1)
 * @param unfiltered_regen_torque: Input torque before filtering
 * @return Filtered torque (scaled by SOC factor)
 */
float FEB_Regen_FilterSOC(float unfiltered_regen_torque);

/**
 * @brief Apply temperature filter (exponential decay)
 * Function with vertical asymptote at MAX_CELL_TEMP
 * @param unfiltered_regen_torque: Input torque before filtering
 * @return Filtered torque (scaled by temperature factor)
 */
float FEB_Regen_FilterTemp(float unfiltered_regen_torque);

/**
 * @brief Apply all regen filters (wrapper function)
 * Applies speed, SOC, temperature, and user filters sequentially
 * @param regen_torque_max: Maximum regen torque from electrical calculation
 * @return Fully filtered regen torque ready to command
 */
float FEB_Regen_ApplyFilters(float regen_torque_max);

/**
 * @brief Get filtered regen torque (main function called by RMS control)
 * @return MAGNITUDE of regen torque in Nm (caller applies negative sign)
 */
float FEB_Regen_GetFilteredTorque(void);

/**
 * @brief Check if BMS state allows regenerative braking
 * @return true if regen allowed, false otherwise
 */
bool FEB_Regen_IsAllowedByBMS(void);

/* Helper macros */
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#ifdef __cplusplus
}
#endif

#endif /* INC_FEB_REGEN_H_ */