#ifndef INC_FEB_CELL_TEMP_LUT_H_
#define INC_FEB_CELL_TEMP_LUT_H_

// ********************************** Temperature LUT ****************************
// Lookup table for converting thermistor ADC values to temperature
// Typically used with NTC thermistors in battery cell monitoring

#include <stdint.h>

// Function to convert thermistor voltage to temperature
// voltage_mV: voltage in millivolts from thermistor divider circuit
// returns: temperature in degrees Celsius
static inline float convert_thermistor_to_temp(float voltage_mV) {
    // Simplified conversion - replace with actual thermistor equation or LUT
    // This is a placeholder using a basic linear approximation
    // Typical NTC 10k thermistor at 25°C with voltage divider

    // TODO: Replace with actual thermistor calibration curve based on your hardware
    // Common approaches:
    // 1. Steinhart-Hart equation for NTC thermistors
    // 2. Lookup table with interpolation
    // 3. Polynomial curve fit

    // For now, return a dummy linear approximation
    // Assumes 2.5V ≈ 25°C with ~10mV/°C sensitivity
    float temp_C = 25.0f + (voltage_mV - 2500.0f) / 10.0f;

    return temp_C;
}

// Function to convert thermistor voltage to temperature in deci-Celsius units (0.1°C)
// voltage_mV: voltage in millivolts from thermistor divider circuit
// returns: temperature in units of 100mC (0.1°C), e.g., 250 = 25.0°C
static inline int FEB_Cell_Temp_LUT_Get_Temp_100mC(int voltage_mV) {
    // Convert using the same logic as convert_thermistor_to_temp
    // TODO: Replace with actual thermistor calibration curve/LUT based on your hardware

    // For now, use a simple linear approximation
    // Assumes 2.5V ≈ 25°C with ~10mV/°C sensitivity
    float temp_C = 25.0f + ((float)voltage_mV - 2500.0f) / 10.0f;

    // Convert to units of 0.1°C (100mC)
    return (int)(temp_C * 10.0f);
}

#endif /* INC_FEB_CELL_TEMP_LUT_H_ */
