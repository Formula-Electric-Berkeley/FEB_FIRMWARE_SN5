/**
 * @file ADBMS6830B_Registers.h
 * @brief Complete ADBMS6830B Register Interface - Self-Contained Driver
 *
 * This driver provides a complete byte-level mirror of the ADBMS6830B memory map.
 * All SPI communication and PEC handling is internal to this module.
 *
 * To integrate:
 * 1. Implement the weak platform functions (ADBMS_Platform_*) for your hardware
 * 2. Call ADBMS_Init() with the number of ICs in your daisy chain
 * 3. Use ADBMS_ReadXxx/ADBMS_WriteXxx functions to access registers
 *
 * @see ADBMS6830B_Memory_Map.h for register structure definitions
 * @see ADBMS6830B_Commands.h for command codes
 */

#ifndef ADBMS6830B_REGISTERS_H
#define ADBMS6830B_REGISTERS_H

#include "ADBMS6830B_Commands.h"
#include "ADBMS6830B_Memory_Map.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*============================================================================
 * Configuration
 *============================================================================*/

/** Maximum number of ICs supported in daisy chain */
#ifndef ADBMS_MAX_ICS
#define ADBMS_MAX_ICS 16
#endif

/** Number of cells per IC */
#define ADBMS_NUM_CELLS 16

/** Number of GPIO pins per IC */
#define ADBMS_NUM_GPIO 10

/** PEC sizes */
#define ADBMS_PEC15_SIZE 2
#define ADBMS_PEC10_SIZE 2

/** Command size */
#define ADBMS_CMD_SIZE 2

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum
{
  ADBMS_OK = 0,             /**< Success */
  ADBMS_ERR_PEC,            /**< PEC mismatch on received data */
  ADBMS_ERR_SPI,            /**< SPI communication failure */
  ADBMS_ERR_TIMEOUT,        /**< ADC conversion timeout */
  ADBMS_ERR_INVALID_PARAM,  /**< Invalid parameter */
  ADBMS_ERR_NOT_INITIALIZED /**< Driver not initialized */
} ADBMS_Error_t;

/*============================================================================
 * Platform Status Codes (for SPI hooks)
 *============================================================================*/

/**
 * @brief Return codes for platform SPI hooks
 */
typedef enum
{
  ADBMS_PLATFORM_OK = 0,          /**< Success */
  ADBMS_PLATFORM_ERR_SPI = -1,    /**< Generic SPI error */
  ADBMS_PLATFORM_ERR_TIMEOUT = -2 /**< SPI timeout */
} ADBMS_PlatformStatus_t;

/*============================================================================
 * Register Group Identifiers
 *============================================================================*/

typedef enum
{
  /* Configuration Registers (R/W) */
  ADBMS_REG_CFGA = 0, /**< Configuration Register Group A */
  ADBMS_REG_CFGB,     /**< Configuration Register Group B */

  /* Cell Voltage Registers (R/O) */
  ADBMS_REG_CVA,   /**< Cell Voltage A (C1-C3) */
  ADBMS_REG_CVB,   /**< Cell Voltage B (C4-C6) */
  ADBMS_REG_CVC,   /**< Cell Voltage C (C7-C9) */
  ADBMS_REG_CVD,   /**< Cell Voltage D (C10-C12) */
  ADBMS_REG_CVE,   /**< Cell Voltage E (C13-C15) */
  ADBMS_REG_CVF,   /**< Cell Voltage F (C16) */
  ADBMS_REG_CVALL, /**< All Cell Voltages (bulk read) */

  /* Averaged Cell Voltage Registers (R/O) */
  ADBMS_REG_ACA,   /**< Averaged Cell A */
  ADBMS_REG_ACB,   /**< Averaged Cell B */
  ADBMS_REG_ACC,   /**< Averaged Cell C */
  ADBMS_REG_ACD,   /**< Averaged Cell D */
  ADBMS_REG_ACE,   /**< Averaged Cell E */
  ADBMS_REG_ACF,   /**< Averaged Cell F */
  ADBMS_REG_ACALL, /**< All Averaged Cells (bulk read) */

  /* S-Voltage Registers (R/O) - Redundant ADC */
  ADBMS_REG_SVA,   /**< S-Voltage A */
  ADBMS_REG_SVB,   /**< S-Voltage B */
  ADBMS_REG_SVC,   /**< S-Voltage C */
  ADBMS_REG_SVD,   /**< S-Voltage D */
  ADBMS_REG_SVE,   /**< S-Voltage E */
  ADBMS_REG_SVF,   /**< S-Voltage F */
  ADBMS_REG_SVALL, /**< All S-Voltages (bulk read) */

  /* Filtered Cell Voltage Registers (R/O) */
  ADBMS_REG_FCA,   /**< Filtered Cell A */
  ADBMS_REG_FCB,   /**< Filtered Cell B */
  ADBMS_REG_FCC,   /**< Filtered Cell C */
  ADBMS_REG_FCD,   /**< Filtered Cell D */
  ADBMS_REG_FCE,   /**< Filtered Cell E */
  ADBMS_REG_FCF,   /**< Filtered Cell F */
  ADBMS_REG_FCALL, /**< All Filtered Cells (bulk read) */

  /* Auxiliary Registers (R/O) */
  ADBMS_REG_AUXA, /**< Auxiliary A (GPIO 1-3) */
  ADBMS_REG_AUXB, /**< Auxiliary B (GPIO 4-6) */
  ADBMS_REG_AUXC, /**< Auxiliary C (GPIO 7-9) */
  ADBMS_REG_AUXD, /**< Auxiliary D (GPIO 10, VMV, VPV) */

  /* Redundant Auxiliary Registers (R/O) */
  ADBMS_REG_RAXA, /**< Redundant Auxiliary A */
  ADBMS_REG_RAXB, /**< Redundant Auxiliary B */
  ADBMS_REG_RAXC, /**< Redundant Auxiliary C */
  ADBMS_REG_RAXD, /**< Redundant Auxiliary D */

  /* Status Registers (R/O) */
  ADBMS_REG_STATA, /**< Status A (VREF2, ITMP) */
  ADBMS_REG_STATB, /**< Status B (VD, VA, VRES) */
  ADBMS_REG_STATC, /**< Status C (Faults, Flags) */
  ADBMS_REG_STATD, /**< Status D (UV/OV Flags) */
  ADBMS_REG_STATE, /**< Status E (GPI, REV) */

  /* PWM Registers (R/W) */
  ADBMS_REG_PWMA, /**< PWM A (Cells 1-12) */
  ADBMS_REG_PWMB, /**< PWM B (Cells 13-16) */

  /* COMM Register (R/W) */
  ADBMS_REG_COMM, /**< Communication Register */

  /* LPCM Registers (R/W) */
  ADBMS_REG_CMCFG,   /**< LPCM Configuration */
  ADBMS_REG_CMCELLT, /**< LPCM Cell Thresholds */
  ADBMS_REG_CMGPIOT, /**< LPCM GPIO Thresholds */
  ADBMS_REG_CMFLAG,  /**< LPCM Flags (R/O) */

  /* Serial ID (R/O) */
  ADBMS_REG_SID, /**< Serial ID */

  /* Retention Register (R/W after unlock) */
  ADBMS_REG_RR, /**< Retention Register */

  ADBMS_REG_COUNT /**< Number of register groups */
} ADBMS_RegGroup_t;

