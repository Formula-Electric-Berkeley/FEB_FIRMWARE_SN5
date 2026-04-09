/**
 * @file FEB_ADBMS6830B_Driver.h
 * @brief Low-Level Driver Interface for ADBMS6830B Battery Monitor
 * @author Formula Electric @ Berkeley
 *
 * @details
 * This module provides the low-level driver interface for communicating with
 * ADBMS6830B battery monitor ICs over isoSPI. It handles:
 * - Configuration register read/write operations
 * - ADC conversion commands and polling
 * - Cell voltage and auxiliary register parsing
 * - PEC (Packet Error Code) error tracking
 *
 * @par Architecture
 * This driver sits between the SPI interface (FEB_AD68xx_Interface) and the
 * BMS application layer (FEB_ADBMS6830B). It manages the cell_asic data
 * structure which stores all per-IC register data.
 *
 * @par Thread Safety
 * Functions in this module are NOT thread-safe. Callers must acquire
 * ADBMSMutexHandle before calling any function in this module.
 *
 * @par Data Flow
 * @code
 * FEB_ADBMS6830B.c (BMS Application)
 *        │
 *        ▼
 * FEB_ADBMS6830B_Driver.c (This module - register operations)
 *        │
 *        ▼
 * FEB_AD68xx_Interface.c (SPI transmission)
 *        │
 *        ▼
 * ADBMS6830B Hardware (isoSPI daisy chain)
 * @endcode
 *
 * @see ADBMS6830B Datasheet Rev. A
 * @see ADBMS6830B_Commands.h for command code definitions
 * @see ADBMS6830B_Cmd.h for the newer typed command API
 */

#ifndef INC_FEB_ADBMS6830B_DRIVER_H_
#define INC_FEB_ADBMS6830B_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

/*============================================================================
 * Constants and Macros
 *============================================================================*/

/** @defgroup ADBMS_Driver_Constants Driver Constants
 *  @{
 */

/** Number of bytes received per register read (6 data + 2 PEC) */
#define NUM_RX_BYT 8

/** Size of register data payload (excluding PEC) */
#define REG_DATA_SIZE 6

/** Size of register with PEC bytes */
#define REG_WITH_PEC_SIZE 8

/** Maximum write buffer size for daisy chain operations */
#define MAX_WRITE_BUFFER_SIZE 256

/** ADC polling timeout in microseconds */
#define ADC_POLL_TIMEOUT 200000

/** ADC polling increment in microseconds */
#define ADC_POLL_INCREMENT 10

/** @} */ /* end of ADBMS_Driver_Constants */

/** @defgroup ADBMS_Register_Types Register Type Identifiers
 *  Used to identify register types for PEC error tracking
 *  @{
 */
#define CFGRA 0  /**< Configuration Register A */
#define CFGRB 1  /**< Configuration Register B */
#define CELL  2  /**< Cell Voltage Registers */
#define AUX   3  /**< Auxiliary Registers */
#define STAT  4  /**< Status Registers */
/** @} */

/** @defgroup ADBMS_Bit_Masks Bit Masks for Configuration
 *  @{
 */
#define REFON_MASK        0x80  /**< REFON bit position in CFGA[0] */
#define REFON_CLEAR_MASK  0x7F  /**< Mask to clear REFON bit */
#define DCC_LOW_MASK      0xFF  /**< Mask for DCC bits 0-7 */
#define DCC_HIGH_MASK     0xFF  /**< Mask for DCC bits 8-15 */
#define BYTE_MASK         0xFF  /**< Full byte mask */
#define NIBBLE_MASK       0x0F  /**< Lower nibble mask */
#define HALF_BYTE_SHIFT   4     /**< Shift for upper nibble */
/** @} */

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Cell voltage data storage
 *
 * Stores both primary (C-ADC) and redundant (S-ADC) cell voltage codes
 * along with PEC match status for each register read.
 */
typedef struct
{
    uint16_t c_codes[18];  /**< Primary C-ADC cell voltage codes (150µV/LSB) */
    uint16_t s_codes[18];  /**< Redundant S-ADC cell voltage codes (150µV/LSB) */
    uint8_t pec_match[6];  /**< PEC status for CVA-CVF registers (0=match, 1=error) */
} cv;

