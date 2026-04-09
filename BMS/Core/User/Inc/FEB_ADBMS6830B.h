/**
 * @file FEB_ADBMS6830B.h
 * @brief BMS Application Layer for ADBMS6830B Battery Monitor
 * @author Formula Electric @ Berkeley
 *
 * @details
 * This module provides the high-level BMS application interface for the
 * ADBMS6830B battery monitor IC. It handles:
 * - Cell voltage monitoring with dual-redundant ADC verification (C & S codes)
 * - Temperature monitoring via GPIO multiplexed thermistors
 * - Cell balancing with configurable discharge control
 * - Pack-level voltage and temperature aggregation
 *
 * @par Data Storage
 * All measurement data is stored in the global `FEB_ACC` (accumulator_t)
 * structure which is protected by `ADBMSMutexHandle` for thread safety.
 *
 * @par Thread Safety
 * - Periodic process functions (Voltage_Process, Temperature_Process) must
 *   be called with ADBMSMutexHandle held (typically from FEB_Task_ADBMS)
 * - Getter functions acquire the mutex internally and are safe to call
 *   from any task context
 *
 * @par Hardware Configuration
 * - FEB_NUM_IC: Number of ADBMS6830B ICs in daisy chain (typically 1)
 * - FEB_NUM_CELLS_PER_IC: Cells per IC (14)
 * - FEB_NUM_TEMP_SENSORS: Temperature sensors per bank (41)
 *
 * @see FEB_Task_ADBMS.c for the FreeRTOS task that calls periodic functions
 * @see FEB_Const.h for configuration constants and data structures
 * @see ADBMS6830B_Cmd.h for low-level command interface
 */

#ifndef INC_FEB_ADBMS6830B_H_
#define INC_FEB_ADBMS6830B_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

/*============================================================================
 * ADBMS6830B ADC Configuration Enums
 *============================================================================*/

/** @brief Redundancy bit for ADC conversion */
typedef enum {
    RD_OFF = 0x00,   /**< Single ADC mode */
    RD_ON            /**< Redundant ADC mode */
} RD;

/** @brief Discharge permit during ADC conversion */
typedef enum {
    DCP_OFF = 0x00,  /**< Discharge disabled during conversion */
    DCP_ON           /**< Discharge permitted during conversion */
} DCP;

/** @brief ADC conversion mode */
typedef enum {
    SINGLE = 0x00,   /**< Single conversion */
    CONTINUOUS       /**< Continuous conversion mode */
} CONT;

/** @brief Reset filter flag */
typedef enum {
    RSTF_OFF = 0x00, /**< Don't reset filter */
    RSTF_ON          /**< Reset filter before conversion */
} RSTF;

/** @brief Open-wire detection mode for cell voltage */
typedef enum {
    OW_OFF_ALL_CH = 0x00, /**< Open-wire detection off */
    OW_ON_EVEN_CH,        /**< Open-wire on even channels */
    OW_ON_ODD_CH,         /**< Open-wire on odd channels */
    OW_ON_ALL_CH,         /**< Open-wire on all channels */
} OW;

/** @brief Open-wire detection mode for auxiliary */
typedef enum {
    AUX_OW_OFF = 0x00,  /**< Auxiliary open-wire off */
    AUX_OW_ON           /**< Auxiliary open-wire on */
} AUX_OW;

/** @brief Pull-up/pull-down selection for open-wire */
typedef enum {
    PUP_DOWN = 0x00,  /**< Pull-down current */
    PUP_UP            /**< Pull-up current */
} PUP;