/*============================================================================
 * Memory Structure - Complete ADBMS6830B Memory Mirror
 *
 * Each register group provides both raw byte access and parsed field access.
 * Total size: ~342 bytes per IC
 *============================================================================*/

/** Register union type for raw + parsed access */
#define ADBMS_REG_UNION(type)                                                                                          \
  union                                                                                                                \
  {                                                                                                                    \
    uint8_t raw[ADBMS_REG_SIZE];                                                                                       \
    type parsed;                                                                                                       \
  }

/**
 * @brief Complete memory mirror for a single ADBMS6830B IC
 */
typedef struct __attribute__((packed))
{
  /*========================================================================
   * Configuration Registers (Read/Write) - 12 bytes
   *========================================================================*/
  ADBMS_REG_UNION(CFGARA_t) cfga; /**< Configuration A */
  ADBMS_REG_UNION(CFGBR_t) cfgb;  /**< Configuration B */

  /*========================================================================
   * Cell Voltage Registers (Read-Only) - 36 bytes
   * Contains C-ADC results for cells 1-16
   *========================================================================*/
  union
  {
    struct
    {
      ADBMS_REG_UNION(RDCVA_t) a; /**< Cells 1-3 */
      ADBMS_REG_UNION(RDCVB_t) b; /**< Cells 4-6 */
      ADBMS_REG_UNION(RDCVC_t) c; /**< Cells 7-9 */
      ADBMS_REG_UNION(RDCVD_t) d; /**< Cells 10-12 */
      ADBMS_REG_UNION(RDCVE_t) e; /**< Cells 13-15 */
      ADBMS_REG_UNION(RDCVF_t) f; /**< Cell 16 */
    } groups;
    uint8_t all_raw[36]; /**< For bulk RDCVALL command */
  } cv;

  /*========================================================================
   * Averaged Cell Voltage Registers (Read-Only) - 36 bytes
   *========================================================================*/
  union
  {
    struct
    {
      ADBMS_REG_UNION(RDACA_t) a;
      ADBMS_REG_UNION(RDACB_t) b;
      ADBMS_REG_UNION(RDACC_t) c;
      ADBMS_REG_UNION(RDACD_t) d;
      ADBMS_REG_UNION(RDACE_t) e;
      ADBMS_REG_UNION(RDACF_t) f;
    } groups;
    uint8_t all_raw[36];
  } acv;

  /*========================================================================
   * Filtered Cell Voltage Registers (Read-Only) - 36 bytes
   *========================================================================*/
  union
  {
    struct
    {
      ADBMS_REG_UNION(RDFCA_t) a;
      ADBMS_REG_UNION(RDFCB_t) b;
      ADBMS_REG_UNION(RDFCC_t) c;
      ADBMS_REG_UNION(RDFCD_t) d;
      ADBMS_REG_UNION(RDFCE_t) e;
      ADBMS_REG_UNION(RDFCF_t) f;
    } groups;
    uint8_t all_raw[36];
  } fcv;

  /*========================================================================
   * S-Voltage Registers (Read-Only) - 36 bytes
   * Redundant ADC measurements
   *========================================================================*/
  union
  {
    struct
    {
      ADBMS_REG_UNION(RDSVA_t) a;
      ADBMS_REG_UNION(RDSVB_t) b;
      ADBMS_REG_UNION(RDSVC_t) c;
      ADBMS_REG_UNION(RDSVD_t) d;
      ADBMS_REG_UNION(RDSVE_t) e;
      ADBMS_REG_UNION(RDSVF_t) f;
    } groups;
    uint8_t all_raw[36];
  } sv;

  /*========================================================================
   * Auxiliary Registers (Read-Only) - 24 bytes
   * GPIO voltage measurements
   *========================================================================*/
  union
  {
    struct
    {
      ADBMS_REG_UNION(RDAUXA_t) a; /**< GPIO 1-3 */
      ADBMS_REG_UNION(RDAUXB_t) b; /**< GPIO 4-6 */
      ADBMS_REG_UNION(RDAUXC_t) c; /**< GPIO 7-9 */
      ADBMS_REG_UNION(RDAUXD_t) d; /**< GPIO 10, VMV, VPV */
    } groups;
    uint8_t all_raw[24];
  } aux;

  /*========================================================================
   * Redundant Auxiliary Registers (Read-Only) - 24 bytes
   *========================================================================*/
  union
  {
    struct
    {
      ADBMS_REG_UNION(RDRAXA_t) a;
      ADBMS_REG_UNION(RDRAXB_t) b;
      ADBMS_REG_UNION(RDRAXC_t) c;
      ADBMS_REG_UNION(RDRAXD_t) d;
    } groups;
    uint8_t all_raw[24];
  } raux;

  /*========================================================================
   * Status Registers (Read-Only) - 30 bytes
   *========================================================================*/
  union
  {
    struct
    {
      ADBMS_REG_UNION(RDSTATA_t) a; /**< VREF2, ITMP */
      ADBMS_REG_UNION(RDSTATB_t) b; /**< VD, VA, VRES */
      ADBMS_REG_UNION(RDSTATC_t) c; /**< Faults, Counters */
      ADBMS_REG_UNION(RDSTATD_t) d; /**< UV/OV Flags */
      ADBMS_REG_UNION(RDSTATE_t) e; /**< GPI, REV */
    } groups;
    uint8_t all_raw[30];
  } stat;

  /*========================================================================
   * PWM Registers (Read/Write) - 12 bytes
   *========================================================================*/
  union
  {
    struct
    {
      ADBMS_REG_UNION(PWMA_t) a; /**< PWM 1-12 */
      ADBMS_REG_UNION(PWMB_t) b; /**< PWM 13-16 */
    } groups;
    uint8_t all_raw[12];
  } pwm;

  /*========================================================================
   * COMM Register (Read/Write) - 6 bytes
   *========================================================================*/
  ADBMS_REG_UNION(COMM_t) comm;

  /*========================================================================
   * LPCM Registers - 24 bytes
   *========================================================================*/
  ADBMS_REG_UNION(CMCFG_t) cmcfg;     /**< LPCM Configuration (R/W) */
  ADBMS_REG_UNION(CMCELLT_t) cmcellt; /**< LPCM Cell Thresholds (R/W) */
  ADBMS_REG_UNION(CMGPIOT_t) cmgpiot; /**< LPCM GPIO Thresholds (R/W) */
  ADBMS_REG_UNION(CMFLAG_t) cmflag;   /**< LPCM Flags (R/O) */

  /*========================================================================
   * Serial ID (Read-Only) - 6 bytes
   *========================================================================*/
  ADBMS_REG_UNION(RDSID_t) sid;

  /*========================================================================
   * Retention Register (Read/Write after ULRR) - 6 bytes
   *========================================================================*/
  ADBMS_REG_UNION(RRR_t) retention;

} ADBMS_Memory_t;

