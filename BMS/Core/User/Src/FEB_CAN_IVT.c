/**
 * @file FEB_CAN_IVT.c
 * @brief IVT Current Sensor Interface (STUB)
 * @note Placeholder implementation for initial development
 *       TODO: Implement CAN communication with IVT-S sensor
 */

#include "FEB_CAN_IVT.h"
#include <stdio.h>

// Stub IVT data
static ivt_data_t ivt_data = {0};

/**
 * @brief Initialize IVT sensor communication
 * @note STUB: Does nothing currently
 */
void FEB_IVT_Init(void) {
    ivt_data.valid = 0;
    ivt_data.voltage_V = 0.0f;
    ivt_data.current_A = 0.0f;
    ivt_data.temperature_C = 25.0f;
    printf("[IVT] IVT sensor stub initialized\r\n");
}

/**
 * @brief Get pack voltage from IVT V1 measurement
 * @return Pack voltage in volts
 * @note STUB: Returns nominal voltage based on typical Li-ion pack
 *       Assumes 18 cells * 3.7V nominal = 66.6V
 *       In real implementation, this would read from CAN messages
 */
float FEB_IVT_V1_Voltage(void) {
    // Return nominal pack voltage for now
    // TODO: Read actual voltage from IVT CAN messages
    return 66.6f;  // 18S Li-ion pack nominal voltage
}
