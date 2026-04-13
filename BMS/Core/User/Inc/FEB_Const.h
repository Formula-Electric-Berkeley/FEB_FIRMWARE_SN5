#ifndef INC_FEB_CONST_H_
#define INC_FEB_CONST_H_

// ********************************** ADBMS Configuration Constants **************

// Number of ADBMS6830 ICs per bank
#define FEB_NUM_ICPBANK 1

// Number of banks in the system
#define FEB_NBANKS 1

// Total number of ICs in the daisy chain
#define FEB_NUM_IC (FEB_NUM_ICPBANK * FEB_NBANKS)

// Number of cells per IC
#define FEB_NUM_CELLS_PER_IC 14

// Total number of cells per bank
#define FEB_NUM_CELLS_PER_BANK (FEB_NUM_CELLS_PER_IC * FEB_NUM_ICPBANK)

// Alias for compatibility
#define FEB_NUM_CELL_PER_BANK FEB_NUM_CELLS_PER_BANK

// Number of temperature sensors per IC (for MUX reading)
#define FEB_NUM_TEMP_SENSE_PER_IC 10

// Total number of temperature sensors per bank
#define FEB_NUM_TEMP_SENSORS (FEB_NUM_TEMP_SENSE_PER_IC * FEB_NUM_ICPBANK)

// ********************************** Temperature Validation Range ****************
// Note: Thermistor constants are now in BMS_HW_Config.h
// Valid operating range for temperature sensors (in deci-Celsius)

#define TEMP_VALID_MIN_DC (-400) // -40.0°C minimum valid reading
#define TEMP_VALID_MAX_DC 850    // 85.0°C maximum valid reading

// ********************************** Voltage and Temperature Limits *************

// Cell voltage limits (in millivolts)
#define FEB_CELL_MAX_VOLTAGE_MV 4200      // Maximum safe cell voltage (Li-ion typical)
#define FEB_CELL_MIN_VOLTAGE_MV 2500      // Minimum safe cell voltage (Li-ion typical)
#define FEB_CELL_BALANCE_THRESHOLD_MV 10  // Start balancing if cell is >10mV above minimum
#define FEB_CELL_BALANCE_INTERVAL_MS 1000 // Balancing cycle interval (1 second)
#define FEB_CELL_BALANCE_ALL_AT_ONCE 1    // 1=balance all qualifying cells, 0=alternate odd/even

// Cell temperature limits (in deci-Celsius, 1 dC = 0.1°C)
#define FEB_CELL_MAX_TEMP_DC 600             // 60.0°C maximum cell temperature
#define FEB_CELL_MIN_TEMP_DC -200            // -20.0°C minimum cell temperature
#define FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC 550 // 55.0°C soft limit for charging

// Error thresholds (consecutive violations before triggering fault)
#define FEB_VOLTAGE_ERROR_THRESH 3 // Trigger fault after 3 consecutive voltage violations
#define FEB_TEMP_ERROR_THRESH 5    // Trigger fault after 5 consecutive temp violations

// ********************************** isoSPI Communication Mode ******************

// // isoSPI Mode Selection - Choose ONE of the following:
// #define ISOSPI_MODE_REDUNDANT 0 // Dual SPI with automatic PEC-error failover
// #define ISOSPI_MODE_SPI1_ONLY 1 // Use only SPI1 (primary channel)
// #define ISOSPI_MODE_SPI2_ONLY 2 // Use only SPI2 (backup channel)

// // *** SELECT MODE HERE ***
// #define ISOSPI_MODE ISOSPI_MODE_SPI1_ONLY

// // Redundant Mode Configuration (only used when ISOSPI_MODE == ISOSPI_MODE_REDUNDANT)
// #define ISOSPI_FAILOVER_PEC_THRESHOLD 5 // Number of PEC errors before failover
// #define ISOSPI_FAILOVER_LOCKOUT_MS 1000 // Milliseconds to wait before allowing failover again
// #define ISOSPI_PRIMARY_CHANNEL 1        // Primary channel: 1=SPI1, 2=SPI2

#endif /* INC_FEB_CONST_H_ */