/*============================================================================
 * IC Status Structure
 *============================================================================*/

/**
 * @brief Status and error tracking for a single IC
 *
 * @note The ADBMS6830B response frame includes a 6-bit Command Counter (CC)
 *       that increments with each command. Tracking cc_last allows detection
 *       of missed or duplicated commands. TODO: Populate and validate CC
 *       after bench verification confirms expected IC behavior.
 */
typedef struct
{
  uint32_t pec_error_count; /**< Total PEC errors since initialization */
  uint32_t tx_count;        /**< Total transactions */
  uint8_t last_pec_status;  /**< Bitmask of which registers had PEC errors on last read */
  uint8_t cc_last;          /**< Last received Command Counter (0-63), unpopulated */
  bool comm_ok;             /**< True if last communication succeeded */
} ADBMS_ICStatus_t;

/*============================================================================
 * IC State Structure
 *============================================================================*/

/**
 * @brief Complete state for a single ADBMS6830B IC
 */
typedef struct
{
  ADBMS_Memory_t memory;   /**< Register memory mirror */
  ADBMS_ICStatus_t status; /**< Error tracking */
  uint8_t index;           /**< Position in daisy chain (0 = first) */
} ADBMS_IC_t;

/*============================================================================
 * Chain State Structure
 *============================================================================*/

/**
 * @brief Complete state for entire daisy chain
 *
 * @note Consumers that want a consistent snapshot of a register group
 *       without taking a mutex should use the seqlock helpers:
 *       - ADBMS_SeqBegin(reg) before a read
 *       - ADBMS_SeqRetry(reg, seq) after copying, retry if true
 *       Or use the higher-level ADBMS_SnapshotRegisterGroup() helper.
 *
 *       The acquisition task is the sole producer for reg_seq[] /
 *       reg_last_tick_ms[] updates (incremented odd->read->even around
 *       SPI transfers in ADBMS_ReadRegister / ADBMS_WriteRegister).
 *
 *       Pending writes are request flags set by consumer tasks
 *       (e.g. processing task staging discharge bits) and consumed by
 *       the acquisition task during its scheduler loop. Use
 *       ADBMS_RequestWrite() / ADBMS_ConsumePendingWrites() for
 *       atomic set / fetch-and-clear semantics.
 */
typedef struct
{
  ADBMS_IC_t ics[ADBMS_MAX_ICS]; /**< Array of IC instances */
  uint8_t num_ics;               /**< Number of ICs in chain */
  bool initialized;              /**< True if driver is initialized */
  uint16_t active_cell_mask;     /**< Bitmask of populated cells (bit i = cell i+1). Default 0xFFFF. */

  /* Seqlock state (chain-wide; one entry per register group) */
  volatile uint32_t reg_seq[ADBMS_REG_COUNT];           /**< Seqlock counter (even = stable, odd = writer in progress) */
  volatile uint32_t reg_last_tick_ms[ADBMS_REG_COUNT];  /**< Last successful read/write tick (ms), 0 if never */

  /* Pending register-group writes requested by non-acquisition tasks.
   * Bitmask of (1u << ADBMS_RegGroup_t). Chain-wide because ADBMS writes
   * broadcast to all ICs in one daisy-chain transaction, with per-IC data
   * already staged in g_adbms.ics[].memory by the requester. */
  volatile uint32_t pending_writes;
} ADBMS_Chain_t;

/** Global chain state - accessible from application code */
extern ADBMS_Chain_t g_adbms;

/*============================================================================
 * Platform Abstraction - Weak Functions (Override These)
 *
 * Implement these functions for your specific hardware platform.
 * Default weak implementations do nothing.
 *============================================================================*/

/**
 * @brief SPI write only
 * @param data Data to transmit
 * @param len Number of bytes
 * @return ADBMS_PLATFORM_OK on success, or error code
 */
ADBMS_PlatformStatus_t ADBMS_Platform_SPI_Write(const uint8_t *data, uint16_t len);

/**
 * @brief SPI read only
 * @param data Buffer to receive into
 * @param len Number of bytes to read
 * @return ADBMS_PLATFORM_OK on success, or error code
 */
ADBMS_PlatformStatus_t ADBMS_Platform_SPI_Read(uint8_t *data, uint16_t len);

/**
 * @brief SPI write then read (separate operations, not full duplex)
 * @param tx_data Data to transmit
 * @param tx_len Number of bytes to transmit
 * @param rx_data Buffer to receive into
 * @param rx_len Number of bytes to receive
 * @return ADBMS_PLATFORM_OK on success, or error code
 */
ADBMS_PlatformStatus_t ADBMS_Platform_SPI_WriteRead(const uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data, uint16_t rx_len);

/**
 * @brief Assert chip select (drive low)
 */
void ADBMS_Platform_CS_Low(void);

/**
 * @brief Deassert chip select (drive high)
 */
void ADBMS_Platform_CS_High(void);

/**
 * @brief Microsecond delay
 * @param us Microseconds to delay
 */
void ADBMS_Platform_DelayUs(uint32_t us);

/**
 * @brief Millisecond delay
 * @param ms Milliseconds to delay
 */
void ADBMS_Platform_DelayMs(uint32_t ms);

/**
 * @brief Get current tick count in milliseconds
 * @return Current system tick in milliseconds
 */
uint32_t ADBMS_Platform_GetTickMs(void);

/*============================================================================
 * Initialization & Control
 *============================================================================*/

/**
 * @brief Initialize the ADBMS driver
 * @param num_ics Number of ICs in daisy chain (1-ADBMS_MAX_ICS)
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_Init(uint8_t num_ics);

/**
 * @brief Wake all ICs from sleep mode
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_WakeUp(void);

/**
 * @brief Perform soft reset on all ICs
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_SoftReset(void);

/**
 * @brief Get pointer to IC memory structure
 * @param ic_index IC index (0 to num_ics-1)
 * @return Pointer to ADBMS_Memory_t, or NULL if invalid index
 */
ADBMS_Memory_t *ADBMS_GetMemory(uint8_t ic_index);

/**
 * @brief Get pointer to IC status structure
 * @param ic_index IC index (0 to num_ics-1)
 * @return Pointer to ADBMS_ICStatus_t, or NULL if invalid index
 */
ADBMS_ICStatus_t *ADBMS_GetStatus(uint8_t ic_index);

/**
 * @brief Set which of the 16 cell channels are physically populated.
 *
 * Applies uniformly to every IC in the chain. Aggregate functions
 * (GetPackVoltage, GetMinCell, GetMaxCell) will skip masked-out cells.
 *
 * @param mask Bitmask where bit i corresponds to cell i+1. Default 0xFFFF.
 */
