/**
 * @file BMS_HW_Config.h
 * @brief Hardware-specific constants for BMS ADBMS system
 * @author Formula Electric @ Berkeley
 *
 * This file contains all hardware-specific values that may change between
 * board revisions. Modify these values when adapting to new hardware.
 */

#ifndef BMS_HW_CONFIG_H
#define BMS_HW_CONFIG_H

#include <stdint.h>

/*============================================================================
 * Pack Topology
 *
 * Current: 1 bank with 1 IC
 * Future:  2 banks with 4 ICs per bank
 *============================================================================*/
#define BMS_NUM_BANKS 1    /**< Number of battery banks */
#define BMS_ICS_PER_BANK 1 /**< ADBMS6830B ICs per bank */
#define BMS_TOTAL_ICS (BMS_NUM_BANKS * BMS_ICS_PER_BANK)
#define BMS_CELLS_PER_IC 14 /**< Active cells monitored per IC (of 16 possible) */
#define BMS_TOTAL_CELLS (BMS_TOTAL_ICS * BMS_CELLS_PER_IC)

/*============================================================================
 * Temperature Sensing Hardware
 *
 * 6 MUX chips (M1-M6) connected to GPIO1-GPIO6 of ADBMS6830B
 * Each MUX has 8 inputs: 7 temp sensors (IN1-IN7), IN8 grounded
 * MUX select lines controlled by GPO7/GPO8/GPO9
 *
 * Select line circuit: 2.5k pull-up to VREG, 1k pull-down to GND
 *============================================================================*/
#define BMS_TEMP_NUM_MUXES 6       /**< Number of analog MUX chips (M1-M6) */
#define BMS_TEMP_SENSORS_PER_MUX 7 /**< Temp sensors per MUX (IN1-IN7, IN8 grounded) */
#define BMS_TEMP_TOTAL_SENSORS (BMS_TEMP_NUM_MUXES * BMS_TEMP_SENSORS_PER_MUX) /**< 42 total */

/* GPIO pins reading MUX outputs (directly maps to ADBMS GPIO index) */
#define BMS_MUX_M1_GPIO 0 /**< GPIO1 reads M1 output */
#define BMS_MUX_M2_GPIO 1 /**< GPIO2 reads M2 output */
#define BMS_MUX_M3_GPIO 2 /**< GPIO3 reads M3 output */
#define BMS_MUX_M4_GPIO 3 /**< GPIO4 reads M4 output */
#define BMS_MUX_M5_GPIO 4 /**< GPIO5 reads M5 output */
#define BMS_MUX_M6_GPIO 5 /**< GPIO6 reads M6 output */

/* GPO pins for MUX channel selection (bit positions in 10-bit GPO field)
 * Note: GPO field is bits [9:0] where bit 0 = GPO1, bit 9 = GPO10
 * Select lines: Sel1=GPO7 (bit 6), Sel2=GPO8 (bit 7), Sel3=GPO9 (bit 8) */
#define BMS_MUX_SEL1_BIT 6 /**< GPO7 = Sel1 (LSB of channel select) */
#define BMS_MUX_SEL2_BIT 7 /**< GPO8 = Sel2 */
#define BMS_MUX_SEL3_BIT 8 /**< GPO9 = Sel3 (MSB of channel select) */
#define BMS_MUX_SEL_MASK ((1 << BMS_MUX_SEL1_BIT) | (1 << BMS_MUX_SEL2_BIT) | (1 << BMS_MUX_SEL3_BIT))

/*============================================================================
 * NTC Thermistor Parameters
 *
 * NTC: 10k @ 25C, Beta = 3428
 * Circuit: 5V supply, 10k pull-up resistor (R1)
 * Formula: R_th = R_ref * exp(Beta * (1/T - 1/T_ref))
 *          V = Vs * R_th / (R1 + R_th)
 *============================================================================*/
#define THERM_BETA 3428.0f           /**< Beta coefficient */
#define THERM_R_REF_OHMS 10000.0f    /**< Resistance at T_ref (10k) */
#define THERM_T_REF_KELVIN 298.15f   /**< Reference temperature (25C in K) */
#define THERM_R_PULLUP_OHMS 10000.0f /**< Pull-up resistor R1 (10k) */
#define THERM_VS_MV 5000.0f          /**< Supply voltage in mV */
#define THERM_KELVIN_OFFSET 273.15f  /**< Kelvin to Celsius offset */