/** @brief Auxiliary channel selection */
typedef enum {
    AUX_ALL = 0x00,  /**< All GPIO channels */
    GPIO1,           /**< GPIO1 only */
    GPIO2,           /**< GPIO2 only */
    GPIO3,           /**< GPIO3 only */
    GPIO4,           /**< GPIO4 only */
    GPIO5,           /**< GPIO5 only */
    GPIO6,           /**< GPIO6 only */
    GPIO7,           /**< GPIO7 only */
    GPIO8,           /**< GPIO8 only */
    GPIO9,           /**< GPIO9 only */
    GPIO10,          /**< GPIO10 only */
    VREF2,           /**< Reference voltage 2 */
    VD,              /**< Digital supply voltage */
    VA,              /**< Analog supply voltage */
    ITEMP,           /**< Internal die temperature */
    VPV,             /**< Reserved */
    VMV,             /**< Reserved */
    VRES             /**< Reserved */
} AUX_CH;

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Initialize ADBMS6830B chips and validate communication
 *
 * Performs the following initialization sequence:
 * 1. Zeros all accumulator data structures
 * 2. Initializes ADBMS configuration registers with defaults
 * 3. Writes configuration to all ICs
 * 4. Reads and validates serial IDs from all ICs
 *
 * @return true if all ICs initialized successfully with valid serial IDs
 * @return false if communication failed or any IC has invalid serial ID
 *
 * @note Must be called before any other FEB_ADBMS_* functions
 * @note On failure, may be retried (FEB_Task_ADBMS retries up to 5 times)
 */
bool FEB_ADBMS_Init(void);

/*============================================================================
 * Periodic Monitoring Functions
 *
 * These functions perform the measurement cycle and should be called
 * periodically from FEB_Task_ADBMS. Caller must hold ADBMSMutexHandle.
 *============================================================================*/

/**
 * @brief Execute cell voltage measurement cycle
 *
 * Performs:
 * 1. Start C-ADC and S-ADC conversions
 * 2. Read voltage registers from all ICs
 * 3. Store voltages in FEB_ACC structure
 * 4. Validate voltages against UV/OV thresholds
 *
 * @note Typical call rate: 10 Hz (every 100ms)
 * @note Caller must hold ADBMSMutexHandle
 *
 * @post FEB_ACC.banks[].cells[].voltage_V updated (C-code)
 * @post FEB_ACC.banks[].cells[].voltage_S updated (S-code)
 * @post FEB_ACC.pack_min_voltage_V, pack_max_voltage_V updated
 * @post FEB_ACC.total_voltage_V updated
 */
void FEB_ADBMS_Voltage_Process(void);

/**
 * @brief Execute temperature measurement cycle
 *
 * Performs:
 * 1. Configure GPIO MUX selection for each channel
 * 2. Start auxiliary ADC conversion
 * 3. Read auxiliary registers
 * 4. Convert to temperature using thermistor lookup
 * 5. Validate temperatures against min/max thresholds
 *
 * @note Typical call rate: 2 Hz (every 500ms)
 * @note Caller must hold ADBMSMutexHandle
 *
 * @post FEB_ACC.banks[].temp_sensor_readings_V[] updated
 * @post FEB_ACC.pack_min_temp, pack_max_temp, average_pack_temp updated
 */
void FEB_ADBMS_Temperature_Process(void);

/*============================================================================
 * Voltage Getters (Thread-Safe)
 *
 * These functions acquire ADBMSMutexHandle internally and can be called
 * safely from any task context.
 *============================================================================*/

/**
 * @brief Get total pack voltage
 * @return Pack voltage in volts (sum of all cell voltages)
 */
float FEB_ADBMS_GET_ACC_Total_Voltage(void);

/**
 * @brief Get minimum cell voltage in pack
 * @return Minimum cell voltage in volts
 */
float FEB_ADBMS_GET_ACC_MIN_Voltage(void);

/**
 * @brief Get maximum cell voltage in pack
 * @return Maximum cell voltage in volts
 */
float FEB_ADBMS_GET_ACC_MAX_Voltage(void);

/**
 * @brief Get individual cell voltage (primary C-ADC)
 * @param bank Bank index (0 to FEB_NBANKS-1)
 * @param cell Cell index within bank (0 to FEB_NUM_CELL_PER_BANK-1)
 * @return Cell voltage in volts, or -1.0f if invalid index
 */
float FEB_ADBMS_GET_Cell_Voltage(uint8_t bank, uint16_t cell);

/**
 * @brief Get individual cell voltage (redundant S-ADC)
 * @param bank Bank index (0 to FEB_NBANKS-1)
 * @param cell Cell index within bank (0 to FEB_NUM_CELL_PER_BANK-1)
 * @return Cell voltage in volts, or -1.0f if invalid index
 */