void ADBMS_SetActiveCellMask(uint16_t mask);

/**
 * @brief Get the current active cell mask.
 * @return Current bitmask of active cells.
 */
uint16_t ADBMS_GetActiveCellMask(void);

/*============================================================================
 * Seqlock + Pending Writes (lock-free producer/consumer helpers)
 *
 * Pattern:
 *   uint32_t seq;
 *   do {
 *     seq = ADBMS_SeqBegin(reg);
 *     // copy needed bytes out of g_adbms.ics[i].memory
 *   } while (ADBMS_SeqRetry(reg, seq));
 *
 * A read is consistent iff seq is even before and after copying AND
 * the two observations are equal.
 *============================================================================*/

/**
 * @brief Begin a seqlock read for a register group.
 * @param reg Register group
 * @return Seq value to pass to ADBMS_SeqRetry()
 */
uint32_t ADBMS_SeqBegin(ADBMS_RegGroup_t reg);

/**
 * @brief Check whether a seqlock read should retry.
 * @param reg Register group
 * @param begin_seq Value returned by ADBMS_SeqBegin()
 * @return true if the caller must retry (copy was inconsistent)
 */
bool ADBMS_SeqRetry(ADBMS_RegGroup_t reg, uint32_t begin_seq);

/**
 * @brief Last successful read/write tick (ms) for a register group.
 * @param reg Register group
 * @return Tick count in ms, 0 if group has never been touched
 */
uint32_t ADBMS_GetRegisterLastTickMs(ADBMS_RegGroup_t reg);

/**
 * @brief Snapshot a register group from one IC into an output buffer.
 *
 * Uses the seqlock to guarantee the copy is consistent with respect to
 * the acquisition task. Falls back to a best-effort copy if @p max_retries
 * is exhausted (extremely unlikely under normal scheduling).
 *
 * @param reg Register group (must be a *simple* group with a data_size,
 *            not a bulk aggregate - use the individual groups such as
 *            ADBMS_REG_CVA .. ADBMS_REG_CVF for cell voltages).
 * @param ic_index IC index
 * @param dst Output buffer (at least ADBMS_REG_SIZE bytes)
 * @param size Size of @p dst buffer
 * @param max_retries Maximum retries before giving up (e.g. 8)
 * @return true if copy is guaranteed consistent, false if best-effort
 */
bool ADBMS_SnapshotRegisterGroup(ADBMS_RegGroup_t reg, uint8_t ic_index, void *dst, size_t size, uint32_t max_retries);

/**
 * @brief Snapshot entire memory mirror of one IC into an output struct.
 *
 * Walks every register group and copies it atomically w.r.t. the
 * acquisition task. Intended for diagnostic/command-line memory dumps;
 * not a hot-path primitive.
 *
 * @param ic_index IC index
 * @param out Output struct
 * @param max_retries Max retries per group
 * @return true if every group copy was consistent
 */
bool ADBMS_SnapshotMemory(uint8_t ic_index, ADBMS_Memory_t *out, uint32_t max_retries);

/**
 * @brief Request a register-group write to be performed by the acquisition task.
 *
 * Staging must have already been done in g_adbms.ics[].memory by the caller.
 * The actual SPI write is deferred until the acquisition task's scheduler
 * loop picks it up (typically within the next tick).
 *
 * @param reg Writable register group (CFGA/CFGB/PWMA/PWMB/...)
 */
void ADBMS_RequestWrite(ADBMS_RegGroup_t reg);

/**
 * @brief Atomically fetch and clear all pending-write flags.
 *
 * Called by the acquisition task. The bitmask returned tells it which
 * register groups to write this iteration. Per-IC data has already been
 * staged in g_adbms.ics[].memory by the requester.
 *
 * @return Bitmask of (1u << ADBMS_RegGroup_t) that were pending
 */
uint32_t ADBMS_ConsumePendingWrites(void);

/**
 * @brief Peek at currently pending writes without consuming them.
 */
uint32_t ADBMS_GetPendingWrites(void);

/*============================================================================
 * Register Read/Write Functions
 *============================================================================*/

/**
 * @brief Read a register group from all ICs
 * @param reg Register group to read
 * @return ADBMS_OK on success, ADBMS_ERR_PEC if any PEC error
 * @note Data is stored in g_adbms.ics[].memory
 */
ADBMS_Error_t ADBMS_ReadRegister(ADBMS_RegGroup_t reg);

/**
 * @brief Write a register group to all ICs
 * @param reg Register group to write (must be writable)
 * @return ADBMS_OK on success
 * @note Data is taken from g_adbms.ics[].memory
 */
ADBMS_Error_t ADBMS_WriteRegister(ADBMS_RegGroup_t reg);

/*============================================================================
 * ADC Control Functions
 *============================================================================*/

/**
 * @brief Start cell voltage ADC conversion (C-ADC and optionally A-ADC)
 * @param rd Redundant mode: 0=C-ADC only, 1=C-ADC + A-ADC
 * @param cont Continuous mode: 0=single shot, 1=continuous
 * @param dcp Discharge permitted: 0=no, 1=yes
 * @param rstf Reset filter: 0=no, 1=yes
 * @param ow Open-wire detection: 0=off, 1=even, 2=odd, 3=all
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_StartCellADC(uint8_t rd, uint8_t cont, uint8_t dcp, uint8_t rstf, uint8_t ow);

/**
 * @brief Start S-ADC conversion (redundant cell voltages)
 * @param cont Continuous mode
 * @param dcp Discharge permitted
 * @param ow Open-wire detection
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_StartSADC(uint8_t cont, uint8_t dcp, uint8_t ow);

/**
 * @brief Start auxiliary ADC conversion (GPIO/temperature)
 * @param ow Open-wire detection: 0=off, 1=on
 * @param pup Pull-up/down: 0=pull-down, 1=pull-up
 * @param ch Channel: 0=all, 1-10=GPIO1-10, etc.
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_StartAuxADC(uint8_t ow, uint8_t pup, uint8_t ch);

/**
 * @brief Poll for any ADC conversion complete
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return ADBMS_OK when complete, ADBMS_ERR_TIMEOUT if exceeded
 */
ADBMS_Error_t ADBMS_PollADC(uint32_t timeout_ms);

/**
 * @brief Poll for C-ADC conversion complete
 * @param timeout_ms Maximum time to wait
 * @return ADBMS_OK when complete
 */
ADBMS_Error_t ADBMS_PollCADC(uint32_t timeout_ms);

/**
 * @brief Poll for S-ADC conversion complete
 * @param timeout_ms Maximum time to wait
 * @return ADBMS_OK when complete
 */
ADBMS_Error_t ADBMS_PollSADC(uint32_t timeout_ms);

/**
 * @brief Poll for AUX ADC conversion complete
 * @param timeout_ms Maximum time to wait
 * @return ADBMS_OK when complete
 */
