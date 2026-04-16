/**
 * @file FEB_ADBMS_App.h
 * @brief BMS Application Layer for ADBMS6830B
 * @author Formula Electric @ Berkeley
 *
 * High-level interface for battery monitoring, temperature sensing,
 * and cell balancing using the ADBMS6830B register driver.
 */

#ifndef FEB_ADBMS_APP_H
#define FEB_ADBMS_APP_H

#include <stdint.h>
#include <stdbool.h>
#include "BMS_HW_Config.h"

/*============================================================================
 * Operating Mode
 *============================================================================*/

/**
 * @brief Operating mode affects UV/OV thresholds
 */
typedef enum
{
  BMS_MODE_NORMAL = 0, /**< Normal driving operation */
  BMS_MODE_CHARGING,   /**< Actively charging - tighter limits */
  BMS_MODE_BALANCING   /**< Cell balancing active */
} BMS_OpMode_t;

/*============================================================================
 * Error Codes
 *============================================================================*/

/**
 * @brief Application layer error codes
 */
typedef enum
{
  BMS_APP_OK = 0,         /**< Success */
  BMS_APP_ERR_INIT,       /**< Initialization failed */
  BMS_APP_ERR_COMM,       /**< Communication error (PEC) */
  BMS_APP_ERR_VOLTAGE_UV, /**< Undervoltage detected */
  BMS_APP_ERR_VOLTAGE_OV, /**< Overvoltage detected */
  BMS_APP_ERR_TEMP_HIGH,  /**< Overtemperature */
  BMS_APP_ERR_TEMP_LOW,   /**< Undertemperature */
  BMS_APP_ERR_REDUNDANCY, /**< C-ADC vs S-ADC mismatch */
  BMS_APP_ERR_SENSOR      /**< Sensor fault (open/short) */
} BMS_AppError_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Per-cell data (processed values)
 */
typedef struct
{
  float voltage_C_V;      /**< Primary C-ADC voltage (volts) */
  float voltage_S_V;      /**< Redundant S-ADC voltage (volts) */
  uint8_t uv_count;       /**< Consecutive UV violations */
  uint8_t ov_count;       /**< Consecutive OV violations */
  uint8_t is_discharging; /**< Balancing discharge active */
} BMS_CellData_t;

/**
 * @brief Per-IC data
 */
typedef struct
{
  BMS_CellData_t cells[BMS_CELLS_PER_IC];
  float internal_temp_C; /**< IC die temperature */
  uint32_t pec_errors;   /**< Cumulative PEC errors */
  bool comm_ok;          /**< Last communication status */
  uint8_t serial_id[6];  /**< IC serial ID */
} BMS_ICData_t;

/**
 * @brief Per-bank data
 */
typedef struct
{
  BMS_ICData_t ics[BMS_ICS_PER_BANK];
  float temp_sensors_C[BMS_TEMP_TOTAL_SENSORS];    /**< Temperature readings */
  uint8_t temp_violations[BMS_TEMP_TOTAL_SENSORS]; /**< Per-sensor violation counts */
  float total_voltage_V;
  float min_voltage_V;
  float max_voltage_V;
  float min_temp_C;
  float max_temp_C;
  float avg_temp_C;
  uint8_t voltage_valid; /**< Voltage data valid flag */
  uint8_t temp_valid;    /**< Temperature data valid flag */
} BMS_BankData_t;

/**
 * @brief Pack-level data (global state)
 */
typedef struct
{
  BMS_BankData_t banks[BMS_NUM_BANKS];

  /* Pack aggregates */
  float pack_voltage_V;
  float pack_min_cell_V;
  float pack_max_cell_V;
  float pack_min_temp_C;
  float pack_max_temp_C;
  float pack_avg_temp_C;

  /* Error tracking */
  BMS_AppError_t last_error;
  uint8_t error_bank; /**< Bank index where error occurred */
  uint8_t error_ic;   /**< IC index where error occurred */
  uint8_t error_cell; /**< Cell index where error occurred */

  /* Statistics */
  uint32_t voltage_read_count;
  uint32_t temp_read_count;
  uint32_t total_pec_errors;

  /* State */
  BMS_OpMode_t mode;
  bool initialized;
  bool voltage_valid;
  bool temp_valid;
} BMS_PackData_t;

/** Global pack data - protected by ADBMSMutexHandle */
extern BMS_PackData_t g_bms_pack;

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Full startup initialization sequence
 *
 * Performs:
 * 1. Platform initialization (DWT, SPI)
 * 2. ADBMS driver initialization
 * 3. Wake-up and soft reset all ICs
 * 4. Configure UV/OV thresholds
 * 5. Read all registers to verify communication
 * 6. Validate serial IDs
 *
 * @return BMS_APP_OK on success, error code on failure
 */
BMS_AppError_t BMS_App_Init(void);

/*============================================================================
 * Periodic Processing (Call from FEB_Task_ADBMS)
 *============================================================================*/