/**
 * @brief Auxiliary register voltage data
 *
 * Stores GPIO/auxiliary ADC readings for temperature sensing
 * and other analog inputs.
 */
typedef struct
{
    uint16_t a_codes[10];  /**< Auxiliary voltage codes GPIO1-10 (150µV/LSB) */
    uint8_t pec_match[4];  /**< PEC status for AUXA-AUXD registers */
} ax;

/**
 * @brief Status register data
 *
 * Contains IC status information including sum-of-cells voltage,
 * internal temperature, UV/OV flags, and diagnostic results.
 */
typedef struct
{
    uint16_t stat_codes[4];  /**< Status codes: SC, ITMP, VA, VD */
    uint8_t flags[3];        /**< UV/OV flag data for cells */
    uint8_t mux_fail[1];     /**< MUX self-test status (1=fail) */
    uint8_t thsd[1];         /**< Thermal shutdown status (1=triggered) */
    uint8_t pec_match[2];    /**< PEC status for STATA/STATB */
} st;

/**
 * @brief Generic IC register buffer
 *
 * Used for register read/write operations where typed access
 * is not needed.
 */
typedef struct
{
    uint8_t tx_data[6];    /**< Data to be transmitted (6 bytes) */
    uint8_t rx_data[8];    /**< Received data (6 data + 2 PEC) */
    uint8_t rx_pec_match;  /**< PEC match status of last read (0=OK) */
} ic_register;

/**
 * @brief PEC error tracking counters
 *
 * Tracks cumulative PEC errors by register type for diagnostics.
 * High PEC counts may indicate communication issues or noise.
 */
typedef struct
{
    uint16_t pec_count;    /**< Total PEC errors across all registers */
    uint16_t cfgr_pec;     /**< Configuration register PEC errors */
    uint16_t cell_pec[6];  /**< Cell voltage register PEC errors (CVA-CVF) */
    uint16_t aux_pec[4];   /**< Auxiliary register PEC errors (AUXA-AUXD) */
    uint16_t stat_pec[2];  /**< Status register PEC errors (STATA-STATB) */
} pec_counter;

/**
 * @brief Register configuration limits
 *
 * Defines the number of channels and registers for this IC variant.
 * ADBMS6830B supports up to 18 cells, 10 GPIO, and various status codes.
 */
typedef struct
{
    uint8_t cell_channels;  /**< Number of cell voltage channels (18 max) */
    uint8_t stat_channels;  /**< Number of status channels */
    uint8_t aux_channels;   /**< Number of auxiliary channels (10 max) */
    uint8_t num_cv_reg;     /**< Number of cell voltage registers (6: CVA-CVF) */
    uint8_t num_gpio_reg;   /**< Number of auxiliary registers (4: AUXA-AUXD) */
    uint8_t num_stat_reg;   /**< Number of status registers (2: STATA-STATB) */
} register_cfg;

/**
 * @brief Complete IC data structure
 *
 * Master structure containing all register data and state for a single
 * ADBMS6830B IC. An array of these structures represents the daisy chain.
 *
 * @note This structure is ~200 bytes per IC. For a 16-IC chain,
 *       total memory usage is approximately 3.2KB.
 */
typedef struct
{
    ic_register configa;     /**< Configuration Register A data */
    ic_register configb;     /**< Configuration Register B data */
    cv cells;                /**< Cell voltage measurement data */
    ax aux;                  /**< Auxiliary/GPIO measurement data */
    st stat;                 /**< Status register data */
    ic_register com;         /**< Communication register (COMM) */
    ic_register pwm;         /**< PWM Register A (cells 1-12) */
    ic_register pwmb;        /**< PWM Register B (cells 13-18) */
    ic_register sctrl;       /**< S Control Register A */
    ic_register sctrlb;      /**< S Control Register B */
    uint8_t sid[6];          /**< Factory-programmed 48-bit serial ID */
    bool isospi_reverse;     /**< isoSPI direction flag */
    pec_counter crc_count;   /**< Cumulative PEC error counters */
    register_cfg ic_reg;     /**< Register configuration limits */
    long system_open_wire;   /**< Open-wire detection results */
} cell_asic;