ADBMS_Error_t ADBMS_PollAuxADC(uint32_t timeout_ms);

/*============================================================================
 * Clear & Control Commands
 *============================================================================*/

ADBMS_Error_t ADBMS_ClearCellVoltages(void);
ADBMS_Error_t ADBMS_ClearFilteredVoltages(void);
ADBMS_Error_t ADBMS_ClearAuxVoltages(void);
ADBMS_Error_t ADBMS_ClearSVoltages(void);
ADBMS_Error_t ADBMS_ClearFlags(void);
ADBMS_Error_t ADBMS_ClearOVUV(void);

ADBMS_Error_t ADBMS_MuteDischarge(void);
ADBMS_Error_t ADBMS_UnmuteDischarge(void);
ADBMS_Error_t ADBMS_Snapshot(void);
ADBMS_Error_t ADBMS_ReleaseSnapshot(void);
ADBMS_Error_t ADBMS_ResetCommandCounter(void);

/*============================================================================
 * High-Level Convenience Functions
 *============================================================================*/

/**
 * @brief Read all cell voltages using bulk command (RDCVALL)
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_ReadAllCellVoltages(void);

/*============================================================================
 * Individual Register Read Functions
 *
 * These provide explicit access to individual register groups.
 * Useful for selective reads or when bulk commands are not desired.
 *============================================================================*/

/* Cell Voltages - Individual groups (6 bytes each) */
ADBMS_Error_t ADBMS_ReadCellVoltagesA(void); /**< CVA: cells 1-3 */
ADBMS_Error_t ADBMS_ReadCellVoltagesB(void); /**< CVB: cells 4-6 */
ADBMS_Error_t ADBMS_ReadCellVoltagesC(void); /**< CVC: cells 7-9 */
ADBMS_Error_t ADBMS_ReadCellVoltagesD(void); /**< CVD: cells 10-12 */
ADBMS_Error_t ADBMS_ReadCellVoltagesE(void); /**< CVE: cells 13-15 */
ADBMS_Error_t ADBMS_ReadCellVoltagesF(void); /**< CVF: cell 16 */

/* S-Voltages - Individual groups (redundant ADC) */
ADBMS_Error_t ADBMS_ReadSVoltagesA(void); /**< SVA: cells 1-3 */
ADBMS_Error_t ADBMS_ReadSVoltagesB(void); /**< SVB: cells 4-6 */
ADBMS_Error_t ADBMS_ReadSVoltagesC(void); /**< SVC: cells 7-9 */
ADBMS_Error_t ADBMS_ReadSVoltagesD(void); /**< SVD: cells 10-12 */
ADBMS_Error_t ADBMS_ReadSVoltagesE(void); /**< SVE: cells 13-15 */
ADBMS_Error_t ADBMS_ReadSVoltagesF(void); /**< SVF: cell 16 */

/* Auxiliary - Individual groups (GPIO voltages) */
ADBMS_Error_t ADBMS_ReadAuxA(void); /**< AUXA: GPIO 1-3 */
ADBMS_Error_t ADBMS_ReadAuxB(void); /**< AUXB: GPIO 4-6 */
ADBMS_Error_t ADBMS_ReadAuxC(void); /**< AUXC: GPIO 7-9 */
ADBMS_Error_t ADBMS_ReadAuxD(void); /**< AUXD: GPIO 10, VMV, VPV */

/* Redundant Auxiliary - Individual groups */
ADBMS_Error_t ADBMS_ReadRAuxA(void); /**< RAXA: redundant GPIO 1-3 */
ADBMS_Error_t ADBMS_ReadRAuxB(void); /**< RAXB: redundant GPIO 4-6 */
ADBMS_Error_t ADBMS_ReadRAuxC(void); /**< RAXC: redundant GPIO 7-9 */
ADBMS_Error_t ADBMS_ReadRAuxD(void); /**< RAXD: redundant GPIO 10 */

/* Status - Individual groups */
ADBMS_Error_t ADBMS_ReadStatusA(void); /**< STATA: VREF2, ITMP */
ADBMS_Error_t ADBMS_ReadStatusB(void); /**< STATB: VD, VA, VRES */
ADBMS_Error_t ADBMS_ReadStatusC(void); /**< STATC: Faults, Counters */
ADBMS_Error_t ADBMS_ReadStatusD(void); /**< STATD: UV/OV Flags */
ADBMS_Error_t ADBMS_ReadStatusE(void); /**< STATE: GPI, REV */

/* Configuration - Individual groups */
ADBMS_Error_t ADBMS_ReadConfigA(void); /**< CFGA */
ADBMS_Error_t ADBMS_ReadConfigB(void); /**< CFGB */

/* Averaged Cell Voltages - Individual groups */
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesA(void); /**< ACA: cells 1-3 */
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesB(void); /**< ACB: cells 4-6 */
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesC(void); /**< ACC: cells 7-9 */
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesD(void); /**< ACD: cells 10-12 */
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesE(void); /**< ACE: cells 13-15 */
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesF(void); /**< ACF: cell 16 */

/* Filtered Cell Voltages - Individual groups */
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesA(void); /**< FCA: cells 1-3 */
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesB(void); /**< FCB: cells 4-6 */
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesC(void); /**< FCC: cells 7-9 */
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesD(void); /**< FCD: cells 10-12 */
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesE(void); /**< FCE: cells 13-15 */
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesF(void); /**< FCF: cell 16 */

/* Serial ID */
ADBMS_Error_t ADBMS_ReadSerialID(void); /**< Read Serial ID register */

/**
 * @brief Read all averaged cell voltages using bulk command (RDACALL)
 */
ADBMS_Error_t ADBMS_ReadAllAveragedVoltages(void);

/**
 * @brief Read all S-voltages using bulk command (RDSALL)
 */
ADBMS_Error_t ADBMS_ReadAllSVoltages(void);

/**
 * @brief Read all filtered cell voltages using bulk command (RDFCALL)
 */
ADBMS_Error_t ADBMS_ReadAllFilteredVoltages(void);

/**
 * @brief Read all auxiliary registers (GPIO voltages)
 */
ADBMS_Error_t ADBMS_ReadAllAux(void);

/**
 * @brief Read all status registers
 */
ADBMS_Error_t ADBMS_ReadAllStatus(void);

/**
 * @brief Write configuration registers A and B
 */
ADBMS_Error_t ADBMS_WriteConfig(void);

/**
 * @brief Read configuration registers A and B
 */
ADBMS_Error_t ADBMS_ReadConfig(void);

/*============================================================================
 * Parsed Value Getters
 *============================================================================*/

/**
 * @brief Get cell voltage in millivolts
 * @param ic_index IC index (0 to num_ics-1)
 * @param cell_index Cell index (0-15)
 * @return Voltage in mV (typically 2000-4200), or -1 if error
 */
int32_t ADBMS_GetCellVoltage_mV(uint8_t ic_index, uint8_t cell_index);