/**
 * @brief Execute voltage measurement cycle (call at 10 Hz)
 *
 * 1. Start C-ADC and S-ADC conversions
 * 2. Poll for completion
 * 3. Read all cell voltages
 * 4. Validate against thresholds
 * 5. Update g_bms_pack
 *
 * @return BMS_APP_OK or error code
 */
BMS_AppError_t BMS_App_ProcessVoltage(void);

/**
 * @brief Execute temperature measurement cycle (call at 2 Hz)
 *
 * 1. For each MUX channel (0-6):
 *    a. Set GPO select lines
 *    b. Start AUX ADC
 *    c. Read GPIO voltages
 *    d. Convert to temperature
 * 2. Validate against thresholds
 * 3. Update g_bms_pack
 *
 * @return BMS_APP_OK or error code
 */
BMS_AppError_t BMS_App_ProcessTemperature(void);

/**
 * @brief Execute balancing iteration (call at 1 Hz when in BALANCE state)
 *
 * 1. Read current voltages
 * 2. Find minimum cell voltage
 * 3. Enable discharge on cells > min + threshold
 * 4. Write discharge config to ICs
 *
 * @return BMS_APP_OK or error code
 */
BMS_AppError_t BMS_App_ProcessBalancing(void);

/**
 * @brief Stop all balancing (call when exiting BALANCE state)
 */
void BMS_App_StopBalancing(void);

/*============================================================================
 * Mode Control
 *============================================================================*/

/**
 * @brief Set operating mode (changes UV/OV thresholds)
 * @param mode New operating mode
 */
void BMS_App_SetMode(BMS_OpMode_t mode);

/**
 * @brief Get current operating mode
 */
BMS_OpMode_t BMS_App_GetMode(void);

/*============================================================================
 * Getters - Lock-Free Snapshots
 *
 * These provide direct read access without mutex. Safe for single 32-bit
 * values on Cortex-M4 due to atomic load/store. For consistent multi-field
 * reads, caller should acquire ADBMSMutexHandle externally.
 *============================================================================*/

/* Voltage getters */
float BMS_App_GetPackVoltage(void);
float BMS_App_GetMinCellVoltage(void);
float BMS_App_GetMaxCellVoltage(void);
float BMS_App_GetCellVoltage(uint8_t bank, uint8_t ic, uint8_t cell);
float BMS_App_GetCellVoltageS(uint8_t bank, uint8_t ic, uint8_t cell);

/* Temperature getters */
float BMS_App_GetMinTemp(void);
float BMS_App_GetMaxTemp(void);
float BMS_App_GetAvgTemp(void);
float BMS_App_GetTempSensor(uint8_t bank, uint8_t sensor_idx);
float BMS_App_GetICTemp(uint8_t bank, uint8_t ic);

/* Status getters */
bool BMS_App_IsBalancingNeeded(void);
bool BMS_App_IsInitialized(void);
bool BMS_App_IsVoltageValid(void);
bool BMS_App_IsTempValid(void);

/* Error handling */
BMS_AppError_t BMS_App_GetLastError(void);
void BMS_App_ClearError(void);

/**
 * @brief Get read-only pointer to pack data
 * @return Pointer to global pack data structure
 * @note For read-only access; use mutex for thread-safe operations
 */
const BMS_PackData_t *BMS_App_GetPackData(void);

/*============================================================================
 * Diagnostics
 *============================================================================*/

/**
 * @brief Full register read and diagnostic dump
 * @param print_fn Printf-like function for output
 */
typedef int (*BMS_PrintFunc_t)(const char *fmt, ...);
void BMS_App_DumpDiagnostics(BMS_PrintFunc_t print_fn);

/**
 * @brief Get PEC error count for specific IC
 */
uint32_t BMS_App_GetICPECErrors(uint8_t bank, uint8_t ic);

/**
 * @brief Get total PEC errors across all ICs
 */
uint32_t BMS_App_GetTotalPECErrors(void);

/*============================================================================
 * Legacy Compatibility Functions
 *
 * These functions maintain compatibility with existing FEB_SM.c code that
 * called the deleted FEB_ADBMS6830B module.
 *============================================================================*/

/**
 * @brief Stop cell balancing (legacy name)
 * @note Calls BMS_App_StopBalancing() internally
 */
void FEB_Stop_Balance(void);

/**
 * @brief Get total pack voltage (legacy name)
 * @return Total pack voltage in volts
 */
float FEB_ADBMS_GET_ACC_Total_Voltage(void);

/**
 * @brief Update error type (legacy compatibility)
 * @param error_type Error code to set
 */
void FEB_ADBMS_Update_Error_Type(uint8_t error_type);

/**
 * @brief Get current error type (legacy compatibility)
 * @return Current error type code
 */
uint8_t FEB_ADBMS_Get_Error_Type(void);

#endif /* FEB_ADBMS_APP_H */
