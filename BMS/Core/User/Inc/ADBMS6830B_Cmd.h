/**
 * @file ADBMS6830B_Cmd.h
 * @brief ADBMS6830B Low-Level Command Interface
 * @author Formula Electric @ Berkeley
 *
 * @details
 * This module provides a clean, typed interface to all ADBMS6830B commands.
 * Each function maps directly to a command from the ADBMS6830B datasheet.
 *
 * All command codes match Table 50 from the ADBMS6830B datasheet.
 * Register structures are defined in ADBMS6830B_Registers.h.
 *
 * @par Thread Safety
 * Functions in this module are NOT thread-safe. Callers must acquire
 * ADBMSMutexHandle before calling any function in this module.
 *
 * @see ADBMS6830B Datasheet Rev. A
 * @see ADBMS6830B_Commands.h for command code definitions
 * @see ADBMS6830B_Registers.h for register structures
 */

#ifndef ADBMS6830B_CMD_H
#define ADBMS6830B_CMD_H

#include <stdint.h>
#include <stdbool.h>
#include "ADBMS6830B_Commands.h"
#include "ADBMS6830B_Registers.h"
#include "FEB_ADBMS6830B_Driver.h"

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum number of ICs in daisy chain */
#define ADBMS_MAX_IC 16

/** ADC conversion timeout in milliseconds */
#define ADBMS_ADC_TIMEOUT_MS 10

/** Open-wire detection modes */
typedef enum {
    ADBMS_OW_OFF = 0,      /**< Open-wire detection disabled */
    ADBMS_OW_PULLUP = 1,   /**< Open-wire with pull-up current */
    ADBMS_OW_PULLDOWN = 2  /**< Open-wire with pull-down current */
} ADBMS_OpenWire_t;

/** Auxiliary channel selection for ADAX command */
typedef enum {
    ADBMS_AUX_ALL = 0,     /**< Convert all GPIO channels */
    ADBMS_AUX_GPIO1 = 1,   /**< Convert GPIO1 only */
    ADBMS_AUX_GPIO2 = 2,   /**< Convert GPIO2 only */
    ADBMS_AUX_GPIO3 = 3,   /**< Convert GPIO3 only */
    ADBMS_AUX_GPIO4 = 4,   /**< Convert GPIO4 only */
    ADBMS_AUX_GPIO5 = 5,   /**< Convert GPIO5 only */
    ADBMS_AUX_GPIO6 = 6,   /**< Convert GPIO6 only */
    ADBMS_AUX_GPIO7 = 7,   /**< Convert GPIO7 only */
    ADBMS_AUX_GPIO8 = 8,   /**< Convert GPIO8 only */
    ADBMS_AUX_GPIO9 = 9,   /**< Convert GPIO9 only */
    ADBMS_AUX_GPIO10 = 10, /**< Convert GPIO10 only */
    ADBMS_AUX_VREF2 = 11,  /**< Convert VREF2 only */
    ADBMS_AUX_ITEMP = 12   /**< Convert internal temperature only */
} ADBMS_AuxChannel_t;

/*============================================================================
 * Per-IC Data Structure
 *============================================================================*/

/**
 * @brief Per-IC raw register data
 *
 * Stores all register data for a single ADBMS6830B IC.
 * This structure is populated by read operations and used for write operations.
 */
typedef struct {
    /* Configuration registers */
    ADBMS_CFGA_t cfgA;           /**< Configuration Register A (read/write) */
    ADBMS_CFGB_t cfgB;           /**< Configuration Register B (read/write) */

    /* Cell voltage data (read-only from device) */
    uint16_t c_codes[18];        /**< C-ADC cell voltage codes (primary) */
    uint16_t s_codes[18];        /**< S-ADC cell voltage codes (redundant) */

    /* Auxiliary data (read-only from device) */
    uint16_t aux_codes[10];      /**< GPIO/auxiliary voltage codes */

    /* Status registers (read-only from device) */
    ADBMS_STATA_t statA;         /**< Status Register A */
    ADBMS_STATB_t statB;         /**< Status Register B */

    /* PWM registers (read/write) */
    ADBMS_PWM_t pwmA;            /**< PWM Register A (cells 1-12) */
    ADBMS_PWM_t pwmB;            /**< PWM Register B (cells 13-18) */

    /* Serial ID (read-only, factory programmed) */
    uint8_t serial_id[6];        /**< 48-bit unique serial ID */

    /* Error tracking */
    uint32_t pec_errors;         /**< Cumulative PEC error count */
    uint8_t last_pec_status;     /**< PEC status of last operation (0=OK) */
} ADBMS_IC_t;

