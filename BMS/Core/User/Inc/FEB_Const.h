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

// Total number of temperature sensors per bank (10 per IC × 2 ICs)
#define FEB_NUM_TEMP_SENSORS 41

// Number of temperature sensors per IC (for MUX reading)
#define FEB_NUM_TEMP_SENSE_PER_IC 10

// ********************************** ADBMS6830B ADC Conversion Constants ********
// From ADBMS6830B datasheet - Cell voltage measurement

#define ADBMS_ADC_LSB_UV 150      // ADC resolution: 150 µV/LSB
#define ADBMS_ADC_LSB_V 0.000150f // ADC resolution in volts
#define ADBMS_ADC_OFFSET_V 1.5f   // ADC bipolar offset voltage

// ********************************** ADBMS6830B Open Wire Detection **************
// Open Wire (OW) detection configuration for cell voltage measurements
// OW detection applies a test current to detect disconnected sense wires
// Options: 0x00=OFF, 0x01=EVEN_CH, 0x02=ODD_CH, 0x03=ALL_CH
#define ADBMS_OW_DETECTION_MODE 0x00 // OW detection disabled

// ********************************** Thermistor Beta Parameter Constants *********
// NTC Thermistor conversion using Beta parameter equation

#define THERM_R_REF_OHMS 10000.0f    // Reference resistance at 25°C (10k NTC)
#define THERM_T_REF_K 298.15f        // Reference temperature in Kelvin (25°C)
#define THERM_BETA 3428.0f           // Beta coefficient from datasheet
#define THERM_R_PULLUP_OHMS 10000.0f // Pull-up resistor value (R1)
#define THERM_VS_MV 5000.0f          // Supply voltage in mV (5V)

// Pre-computed values for optimization
#define THERM_INV_T_REF (1.0f / THERM_T_REF_K) // 1/T_ref
#define THERM_INV_BETA (1.0f / THERM_BETA)     // 1/B
#define THERM_KELVIN_OFFSET 273.15f            // K to C conversion

// Voltage bounds for valid thermistor readings (in mV)
#define THERM_MIN_VOLTAGE_MV 100.0f  // Below this: open circuit / disconnected
#define THERM_MAX_VOLTAGE_MV 4900.0f // Above this: short circuit / sensor fault

// ********************************** Error Type Codes ****************************

#define ERROR_TYPE_TEMP_VIOLATION 0x10
#define ERROR_TYPE_LOW_TEMP_READS 0x20
#define ERROR_TYPE_VOLTAGE_VIOLATION 0x30
#define ERROR_TYPE_INIT_FAILURE 0x40

// ********************************** Temperature Validation Range ****************
// Valid operating range for temperature sensors (in deci-Celsius)

#define TEMP_VALID_MIN_DC (-400) // -40.0°C minimum valid reading
#define TEMP_VALID_MAX_DC 850    // 85.0°C maximum valid reading

// ********************************** Voltage and Temperature Limits *************

// Cell voltage limits (in millivolts)
#define FEB_CELL_MAX_VOLTAGE_MV 4200     // Maximum safe cell voltage (Li-ion typical)
#define FEB_CELL_MIN_VOLTAGE_MV 2500     // Minimum safe cell voltage (Li-ion typical)
#define FEB_CELL_BALANCE_THRESHOLD_MV 10 // Start balancing if cell is >10mV above minimum

// Cell temperature limits (in deci-Celsius, 1 dC = 0.1°C)
#define FEB_CELL_MAX_TEMP_DC 600             // 60.0°C maximum cell temperature
#define FEB_CELL_MIN_TEMP_DC -200            // -20.0°C minimum cell temperature
#define FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC 550 // 55.0°C soft limit for charging

// Error thresholds (consecutive violations before triggering fault)
#define FEB_VOLTAGE_ERROR_THRESH 3 // Trigger fault after 3 consecutive voltage violations
#define FEB_TEMP_ERROR_THRESH 5    // Trigger fault after 5 consecutive temp violations

// ********************************** isoSPI Communication Mode ******************

// isoSPI Mode Selection - Choose ONE of the following:
#define ISOSPI_MODE_REDUNDANT 0 // Dual SPI with automatic PEC-error failover
#define ISOSPI_MODE_SPI1_ONLY 1 // Use only SPI1 (primary channel)
#define ISOSPI_MODE_SPI2_ONLY 2 // Use only SPI2 (backup channel)

// *** SELECT MODE HERE ***
#define ISOSPI_MODE ISOSPI_MODE_SPI1_ONLY

// Redundant Mode Configuration (only used when ISOSPI_MODE == ISOSPI_MODE_REDUNDANT)
#define ISOSPI_FAILOVER_PEC_THRESHOLD 5 // Number of PEC errors before failover
#define ISOSPI_FAILOVER_LOCKOUT_MS 1000 // Milliseconds to wait before allowing failover again
#define ISOSPI_PRIMARY_CHANNEL 1        // Primary channel: 1=SPI1, 2=SPI2

// ********************************** Accumulator Structure **********************

typedef struct
{
  float voltage_V; // C-code voltage measurement
  float voltage_S; // S-code voltage measurement (redundant)
  float temperature_C;
  uint8_t violations;  // Consecutive violation counter for this cell
  uint8_t discharging; // Cell is being discharged for balancing
} cell_data_t;

typedef struct
{
  cell_data_t cells[FEB_NUM_CELLS_PER_BANK];
  float total_voltage_V;
  float min_voltage_V;
  float max_voltage_V;
  float avg_temp_C;
  float min_temp_C;
  float max_temp_C;
  uint8_t voltRead;                                   // Voltage reading valid flag
  uint8_t tempRead;                                   // Temperature reading valid flag
  uint8_t badReadV;                                   // Bad voltage read counter
  float temp_sensor_readings_V[FEB_NUM_TEMP_SENSORS]; // Temperature sensor readings
  uint8_t temp_violations[FEB_NUM_TEMP_SENSORS];      // Per-sensor violation counters
} bank_data_t;

typedef struct
{
  bank_data_t banks[FEB_NBANKS];
  float total_voltage_V;
  float min_voltage_V;
  float max_voltage_V;
  float pack_min_voltage_V; // Minimum cell voltage across entire pack
  float pack_max_voltage_V; // Maximum cell voltage across entire pack
  float avg_temp_C;
  float pack_min_temp;     // Minimum temperature across entire pack
  float pack_max_temp;     // Maximum temperature across entire pack
  float average_pack_temp; // Average temperature across entire pack
  uint8_t error_type;
} accumulator_t;

#endif /* INC_FEB_CONST_H_ */