/**
 * @brief Get cell voltage as float (volts)
 * @param ic_index IC index
 * @param cell_index Cell index (0-15)
 * @return Voltage in V
 */
float ADBMS_GetCellVoltage_V(uint8_t ic_index, uint8_t cell_index);

/**
 * @brief Get averaged cell voltage in millivolts
 */
int32_t ADBMS_GetAvgCellVoltage_mV(uint8_t ic_index, uint8_t cell_index);

/**
 * @brief Get S-voltage (redundant ADC) in millivolts
 */
int32_t ADBMS_GetSVoltage_mV(uint8_t ic_index, uint8_t cell_index);

/**
 * @brief Get GPIO voltage in millivolts
 * @param ic_index IC index
 * @param gpio_index GPIO index (0-9 for GPIO1-10)
 * @return Voltage in mV
 */
int32_t ADBMS_GetGPIOVoltage_mV(uint8_t ic_index, uint8_t gpio_index);

/**
 * @brief Get internal die temperature in deci-Celsius
 * @param ic_index IC index
 * @return Temperature in 0.1C units (e.g., 250 = 25.0C)
 */
int16_t ADBMS_GetInternalTemp_dC(uint8_t ic_index);

/**
 * @brief Get internal die temperature in Celsius (float)
 */
float ADBMS_GetInternalTemp_C(uint8_t ic_index);

/**
 * @brief Check if cell has undervoltage flag set
 * @param ic_index IC index
 * @param cell_index Cell index (0-15)
 * @return true if UV flag is set
 */
bool ADBMS_GetCellUVFlag(uint8_t ic_index, uint8_t cell_index);

/**
 * @brief Check if cell has overvoltage flag set
 */
bool ADBMS_GetCellOVFlag(uint8_t ic_index, uint8_t cell_index);

/**
 * @brief Get sum of all cell voltages for an IC in millivolts
 */
int32_t ADBMS_GetPackVoltage_mV(uint8_t ic_index);

/**
 * @brief Get minimum cell voltage across all cells for an IC
 * @param ic_index IC index
 * @param min_cell_index Output: index of minimum cell (can be NULL)
 * @return Minimum voltage in mV
 */
int32_t ADBMS_GetMinCellVoltage_mV(uint8_t ic_index, uint8_t *min_cell_index);

/**
 * @brief Get maximum cell voltage across all cells for an IC
 */
int32_t ADBMS_GetMaxCellVoltage_mV(uint8_t ic_index, uint8_t *max_cell_index);

/**
 * @brief Get filtered cell voltage in millivolts
 * @param ic_index IC index
 * @param cell_index Cell index (0-15)
 * @return Filtered voltage in mV, or -1 if error
 */
int32_t ADBMS_GetFilteredCellVoltage_mV(uint8_t ic_index, uint8_t cell_index);

/**
 * @brief Get VREF2 (reference voltage) in millivolts
 * @param ic_index IC index
 * @return VREF2 in mV, or -1 if error
 */
int32_t ADBMS_GetVREF2_mV(uint8_t ic_index);

/**
 * @brief Get VD (digital supply voltage) in millivolts
 * @param ic_index IC index
 * @return VD in mV, or -1 if error
 */
int32_t ADBMS_GetVD_mV(uint8_t ic_index);

/**
 * @brief Get VA (analog supply voltage) in millivolts
 * @param ic_index IC index
 * @return VA in mV, or -1 if error
 */
int32_t ADBMS_GetVA_mV(uint8_t ic_index);

/**
 * @brief Get silicon revision ID
 * @param ic_index IC index
 * @return Revision ID, or 0xFF if error
 */
uint8_t ADBMS_GetRevisionID(uint8_t ic_index);

/**
 * @brief Get serial ID of IC
 * @param ic_index IC index
 * @param sid_out Output buffer (6 bytes)
 */
void ADBMS_GetSerialID(uint8_t ic_index, uint8_t sid_out[6]);

/**
 * @brief Check if all ICs in chain have successful communication
 * @return true if all ICs have comm_ok set
 */
bool ADBMS_IsChainCommOk(void);

/**
 * @brief Get bitmask of ICs with communication failures
 * @return Bitmask where bit N = 1 means IC N has comm failure
 */
uint16_t ADBMS_GetFailedICMask(void);

/*============================================================================
 * Register Dump API
 *
 * Functions for dumping register contents for debugging and diagnostics.
 *============================================================================*/

/** Printf-like function pointer for output */
typedef int (*ADBMS_PrintFunc_t)(const char *fmt, ...);

/**
 * @brief Get register name string
 * @param reg Register group identifier
 * @return Register name string, or "UNKNOWN" if invalid
 */
const char *ADBMS_GetRegisterName(ADBMS_RegGroup_t reg);

/**
 * @brief Dump a single register group (raw hex)
 * @param reg Register group to dump
 * @param print_fn Printf-like function for output
 */
void ADBMS_DumpRegister(ADBMS_RegGroup_t reg, ADBMS_PrintFunc_t print_fn);

/**
 * @brief Dump all registers from all ICs
 * @param print_fn Printf-like function for output
 * @param fresh_read If true, reads all registers from ICs first.
 *                   If false, uses cached data in g_adbms.
 */
void ADBMS_DumpAllRegisters(ADBMS_PrintFunc_t print_fn, bool fresh_read);

/**
 * @brief Dump communication status for all ICs
 * @param print_fn Printf-like function for output
 */
void ADBMS_DumpCommStatus(ADBMS_PrintFunc_t print_fn);

/**
 * @brief Read all registers into cached memory
 * @return ADBMS_OK on success, error code on first failure
 * @note Reads configuration, voltages, status, and serial ID
 */
ADBMS_Error_t ADBMS_ReadAllRegistersToCache(void);

/*============================================================================
 * Configuration Setters
 *============================================================================*/

/**
 * @brief Set undervoltage threshold
 * @param ic_index IC index (or 0xFF for all ICs)
 * @param threshold_mV Threshold in millivolts
 * @return ADBMS_OK on success
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetUVThreshold(uint8_t ic_index, uint16_t threshold_mV);

/**
 * @brief Set overvoltage threshold
 * @param ic_index IC index (or 0xFF for all ICs)
 * @param threshold_mV Threshold in millivolts
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetOVThreshold(uint8_t ic_index, uint16_t threshold_mV);

/**
 * @brief Set discharge control for cells
 * @param ic_index IC index
 * @param cell_mask 16-bit mask (bit N = cell N+1)
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetDischarge(uint8_t ic_index, uint16_t cell_mask);

/**
 * @brief Set discharge timeout
 * @param ic_index IC index (or 0xFF for all)
 * @param timeout_code 6-bit timeout value (see datasheet)
 */
ADBMS_Error_t ADBMS_SetDischargeTimeout(uint8_t ic_index, uint8_t timeout_code);

/**
 * @brief Set GPIO output states
 * @param ic_index IC index
 * @param gpio_mask 10-bit mask for GPIO1-10
 */