/*============================================================================
 * Initialization Functions
 *============================================================================*/

/**
 * @brief Initialize ADBMS IC data structures with default values
 *
 * Sets up default configuration for all ICs:
 * - REFON enabled (reference always on)
 * - CTH comparison thresholds set
 * - UV/OV thresholds set to 2.0V/5.0V
 * - Discharge disabled
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of IC data structures to initialize
 *
 * @note This only initializes the software structures. Call ADBMS_WriteConfig()
 *       to actually write the configuration to the hardware.
 */
void ADBMS_InitDefaults(uint8_t total_ic, ADBMS_IC_t ic[]);

/**
 * @brief Write configuration registers A and B to all ICs
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of IC data structures containing configuration to write
 * @return 0 on success, negative on error
 */
int ADBMS_WriteConfig(uint8_t total_ic, const ADBMS_IC_t ic[]);

/**
 * @brief Read configuration registers A and B from all ICs
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of IC data structures to store read configuration
 * @return 0 on success, number of PEC errors on partial success, negative on error
 */
int ADBMS_ReadConfig(uint8_t total_ic, ADBMS_IC_t ic[]);

/*============================================================================
 * ADC Conversion Commands
 *============================================================================*/

/**
 * @brief Start cell voltage ADC conversion (C-ADC)
 *
 * Initiates ADC conversion on all cell voltage inputs using the primary C-ADC.
 * Conversion takes approximately 1-2ms.
 *
 * @param continuous Enable continuous conversion mode (repeated conversions)
 * @param discharge_permit Allow cell discharge during measurement
 * @param ow_mode Open-wire detection mode
 *
 * @note Use ADBMS_PollADC() or a delay to wait for conversion completion
 *       before reading results with ADBMS_ReadCellVoltages().
 *
 * @see ADBMS_ReadCellVoltages() to read conversion results
 */
void ADBMS_StartCellADC(bool continuous, bool discharge_permit, ADBMS_OpenWire_t ow_mode);

/**
 * @brief Start S-voltage ADC conversion (redundant S-ADC)
 *
 * Initiates ADC conversion using the redundant S-ADC.
 * Results are stored in s_codes[] and can be compared with c_codes[]
 * for redundancy checking.
 *
 * @note Conversion takes approximately 1-2ms.
 *
 * @see ADBMS_ReadSVoltages() to read conversion results
 */
void ADBMS_StartSADC(void);

/**
 * @brief Start auxiliary ADC conversion (GPIO/temperature)
 *
 * Initiates ADC conversion on GPIO/auxiliary inputs.
 *
 * @param channel Which GPIO channel(s) to convert
 * @param open_wire Enable open-wire detection
 * @param pullup Use pull-up current (false = pull-down)
 *
 * @see ADBMS_ReadAux() to read conversion results
 */
void ADBMS_StartAuxADC(ADBMS_AuxChannel_t channel, bool open_wire, bool pullup);

/**
 * @brief Poll ADC conversion status
 *
 * Checks if the ADC conversion is complete.
 *
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return 0 if conversion complete, 1 if still converting, negative on error
 *
 * @note Prefer using osDelay() in RTOS environments to avoid blocking.
 */
int ADBMS_PollADC(uint32_t timeout_ms);

/*============================================================================
 * Read Functions
 *============================================================================*/

/**
 * @brief Read cell voltages from all ICs (primary C-ADC)
 *
 * Reads the cell voltage register groups CVA-CVF and stores the results
 * in ic[].c_codes[].
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of IC data structures to store results
 * @return 0 on success, number of PEC errors on partial success
 *
 * @note Call ADBMS_StartCellADC() and wait for conversion before reading.
 */
int ADBMS_ReadCellVoltages(uint8_t total_ic, ADBMS_IC_t ic[]);

/**
 * @brief Read S-voltages from all ICs (redundant S-ADC)
 *
 * Reads the S-voltage register groups SVA-SVF and stores the results
 * in ic[].s_codes[].
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of IC data structures to store results
 * @return 0 on success, number of PEC errors on partial success
 *
 * @note Call ADBMS_StartSADC() and wait for conversion before reading.
 */
int ADBMS_ReadSVoltages(uint8_t total_ic, ADBMS_IC_t ic[]);

/**
 * @brief Read auxiliary registers from all ICs
 *
 * Reads the auxiliary register groups AUXA-AUXD and stores the results
 * in ic[].aux_codes[].
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of IC data structures to store results
 * @return 0 on success, number of PEC errors on partial success
 *
 * @note Call ADBMS_StartAuxADC() and wait for conversion before reading.
 */