/* Pre-computed values for optimization */
#define THERM_INV_T_REF (1.0f / THERM_T_REF_KELVIN)
#define THERM_INV_BETA (1.0f / THERM_BETA)

/* Voltage bounds for valid thermistor readings (in mV) */
#define THERM_MIN_VOLTAGE_MV 100.0f  /**< Below = open circuit */
#define THERM_MAX_VOLTAGE_MV 4900.0f /**< Above = short circuit */

/*============================================================================
 * Cell Voltage Thresholds (in millivolts)
 *============================================================================*/
/* Normal operation limits */
#define BMS_CELL_UV_NORMAL_MV 2500 /**< Undervoltage fault threshold */
#define BMS_CELL_OV_NORMAL_MV 4200 /**< Overvoltage fault threshold */

/* Charging limits (tighter) */
#define BMS_CELL_UV_CHARGING_MV 2700 /**< Don't charge below this */
#define BMS_CELL_OV_CHARGING_MV 4150 /**< Stop charging above this */

/* Balancing limits */
#define BMS_CELL_UV_BALANCING_MV 3000 /**< Stop balancing if any cell below */
#define BMS_CELL_OV_BALANCING_MV 4200 /**< Same as normal */

/* Balancing algorithm parameters */
#define BMS_BALANCE_THRESHOLD_MV 10 /**< Balance cells >10mV above minimum */
#define BMS_BALANCE_HYSTERESIS_MV 5 /**< Stop when cells within 5mV */

/* Redundancy check */
#define BMS_C_S_VOLTAGE_TOLERANCE_MV 30 /**< Max allowed C-ADC vs S-ADC difference */

/*============================================================================
 * Temperature Thresholds (in deci-Celsius: 10 = 1.0C)
 *============================================================================*/
#define BMS_CELL_MAX_TEMP_DC 600     /**< 60.0C - fault threshold */
#define BMS_CELL_MIN_TEMP_DC (-200)  /**< -20.0C - fault threshold */
#define BMS_BALANCE_MAX_TEMP_DC 550  /**< 55.0C - stop balancing */
#define BMS_CHARGING_MAX_TEMP_DC 450 /**< 45.0C - reduce charge rate */

/* Valid sensor range (outside = sensor fault) */
#define BMS_TEMP_SENSOR_MIN_DC (-400) /**< -40.0C */
#define BMS_TEMP_SENSOR_MAX_DC 850    /**< 85.0C */

/*============================================================================
 * Error Thresholds
 *============================================================================*/
#define BMS_PEC_ERROR_THRESHOLD 5     /**< Consecutive PEC errors before comm fault */
#define BMS_VOLTAGE_ERROR_THRESHOLD 3 /**< Consecutive UV/OV before voltage fault */
#define BMS_TEMP_ERROR_THRESHOLD 5    /**< Consecutive temp violations before fault */
#define BMS_INIT_RETRY_COUNT 5        /**< Init attempts before giving up */
#define BMS_INIT_RETRY_DELAY_MS 500   /**< Delay between init retries */

/*============================================================================
 * Task Timing (in milliseconds)
 *============================================================================*/
#define BMS_VOLTAGE_INTERVAL_MS 100  /**< 10 Hz voltage monitoring */
#define BMS_TEMP_INTERVAL_MS 500     /**< 2 Hz temperature monitoring */
#define BMS_BALANCE_INTERVAL_MS 1000 /**< 1 Hz balancing cycle */
#define BMS_ADC_SETTLE_MS 3          /**< ADC conversion time */
#define BMS_MUX_SETTLE_MS 1          /**< MUX switching settle time */
#define BMS_ADC_POLL_TIMEOUT_MS 10   /**< Max time to wait for ADC */

/*============================================================================
 * Discharge Timer Configuration
 *============================================================================*/
#define BMS_DISCHARGE_TIMEOUT_CODE 0x3F /**< Maximum discharge timeout (DCTO field) */

#endif /* BMS_HW_CONFIG_H */