/*============================================================================
 * Initialization Functions
 *============================================================================*/

/**
 * @brief Initialize configuration registers with defaults
 *
 * Sets up the cell_asic structures with default configuration values:
 * - REFON enabled (reference always on for faster measurements)
 * - All GPIO pull-downs enabled
 * - Default UV/OV thresholds
 * - Discharge disabled
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to initialize
 *
 * @note This only initializes software structures. Call ADBMS6830B_wrcfga()
 *       and ADBMS6830B_wrcfgb() to write configuration to hardware.
 */
void ADBMS6830B_init_cfg(uint8_t total_ic, cell_asic *ic);

/**
 * @brief Initialize register limits for ADBMS6830B
 *
 * Sets the register_cfg structure with ADBMS6830B-specific limits:
 * - 18 cell channels
 * - 10 auxiliary channels
 * - 6 cell voltage registers (CVA-CVF)
 * - 4 auxiliary registers (AUXA-AUXD)
 * - 2 status registers (STATA-STATB)
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to configure
 */
void ADBMS6830B_init_reg_limits(uint8_t total_ic, cell_asic *ic);

/**
 * @brief Reset all PEC error counters
 *
 * Clears cumulative PEC error counts for all register types.
 * Useful for starting a fresh monitoring period.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures
 */
void ADBMS6830B_reset_crc_count(uint8_t total_ic, cell_asic *ic);

/*============================================================================
 * Configuration Helper Functions
 *============================================================================*/

/**
 * @brief Set all configuration register fields
 *
 * Comprehensive function to set all CFGA and CFGB fields at once.
 *
 * @param nIC IC index (0 to total_ic-1)
 * @param ic Array of cell_asic structures
 * @param refon Reference enable (true = always on)
 * @param cth Comparison threshold bits [0:2]
 * @param gpio GPIO pull-down enables [0:9]
 * @param dcc Discharge cell control bits (bit N = cell N+1)
 * @param dcto Discharge timeout bits [0:5]
 * @param uv Undervoltage threshold code
 * @param ov Overvoltage threshold code
 */
void ADBMS6830B_set_cfgr(uint8_t nIC, cell_asic *ic, bool refon,
                         bool cth[3], bool gpio[10], uint16_t dcc,
                         bool dcto[6], uint16_t uv, uint16_t ov);

/**
 * @brief Set REFON bit (reference enable)
 *
 * When enabled, the internal reference stays on between conversions
 * for faster measurements but higher power consumption.
 *
 * @param nIC IC index
 * @param ic Array of cell_asic structures
 * @param refon true to enable, false to disable
 */
void ADBMS6830B_set_cfgr_refon(uint8_t nIC, cell_asic *ic, bool refon);

/**
 * @brief Set comparison threshold bits
 *
 * @param nIC IC index
 * @param ic Array of cell_asic structures
 * @param cth Array of 3 CTH bits
 */
void ADBMS6830B_set_cfgr_cth(uint8_t nIC, cell_asic *ic, bool cth[3]);

/**
 * @brief Set GPIO pull-down control bits
 *
 * Controls the internal pull-down resistors on GPIO pins.
 * Disable pull-downs when using GPIOs for thermistor measurements.
 *
 * @param nIC IC index
 * @param ic Array of cell_asic structures
 * @param gpio Array of 10 GPIO enable bits
 */
void ADBMS6830B_set_cfgr_gpio(uint8_t nIC, cell_asic *ic, bool gpio[10]);

/**
 * @brief Set discharge cell control bits
 *
 * Controls which cells are actively discharging for balancing.
 * Bit N corresponds to cell N+1 (bit 0 = cell 1).
 *
 * @param nIC IC index
 * @param ic Array of cell_asic structures
 * @param dcc 16-bit discharge mask (cells 1-16)
 */
void ADBMS6830B_set_cfgr_dis(uint8_t nIC, cell_asic *ic, uint16_t dcc);

