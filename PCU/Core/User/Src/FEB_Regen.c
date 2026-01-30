/**
 ******************************************************************************
 * @file           : FEB_Regen.c
 * @brief          : Regenerative braking implementation (SN3-style)
 ******************************************************************************
 * Mirrors SN3 regen system:
 * - Calculates max regen based on 20A current limit and motor speed
 * - Applies speed, SOC, and temperature filters
 * - Returns magnitude of regen torque (caller applies negative sign)
 */

#include "FEB_Regen.h"
#include "FEB_Debug.h"

/* External references to RMS and BMS data */
extern RMS_MESSAGE_TYPE RMS_MESSAGE;

/**
 * @brief Calculate maximum regenerative torque based on electrical limits
 * Formula: max_torque = min(MAX_TORQUE_REGEN, (V_acc * 20A) / omega)
 *
 * This ensures we don't exceed the 20A charging limit while maximizing
 * energy recovery at different speeds.
 */
float FEB_Regen_GetElecMaxRegenTorque(void)
{
  // Get accumulator voltage (convert from RMS format: decivolts with 50V offset)
  // INIT_VOLTAGE is in decivolts (5100 = 510V), so divide by 10 for volts
  float accumulator_voltage = MIN(INIT_VOLTAGE / 10.0f, (RMS_MESSAGE.HV_Bus_Voltage - 50) / 10.0f);

  // Convert motor speed from RPM to rad/s
  float motor_speed_rads = RMS_MESSAGE.Motor_Speed * RPM_TO_RAD_S;

  // Avoid division by zero at very low speeds
  if (motor_speed_rads < 1.0f)
  {
    return 0.0f;
  }

  // Calculate max torque: P = V*I, and P = τ*ω, so τ = V*I/ω
  float max_torque = MIN(MAX_TORQUE_REGEN, (accumulator_voltage * PEAK_CURRENT_REGEN) / motor_speed_rads);

  return max_torque;
}

/**
 * @brief Apply speed filter - step function at FADE_SPEED_RPM
 * Below FADE_SPEED_RPM: return 0 (no regen for safety)
 * Above FADE_SPEED_RPM: return unchanged (full regen allowed)
 */
float FEB_Regen_FilterSpeed(float unfiltered_regen_torque)
{
  float motor_speed_rpm = (float)RMS_MESSAGE.Motor_Speed;

  if (motor_speed_rpm < FADE_SPEED_RPM)
  {
    return 0.0f;
  }

  return unfiltered_regen_torque;
}

/**
 * @brief Apply SOC filter - saturated linear function
 * Linear interpolation between:
 *   (START_REGEN_SOC, 0) → no regen at high SOC
 *   (MAX_REGEN_SOC, 1)   → full regen at low SOC
 *
 * Formula: y = m(x - x0) + y0, where m = (1-0)/(MAX-START)
 */
float FEB_Regen_FilterSOC(float unfiltered_regen_torque)
{
  // TODO: Get actual SOC from BMS - placeholder for now
  // Assume 85% SOC as conservative default (allows full regen)
  float state_of_charge = 0.85f; // FEB_CAN_BMS_getSOC() when available

  // Calculate slope: m = (y1 - y0) / (x1 - x0)
  float slope = (1.0f - 0.0f) / (MAX_REGEN_SOC - START_REGEN_SOC);

  // Calculate filter coefficient: k = m(x - x0) + y0
  float k_SOC = slope * (state_of_charge - START_REGEN_SOC);

  // Saturate between 0 and 1
  if (k_SOC > 1.0f)
  {
    k_SOC = 1.0f;
  }
  if (k_SOC < 0.0f)
  {
    return 0.0f; // No regen at high SOC
  }

  return k_SOC * unfiltered_regen_torque;
}

/**
 * @brief Apply temperature filter - exponential decay
 * Function with vertical asymptote at MAX_CELL_TEMP (45°C)
 *
 * Formula: k_temp = 1 - e^(SHARPNESS * (T - MAX_TEMP))
 *
 * This creates smooth rolloff as temperature approaches limit
 */
float FEB_Regen_FilterTemp(float unfiltered_regen_torque)
{
  // Get cell temperature from BMS (convert from BMS format if needed)
  uint16_t bms_temp = FEB_CAN_BMS_getTemp();
  float hottest_cell_temp_C = (float)bms_temp / 10.0f; // Assuming deciselsius format

  // Calculate exponential filter coefficient
  float exponent = TEMP_FILTER_SHARPNESS * (hottest_cell_temp_C - MAX_CELL_TEMP);
  float k_temp = 1.0f - powf(2.71828f, exponent); // e^x

  // No regen if coefficient goes negative (over temp)
  if (k_temp < 0.0f)
  {
    return 0.0f;
  }

  return k_temp * unfiltered_regen_torque;
}

/**
 * @brief Apply all regen filters (wrapper function)
 * Applies filters sequentially:
 *   1. Speed filter (binary on/off)
 *   2. SOC filter (linear interpolation)
 *   3. Temperature filter (exponential decay)
 *   4. User preference filter (constant multiplier)
 */
float FEB_Regen_ApplyFilters(float regen_torque_max)
{
  float filtered_regen_torque = regen_torque_max;

  // Apply each filter sequentially
  filtered_regen_torque = FEB_Regen_FilterSpeed(filtered_regen_torque);
  filtered_regen_torque = FEB_Regen_FilterSOC(filtered_regen_torque);
  filtered_regen_torque = FEB_Regen_FilterTemp(filtered_regen_torque);

  // Apply user preference filter
  return filtered_regen_torque * USER_REGEN_FILTER;
}

/**
 * @brief Get filtered regen torque - main function called by RMS control
 * Returns MAGNITUDE of regen torque (positive value)
 * Caller must apply negative sign for braking direction
 */
float FEB_Regen_GetFilteredTorque(void)
{
  // Calculate maximum regen based on electrical limits
  float present_regen_max = FEB_Regen_GetElecMaxRegenTorque();

  // Apply all filters and return
  return FEB_Regen_ApplyFilters(present_regen_max);
}

/**
 * @brief Check if BMS state allows regenerative braking
 * TODO: Implement proper BMS state machine check
 */
bool FEB_Regen_IsAllowedByBMS(void)
{
  // Get current BMS state
  FEB_SM_ST_t current_state = FEB_CAN_BMS_getState();

  // Allow regen in DRIVE or DRIVE_REGEN states
  // TODO: Define FEB_SM_ST_DRIVE_REGEN state in BMS state machine
  return (current_state == FEB_SM_ST_DRIVE);
}