ADBMS_Error_t ADBMS_SetGPO(uint8_t ic_index, uint16_t gpio_mask);

/**
 * @brief Enable/disable reference voltage
 * @param ic_index IC index (or 0xFF for all)
 * @param enable true to enable reference
 */
ADBMS_Error_t ADBMS_SetRefOn(uint8_t ic_index, bool enable);

/**
 * @brief Set C-ADC vs S-ADC comparison threshold (Table 102)
 * @param ic_index IC index (or 0xFF for all ICs)
 * @param cth Threshold value from ADBMS_CTH_t enum
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetCTH(uint8_t ic_index, ADBMS_CTH_t cth);

/**
 * @brief Set FLAG_D diagnostic bits for latent fault testing (Table 102)
 * @param ic_index IC index (or 0xFF for all ICs)
 * @param flag_d 8-bit flag value (use ADBMS_FLAG_D_Bits_t ORed together)
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetFlagD(uint8_t ic_index, uint8_t flag_d);

/**
 * @brief Enable/disable soak time for AUX ADCs (Table 102)
 * @param ic_index IC index (or 0xFF for all ICs)
 * @param enable true to enable soak time for all commands
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetSoakOn(uint8_t ic_index, bool enable);

/**
 * @brief Set soak time range (Table 102)
 * @param ic_index IC index (or 0xFF for all ICs)
 * @param long_range true for long soak time range, false for short
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetOwrng(uint8_t ic_index, bool long_range);

/**
 * @brief Set open wire soak time (Table 102)
 * @param ic_index IC index (or 0xFF for all ICs)
 * @param owa 3-bit open wire time value (0-7)
 *        If OWRNG=0: soak_time = 2^(6+OWA) clocks (32us to 4.1ms)
 *        If OWRNG=1: soak_time = 2^(13+OWA) clocks (4.1ms to 524ms)
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetOWA(uint8_t ic_index, uint8_t owa);

/**
 * @brief Set IIR filter parameter (Table 102)
 * @param ic_index IC index (or 0xFF for all ICs)
 * @param fc Filter coefficient from ADBMS_FC_t enum
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetFC(uint8_t ic_index, ADBMS_FC_t fc);

/**
 * @brief Enable/disable communication break (Table 102)
 * @param ic_index IC index (or 0xFF for all ICs)
 * @param enable true to prevent communication propagation through daisy chain
 * @note Does NOT write to IC - call ADBMS_WriteConfig() after
 */
ADBMS_Error_t ADBMS_SetCommBreak(uint8_t ic_index, bool enable);

/*============================================================================
 * PWM Control
 *============================================================================*/

/**
 * @brief Set PWM duty cycle for a cell
 * @param ic_index IC index
 * @param cell_index Cell index (0-15)
 * @param duty 4-bit duty cycle (0-15)
 * @note Does NOT write to IC - call ADBMS_WriteRegister(ADBMS_REG_PWMA/B) after
 */
ADBMS_Error_t ADBMS_SetPWM(uint8_t ic_index, uint8_t cell_index, uint8_t duty);

/**
 * @brief Write PWM registers to all ICs
 */
ADBMS_Error_t ADBMS_WritePWM(void);

/*============================================================================
 * Bulk Read Commands
 *============================================================================*/

/**
 * @brief Read all C and S voltage results
 * @return ADBMS_OK on success
 * @note Populates both cv and sv memory regions.
 * @warning This performs TWO separate transactions (CVALL + SVALL). It does
 *          NOT use the combined RDCSALL opcode, so the C and S readings are
 *          not an atomic snapshot.
 */
ADBMS_Error_t ADBMS_ReadAllCellAndSVoltages(void);

/**
 * @brief Read all averaged C and S voltage results
 * @return ADBMS_OK on success
 * @note Populates both acv and sv memory regions.
 * @warning This performs TWO separate transactions (ACALL + SVALL). It does
 *          NOT use the combined RDACSALL opcode, so the readings are not an
 *          atomic snapshot.
 */
ADBMS_Error_t ADBMS_ReadAllAvgCellAndSVoltages(void);

/**
 * @brief Read all auxiliary and status registers
 * @return ADBMS_OK on success
 * @note Populates aux and stat memory regions.
 * @warning This performs TWO separate transactions (AUXA..D + STATA..E). It
 *          does NOT use the combined RDASALL opcode, so the aux and status
 *          readings are not an atomic snapshot.
 */
ADBMS_Error_t ADBMS_ReadAllAuxAndStatus(void);

/*============================================================================
 * LPCM Control Commands (Table 112)
 *============================================================================*/

/**
 * @brief Disable LPCM (Low Power Cell Monitor) using CMDIS command
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_LPCMDisable(void);

/**
 * @brief Enable LPCM (Low Power Cell Monitor) using CMEN command
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_LPCMEnable(void);

/**
 * @brief Send LPCM heartbeat using CMHB command
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_LPCMHeartbeat(void);

/**
 * @brief Clear LPCM flags using CLRCMFLAG command
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_ClearLPCMFlags(void);

/**
 * @brief Read LPCM flags register
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_ReadLPCMFlags(void);

/**
 * @brief Write LPCM configuration register
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_WriteLPCMConfig(void);

/**
 * @brief Read LPCM configuration register
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_ReadLPCMConfig(void);

/**
 * @brief Write LPCM cell thresholds register
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_WriteLPCMCellThresholds(void);

/**
 * @brief Read LPCM cell thresholds register
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_ReadLPCMCellThresholds(void);

/**
 * @brief Write LPCM GPIO thresholds register
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_WriteLPCMGPIOThresholds(void);

/**
 * @brief Read LPCM GPIO thresholds register
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_ReadLPCMGPIOThresholds(void);

/*============================================================================
 * Additional ADC Commands
 *============================================================================*/

/**
 * @brief Start AUX2 ADC conversion using ADAX2 command
 * @param ch Channel selection (0=all, see adc_conv_channel_selection_t)
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_StartAux2ADC(uint8_t ch);

/**
 * @brief Poll AUX2 ADC completion using PLAUX2 command
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return ADBMS_OK when complete, ADBMS_ERR_TIMEOUT if exceeded
 */
ADBMS_Error_t ADBMS_PollAux2ADC(uint32_t timeout_ms);

/*============================================================================
 * Communication Register Commands (Table 116)
 *============================================================================*/

/**
 * @brief Write COMM register using WRCOMM command
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_WriteComm(void);

/**
 * @brief Read COMM register using RDCOMM command
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_ReadComm(void);

/**
 * @brief Start I2C/SPI communication using STCOMM command
 * @return ADBMS_OK on success
 * @note Requires prior WRCOMM to set up communication data
 */
ADBMS_Error_t ADBMS_StartComm(void);

/*============================================================================
 * Retention Register Commands
 *============================================================================*/

/**
 * @brief Unlock retention register using ULRR command
 * @return ADBMS_OK on success
 * @note Required before WRRR (write retention register)
 */