/**
 * @brief Set discharge timeout bits
 *
 * Configures automatic discharge timeout for safety.
 *
 * @param nIC IC index
 * @param ic Array of cell_asic structures
 * @param dcto Array of 6 timeout bits
 */
void ADBMS6830B_set_cfgr_dcto(uint8_t nIC, cell_asic *ic, bool dcto[6]);

/**
 * @brief Set undervoltage threshold
 *
 * @param nIC IC index
 * @param ic Array of cell_asic structures
 * @param uv Undervoltage threshold code (threshold = (uv+1) * 16 * 150µV)
 */
void ADBMS6830B_set_cfgr_uv(uint8_t nIC, cell_asic *ic, uint16_t uv);

/**
 * @brief Set overvoltage threshold
 *
 * @param nIC IC index
 * @param ic Array of cell_asic structures
 * @param ov Overvoltage threshold code (threshold = ov * 16 * 150µV)
 */
void ADBMS6830B_set_cfgr_ov(uint8_t nIC, cell_asic *ic, uint16_t ov);

/*============================================================================
 * ADC Conversion Functions
 *============================================================================*/

/**
 * @brief Start cell voltage ADC conversion (ADCV command)
 *
 * Initiates ADC conversion on all cell voltage inputs. The conversion
 * mode can be configured via parameters.
 *
 * @param RD Redundancy mode (0=single, 1=redundant dual-ADC)
 * @param DCP Discharge permit (0=off during conversion, 1=on)
 * @param CONT Continuous mode (0=single, 1=continuous)
 * @param RSTF Reset filter (0=no reset, 1=reset before conversion)
 * @param OW Open-wire detection mode (0=off, 1=even, 2=odd, 3=all)
 *
 * @note Conversion takes ~1-2ms. Use ADBMS6830B_pollAdc() to wait.
 */
void ADBMS6830B_adcv(uint8_t RD, uint8_t DCP, uint8_t CONT,
                     uint8_t RSTF, uint8_t OW);

/**
 * @brief Poll ADC conversion status
 *
 * Blocks until ADC conversion completes or timeout occurs.
 * The SDO line goes high when conversion is complete.
 *
 * @return Approximate conversion time in microseconds
 *
 * @note In RTOS environments, prefer osDelay() over this blocking function.
 */
uint32_t ADBMS6830B_pollAdc(void);

/**
 * @brief Read cell voltages from all ICs (C-ADC)
 *
 * Reads cell voltage registers CVA-CVF and parses the data into
 * ic[].cells.c_codes[]. Also updates PEC match status.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to store results
 * @return 0 on success, non-zero on PEC error
 *
 * @note Call ADBMS6830B_adcv() and wait before reading.
 */
uint8_t ADBMS6830B_rdcv(uint8_t total_ic, cell_asic *ic);

/**
 * @brief Read S-voltages from all ICs (redundant S-ADC)
 *
 * Reads redundant S-voltage registers SVA-SVF and parses into
 * ic[].cells.s_codes[]. Used for dual-ADC verification.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to store results
 * @return 0 on success, non-zero on PEC error
 */
uint8_t ADBMS6830B_rdsv(uint8_t total_ic, cell_asic *ic);

/**
 * @brief Parse raw cell voltage data
 *
 * Helper function to convert raw register bytes to 16-bit voltage codes.
 *
 * @param current_ic Current IC index
 * @param cell_reg Register index (0-5 for CVA-CVF)
 * @param cell_data Raw register data (6 bytes)
 * @param cell_codes Output array for parsed voltage codes
 * @param ic_pec PEC match status output
 * @return 0 if PEC matches, -1 if PEC error
 */
int8_t parse_cells(uint8_t current_ic, uint8_t cell_reg,
                   uint8_t cell_data[], uint16_t *cell_codes, uint8_t *ic_pec);

/*============================================================================
 * Auxiliary ADC Functions
 *============================================================================*/

/**
 * @brief Start auxiliary ADC conversion (ADAX command)
 *
 * Initiates ADC conversion on GPIO/auxiliary inputs for temperature
 * sensing and other analog measurements.
 *
 * @param OW Open-wire detection enable (0=off, 1=on)
 * @param PUP Pull-up/pull-down during measurement (0=down, 1=up)
 * @param CH Channel selection (0=all, 1-10=specific GPIO, 11=VREF2, etc.)
 */