float FEB_ADBMS_GET_Cell_Voltage_S(uint8_t bank, uint16_t cell);

/**
 * @brief Get consecutive violation count for a cell
 * @param bank Bank index
 * @param cell Cell index
 * @return Number of consecutive UV/OV violations (0 if none)
 */
uint8_t FEB_ADBMS_GET_Cell_Violations(uint8_t bank, uint16_t cell);

/**
 * @brief Check if precharge is complete
 * @return true if pack voltage is within acceptable range
 * @note Currently always returns false (not implemented)
 */
bool FEB_ADBMS_Precharge_Complete(void);

/*============================================================================
 * Temperature Getters (Thread-Safe)
 *============================================================================*/

/**
 * @brief Get average pack temperature
 * @return Average temperature in degrees Celsius
 */
float FEB_ADBMS_GET_ACC_AVG_Temp(void);

/**
 * @brief Get minimum pack temperature
 * @return Minimum temperature in degrees Celsius
 */
float FEB_ADBMS_GET_ACC_MIN_Temp(void);

/**
 * @brief Get maximum pack temperature
 * @return Maximum temperature in degrees Celsius
 */
float FEB_ADBMS_GET_ACC_MAX_Temp(void);

/**
 * @brief Get individual temperature sensor reading
 * @param bank Bank index
 * @param cell Sensor index (0 to FEB_NUM_TEMP_SENSORS-1)
 * @return Temperature in degrees Celsius, or -1.0f if invalid index
 */
float FEB_ADBMS_GET_Cell_Temperature(uint8_t bank, uint16_t cell);

/*============================================================================
 * Cell Balancing
 *============================================================================*/

/**
 * @brief Start cell balancing
 *
 * Initializes balancing mode by:
 * 1. Re-initializing ADBMS configuration
 * 2. Calling FEB_Cell_Balance_Process() for first balancing pass
 *
 * @note Only call when in BMS_STATE_BALANCE or BMS_STATE_BATTERY_FREE
 */
void FEB_Cell_Balance_Start(void);

/**
 * @brief Execute one balancing iteration
 *
 * Performs:
 * 1. Read current cell voltages
 * 2. Find minimum voltage cell
 * 3. Enable discharge on cells > min + FEB_MIN_SLIPPAGE_V (30mV)
 * 4. Alternates odd/even cells every 3 cycles (if FEB_CELL_BALANCE_ALL_AT_ONCE=0)
 *
 * @note Typical call rate: 1 Hz (every 1000ms)
 * @note Caller must hold ADBMSMutexHandle
 */
void FEB_Cell_Balance_Process(void);

/**
 * @brief Stop all cell balancing
 *
 * Disables discharge on all cells by:
 * 1. Clearing DCC bits in all IC configurations
 * 2. Writing configuration to all ICs
 */
void FEB_Stop_Balance(void);

/**
 * @brief Check if balancing is still needed
 *
 * @return true if max-min voltage delta > FEB_MIN_SLIPPAGE_V (30mV)
 * @return false if cells are balanced within threshold or if temperature
 *         exceeds FEB_CONFIG_CELL_SOFT_MAX_TEMP_dC (55°C)
 */
bool FEB_Cell_Balancing_Status(void);

/*============================================================================
 * Diagnostics
 *============================================================================*/

/**
 * @brief Print full accumulator status to console
 *
 * Outputs pack voltage, min/max cells, temperatures, etc.
 *
 * @note Currently disabled (function body commented out)
 */
void FEB_ADBMS_Print_Accumulator(void);

/*============================================================================
 * Error Handling
 *============================================================================*/

/**
 * @brief Get current error type
 * @return Error code (see ERROR_TYPE_* defines in FEB_Const.h)
 *         - 0x00: No error
 *         - 0x10: Temperature violation
 *         - 0x20: Low temperature reads
 *         - 0x30: Voltage violation
 *         - 0x40: Initialization failure
 */
uint8_t FEB_ADBMS_Get_Error_Type(void);

/**
 * @brief Update error type
 * @param error New error code
 */
void FEB_ADBMS_Update_Error_Type(uint8_t error);

#endif /* INC_FEB_ADBMS6830B_H_ */