int ADBMS_ReadAux(uint8_t total_ic, ADBMS_IC_t ic[]);

/**
 * @brief Read status registers from all ICs
 *
 * Reads status register groups STATA and STATB and stores the results
 * in ic[].statA and ic[].statB.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of IC data structures to store results
 * @return 0 on success, number of PEC errors on partial success
 */
int ADBMS_ReadStatus(uint8_t total_ic, ADBMS_IC_t ic[]);

/**
 * @brief Read serial IDs from all ICs
 *
 * Reads the factory-programmed 48-bit serial ID from each IC.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of IC data structures to store results
 * @return 0 on success, number of PEC errors on partial success
 */
int ADBMS_ReadSerialID(uint8_t total_ic, ADBMS_IC_t ic[]);

/*============================================================================
 * Write Functions
 *============================================================================*/

/**
 * @brief Write PWM registers to control cell discharge
 *
 * Writes PWM duty cycle settings for cell discharge FETs.
 * Each cell's duty cycle is controlled by a 4-bit value (0-15 = 0-100%).
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of IC data structures containing PWM settings
 * @return 0 on success, negative on error
 */
int ADBMS_WritePWM(uint8_t total_ic, const ADBMS_IC_t ic[]);

/*============================================================================
 * Control Commands
 *============================================================================*/

/**
 * @brief Clear cell voltage registers
 *
 * Clears all cell voltage measurement registers (CVA-CVF).
 */
void ADBMS_ClearCells(void);

/**
 * @brief Clear auxiliary registers
 *
 * Clears all auxiliary/GPIO measurement registers (AUXA-AUXD).
 */
void ADBMS_ClearAux(void);

/**
 * @brief Clear all flags
 *
 * Clears UV/OV and other fault flags.
 */
void ADBMS_ClearFlags(void);

/**
 * @brief Mute discharge
 *
 * Temporarily disables cell discharge without changing DCC configuration.
 */
void ADBMS_Mute(void);

/**
 * @brief Unmute discharge
 *
 * Re-enables cell discharge according to DCC configuration.
 */
void ADBMS_Unmute(void);

/**
 * @brief Soft reset all ICs
 *
 * Performs a soft reset of all ICs in the daisy chain.
 * All registers return to default values.
 */
void ADBMS_SoftReset(void);

/*============================================================================
 * Configuration Helpers
 *============================================================================*/

/**
 * @brief Set discharge cell control bits
 *
 * Updates the DCC bits in CFGB to control which cells are discharging.
 *
 * @param ic Pointer to IC data structure
 * @param dcc_mask 16-bit mask where bit N enables discharge for cell N+1
 */
void ADBMS_SetDischarge(ADBMS_IC_t *ic, uint16_t dcc_mask);

/**
 * @brief Set undervoltage threshold
 *
 * @param ic Pointer to IC data structure
 * @param voltage_mV Undervoltage threshold in millivolts
 */
void ADBMS_SetUVThreshold(ADBMS_IC_t *ic, uint16_t voltage_mV);

/**
 * @brief Set overvoltage threshold
 *
 * @param ic Pointer to IC data structure
 * @param voltage_mV Overvoltage threshold in millivolts
 */
void ADBMS_SetOVThreshold(ADBMS_IC_t *ic, uint16_t voltage_mV);

/**
 * @brief Set GPIO pull-down control bits
 *
 * @param ic Pointer to IC data structure
 * @param gpio_mask 10-bit mask controlling GPIO pull-downs
 */
void ADBMS_SetGPIO(ADBMS_IC_t *ic, uint16_t gpio_mask);

/*============================================================================
 * Conversion Utilities
 *============================================================================*/

/* Note: Voltage conversion functions are defined in ADBMS6830B_Registers.h:
 *   - ADBMS_CodeToVoltage_mV(code) - Convert to millivolts
 *   - ADBMS_CodeToTemp_C(code) - Convert ITMP to Celsius
 *   - ADBMS_VoltageToThreshold(mV) - Convert to threshold code
 */

/**
 * @brief Check if serial ID is valid
 *
 * A valid serial ID is not all zeros and not all 0xFF.
 *
 * @param sid 6-byte serial ID array
 * @return true if valid, false if invalid
 */
bool ADBMS_IsSerialIDValid(const uint8_t sid[6]);

#endif /* ADBMS6830B_CMD_H */