void ADBMS6830B_adax(uint8_t OW, uint8_t PUP, uint8_t CH);

/**
 * @brief Read auxiliary registers from all ICs
 *
 * Reads auxiliary registers AUXA-AUXD and parses GPIO voltage codes
 * into ic[].aux.a_codes[].
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to store results
 * @return 0 on success, non-zero on PEC error
 */
uint8_t ADBMS6830B_rdaux(uint8_t total_ic, cell_asic *ic);

/*============================================================================
 * Configuration Register Functions
 *============================================================================*/

/**
 * @brief Write all registers to all ICs
 *
 * Bulk write operation for configuration registers.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures containing data to write
 */
void ADBMS6830B_wrALL(uint8_t total_ic, cell_asic ic[]);

/**
 * @brief Read all registers from all ICs
 *
 * Bulk read operation for configuration registers.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to store results
 */
void ADBMS6830B_rdALL(uint8_t total_ic, cell_asic ic[]);

/**
 * @brief Write Configuration Register A (WRCFGA)
 *
 * Writes CFGA to all ICs in descending order (last IC first).
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures containing CFGA data
 */
void ADBMS6830B_wrcfga(uint8_t total_ic, cell_asic *ic);

/**
 * @brief Read Configuration Register A (RDCFGA)
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to store results
 */
void ADBMS6830B_rdcfga(uint8_t total_ic, cell_asic ic[]);

/**
 * @brief Write Configuration Register B (WRCFGB)
 *
 * Writes CFGB containing UV/OV thresholds and DCC bits.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures containing CFGB data
 */
void ADBMS6830B_wrcfgb(uint8_t total_ic, cell_asic *ic);

/**
 * @brief Read Configuration Register B (RDCFGB)
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to store results
 */
void ADBMS6830B_rdcfgb(uint8_t total_ic, cell_asic ic[]);

/*============================================================================
 * PWM Register Functions
 *============================================================================*/

/**
 * @brief Write PWM Register A (cells 1-12)
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures containing PWM data
 */
void ADBMS6830B_wrpwmga(uint8_t total_ic, cell_asic ic[]);

/**
 * @brief Read PWM Register A
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to store results
 */
void ADBMS6830B_rdpwmga(uint8_t total_ic, cell_asic ic[]);

/**
 * @brief Write PWM Register B (cells 13-18)
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures containing PWM data
 */
void ADBMS6830B_wrpwmgb(uint8_t total_ic, cell_asic ic[]);

/**
 * @brief Read PWM Register B
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to store results
 */
void ADBMS6830B_rdpwmgb(uint8_t total_ic, cell_asic ic[]);

/*============================================================================
 * Serial ID and Diagnostic Functions
 *============================================================================*/

/**
 * @brief Read unique serial ID from each IC
 *
 * Reads the factory-programmed 48-bit serial ID which uniquely
 * identifies each ADBMS6830B device.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param ic Array of cell_asic structures to store serial IDs
 * @return 0 on success, non-zero on PEC error
 */
uint8_t ADBMS6830B_rdsid(uint8_t total_ic, cell_asic *ic);

/**
 * @brief Check and accumulate PEC errors
 *
 * Compares received PEC with calculated PEC and updates error counters.
 *
 * @param total_ic Number of ICs in the daisy chain
 * @param reg Register type (CFGRA, CFGRB, CELL, AUX, STAT)
 * @param ic Array of cell_asic structures
 */
void ADBMS6830B_check_pec(uint8_t total_ic, uint8_t reg, cell_asic *ic);

/**
 * @brief Clear all flags (CLRFLAG command)
 *
 * Clears UV/OV flags and other diagnostic flags in all ICs.
 *
 * @param total_ic Number of ICs in the daisy chain
 */
void ADBMS6830B_CLRFLAG(uint8_t total_ic);

#endif /* INC_FEB_ADBMS6830B_DRIVER_H_ */
