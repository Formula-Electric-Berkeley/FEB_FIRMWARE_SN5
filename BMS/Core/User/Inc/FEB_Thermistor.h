#ifndef INC_FEB_THERMISTOR_H_
#define INC_FEB_THERMISTOR_H_

#include <stdint.h>
#include <math.h>
#include "FEB_Const.h"

// ********************************** Thermistor Beta Parameter Conversion *********
// Converts thermistor voltage to temperature using the Beta parameter equation
//
// Equation:
//   R_therm = V_meas * R_pullup / (Vs - V_meas)
//   T_kelvin = 1 / ( (1/T_ref) + (1/B) * ln(R_therm / R_ref) )
//   T_celsius = T_kelvin - 273.15

/**
 * @brief Convert thermistor voltage to temperature using Beta parameter equation
 *
 * @param voltage_mV  Measured voltage across thermistor in millivolts
 * @return float      Temperature in Celsius, or NAN for invalid readings
 */
static inline float FEB_Thermistor_Voltage_To_Temp_C(float voltage_mV)
{
  // Validate voltage range
  if (voltage_mV < THERM_MIN_VOLTAGE_MV || voltage_mV > THERM_MAX_VOLTAGE_MV)
  {
    return NAN; // Invalid reading
  }

  // Prevent division by zero (voltage equals supply)
  float denominator = THERM_VS_MV - voltage_mV;
  if (denominator <= 0.0f)
  {
    return NAN; // Invalid: voltage >= supply voltage
  }

  // Calculate thermistor resistance
  // R_therm = V_meas * R_pullup / (Vs - V_meas)
  float R_thermistor = (voltage_mV * THERM_R_PULLUP_OHMS) / denominator;

  // Validate resistance is positive (should always be, but safety check)
  if (R_thermistor <= 0.0f)
  {
    return NAN;
  }

  // Calculate ln(R_therm / R_ref)
  float ln_ratio = logf(R_thermistor / THERM_R_REF_OHMS);

  // Calculate temperature in Kelvin using Beta equation
  // T = 1 / ( (1/T_ref) + (1/B) * ln(R/R_ref) )
  float inv_T_kelvin = THERM_INV_T_REF + (THERM_INV_BETA * ln_ratio);

  // Prevent division by zero
  if (inv_T_kelvin <= 0.0f)
  {
    return NAN;
  }

  float T_kelvin = 1.0f / inv_T_kelvin;

  // Convert to Celsius
  return T_kelvin - THERM_KELVIN_OFFSET;
}

#endif /* INC_FEB_THERMISTOR_H_ */