ADBMS_Error_t ADBMS_UnlockRetention(void);

/**
 * @brief Write retention register using WRRR command
 * @return ADBMS_OK on success
 * @note Must call ADBMS_UnlockRetention() first
 */
ADBMS_Error_t ADBMS_WriteRetention(void);

/**
 * @brief Read retention register using RDRR command
 * @return ADBMS_OK on success
 */
ADBMS_Error_t ADBMS_ReadRetention(void);

/*============================================================================
 * Additional Status Getters (Tables 105-110)
 *============================================================================*/

/**
 * @brief Get VRES (reference resistor voltage) in millivolts (Table 106)
 * @param ic_index IC index
 * @return VRES in mV, or -1 if error
 * @note Formula: VRES = code * 150µV + 1.5V
 */
int32_t ADBMS_GetVRES_mV(uint8_t ic_index);

/**
 * @brief Get oscillator check counter value (Table 109)
 * @param ic_index IC index
 * @return Counter value (52-71 is passing range), or 0xFF if error
 */
uint8_t ADBMS_GetOscillatorCount(uint8_t ic_index);

/**
 * @brief Get conversion counter from Status C (Table 108)
 * @param ic_index IC index
 * @return 11-bit conversion counter, or 0xFFFF if error
 */
uint16_t ADBMS_GetConversionCount(uint8_t ic_index);

/*============================================================================
 * Status C Flag Getters (Table 108)
 *============================================================================*/

/** @brief Check if thermal shutdown occurred (THSD flag) */
bool ADBMS_GetTHSDFlag(uint8_t ic_index);

/** @brief Check if device entered sleep mode (SLEEP flag) */
bool ADBMS_GetSleepFlag(uint8_t ic_index);

/** @brief Check if C-ADC vs S-ADC comparison is active (COMP flag) */
bool ADBMS_GetCompFlag(uint8_t ic_index);

/** @brief Check if SPI fault occurred (SPIFLT flag) */
bool ADBMS_GetSPIFaultFlag(uint8_t ic_index);

/** @brief Check if oscillator check failed (OSCCHK flag) */
bool ADBMS_GetOscCheckFlag(uint8_t ic_index);

/** @brief Check if test mode was activated (TMODCHK flag) */
bool ADBMS_GetTestModeFlag(uint8_t ic_index);

/** @brief Check if supply rail delta exceeded 0.5V (VDE flag) */
bool ADBMS_GetVDEFlag(uint8_t ic_index);

/** @brief Check latent supply rail delta flag (VDEL flag) */
bool ADBMS_GetVDELFlag(uint8_t ic_index);

/** @brief Check VA overvoltage flag (VA_OV flag) */
bool ADBMS_GetVA_OVFlag(uint8_t ic_index);

/** @brief Check VA undervoltage flag (VA_UV flag) */
bool ADBMS_GetVA_UVFlag(uint8_t ic_index);

/** @brief Check VD overvoltage flag (VD_OV flag) */
bool ADBMS_GetVD_OVFlag(uint8_t ic_index);

/** @brief Check VD undervoltage flag (VD_UV flag) */
bool ADBMS_GetVD_UVFlag(uint8_t ic_index);

/** @brief Check C-trim error detection flag (CED flag) */
bool ADBMS_GetCEDFlag(uint8_t ic_index);

/** @brief Check C-trim multiple error detection flag (CMED flag) */
bool ADBMS_GetCMEDFlag(uint8_t ic_index);

/** @brief Check S-trim error detection flag (SED flag) */
bool ADBMS_GetSEDFlag(uint8_t ic_index);

/** @brief Check S-trim multiple error detection flag (SMED flag) */
bool ADBMS_GetSMEDFlag(uint8_t ic_index);

/**
 * @brief Get cell short-fault bitmask (CSxFLT, 16 bits, one per cell)
 * @param ic_index IC index
 * @return Bitmask where bit N = 1 means C-ADC/S-ADC mismatch on cell N
 */
uint16_t ADBMS_GetCellShortFaultMask(uint8_t ic_index);

/*============================================================================
 * LPCM Flag Getters (Table 115)
 *============================================================================*/

/** @brief Check if LPCM is enabled (CMC_EN flag) */
bool ADBMS_GetLPCMEnabled(uint8_t ic_index);

/** @brief Check LPCM cell undervoltage flag (CMF_CUV) */
bool ADBMS_GetLPCM_CUVFlag(uint8_t ic_index);

/** @brief Check LPCM cell overvoltage flag (CMF_COV) */
bool ADBMS_GetLPCM_COVFlag(uint8_t ic_index);

/** @brief Check LPCM cell delta voltage positive flag (CMF_CDVP) */
bool ADBMS_GetLPCM_CDVPFlag(uint8_t ic_index);

/** @brief Check LPCM cell delta voltage negative flag (CMF_CDVN) */
bool ADBMS_GetLPCM_CDVNFlag(uint8_t ic_index);

/** @brief Check LPCM GPIO undervoltage flag (CMF_GUV) */
bool ADBMS_GetLPCM_GUVFlag(uint8_t ic_index);

/** @brief Check LPCM GPIO overvoltage flag (CMF_GOV) */
bool ADBMS_GetLPCM_GOVFlag(uint8_t ic_index);

/** @brief Check LPCM bridgeless watchdog flag (CMF_BTMWD) */
bool ADBMS_GetLPCM_BTMWDFlag(uint8_t ic_index);

/** @brief Check LPCM bridgeless message comparison flag (CMF_BTMCMP) */
bool ADBMS_GetLPCM_BTMCMPFlag(uint8_t ic_index);

/*============================================================================
 * Threshold Conversion Utilities (Table 107)
 *============================================================================*/

/**
 * @brief Convert VUV/VOV code to voltage in mV
 * @param code 12-bit threshold code
 * @return Voltage threshold in mV
 * @note Formula: V = code * 16 * 150µV + 1.5V = code * 2.4mV + 1500mV
 */
uint16_t ADBMS_ThresholdCodeToMV(uint16_t code);

/**
 * @brief Convert voltage in mV to VUV/VOV code
 * @param voltage_mV Voltage threshold in mV
 * @return 12-bit threshold code
 */
uint16_t ADBMS_MVToThresholdCode(uint16_t voltage_mV);

/*============================================================================
 * PEC Functions (Exposed for advanced use)
 *============================================================================*/

/**
 * @brief Calculate PEC-15 for command codes
 * @param data Data bytes
 * @param len Number of bytes
 * @return 15-bit PEC value (in bits [14:0] of returned uint16_t)
 */
uint16_t ADBMS_CalcPEC15(const uint8_t *data, uint8_t len);

/**
 * @brief Calculate PEC-10 for register data
 * @param data Data bytes
 * @param len Number of bytes
 * @return 10-bit PEC value
 */
uint16_t ADBMS_CalcPEC10(const uint8_t *data, uint8_t len);

#endif /* ADBMS6830B_REGISTERS_H */
