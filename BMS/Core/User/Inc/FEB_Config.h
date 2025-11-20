/**
 * @file FEB_Config.h
 * @brief BMS Configuration Getters
 * @note Hardcoded configuration values for initial development
 */

#ifndef FEB_CONFIG_H
#define FEB_CONFIG_H

#include <stdint.h>
#include "FEB_Const.h"

/**
 * @brief Get maximum safe cell voltage
 * @return Maximum voltage in millivolts (mV)
 */
static inline uint16_t FEB_Config_Get_Cell_Max_Voltage_mV(void) {
    return FEB_CELL_MAX_VOLTAGE_MV;
}

/**
 * @brief Get minimum safe cell voltage
 * @return Minimum voltage in millivolts (mV)
 */
static inline uint16_t FEB_Config_Get_Cell_Min_Voltage_mV(void) {
    return FEB_CELL_MIN_VOLTAGE_MV;
}

/**
 * @brief Get maximum safe cell temperature
 * @return Maximum temperature in deci-Celsius (dC)
 */
static inline int16_t FEB_Config_Get_Cell_Max_Temperature_dC(void) {
    return FEB_CELL_MAX_TEMP_DC;
}

/**
 * @brief Get minimum safe cell temperature
 * @return Minimum temperature in deci-Celsius (dC)
 */
static inline int16_t FEB_Config_Get_Cell_Min_Temperature_dC(void) {
    return FEB_CELL_MIN_TEMP_DC;
}

/**
 * @brief Get cell balancing voltage threshold
 * @return Threshold in millivolts (mV)
 */
static inline uint16_t FEB_Config_Get_Balance_Threshold_mV(void) {
    return FEB_CELL_BALANCE_THRESHOLD_MV;
}

#endif // FEB_CONFIG_H
