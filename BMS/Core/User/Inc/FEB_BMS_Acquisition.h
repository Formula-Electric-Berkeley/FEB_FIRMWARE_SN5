/**
 * @file FEB_BMS_Acquisition.h
 * @brief Low-level ADBMS register acquisition (SPI I/O only).
 * @author Formula Electric @ Berkeley
 *
 * This module owns the SPI bus to the ADBMS6830B chain and is the sole
 * writer of the raw register mirror (g_adbms). It runs a table-driven
 * scheduler that issues register-group reads at strict cadences and
 * drains any pending writes staged by other tasks (e.g. the processing
 * task staging discharge masks).
 *
 * Design constraints:
 *   - No float arithmetic here. No g_bms_pack access.
 *   - All reads update the seqlock on their register group.
 *   - Consumers of the raw data should use ADBMS_SeqBegin/Retry for a
 *     lock-free consistent snapshot (see ADBMS6830B_Registers.h).
 */

#ifndef FEB_BMS_ACQUISITION_H
#define FEB_BMS_ACQUISITION_H

#include "ADBMS6830B_Registers.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Identifiers for scheduled acquisition jobs.
   *
   * Each job encapsulates a start+poll+read sequence for one logical slice
   * of the register file. The scheduler runs each job at its configured
   * cadence.
   */
  typedef enum
  {
    BMS_ACQ_JOB_CELL_VOLTAGES = 0, /**< C-ADC + S-ADC + RDCVALL + RDSALL */
    BMS_ACQ_JOB_AUX_SCAN,          /**< 7-channel MUX scan -> RDAUXA..D */
    BMS_ACQ_JOB_STATUS,            /**< STATA/STATB/STATC/STATD/STATE */
    BMS_ACQ_JOB_FILTERED,          /**< RDFCALL (hardware-IIR results) */
    BMS_ACQ_JOB_AVERAGED,          /**< RDACALL (running average) */
    BMS_ACQ_JOB_SERIAL_ID,         /**< RDSID (sanity heartbeat) */
    BMS_ACQ_JOB_COUNT
  } BMS_Acq_Job_t;

  /**
   * @brief Statistics per acquisition job.
   */
  typedef struct
  {
    uint32_t runs;                /**< Total invocations */
    uint32_t errors;              /**< Runs that returned non-OK */
    uint32_t last_duration_ticks; /**< Duration of last run (osKernel ticks) */
    uint32_t last_run_tick;       /**< Tick when last run started */
  } BMS_Acq_JobStats_t;

  /**
   * @brief Initialise the acquisition module.
   *
   * Zeros scheduler state. Does NOT touch the ADBMS chain -
   * BMS_App_Init() still handles driver init and configuration.
   */
  void BMS_Acq_Init(void);

  /**
   * @brief Configure the scheduling period for a job at runtime.
   *
   * @param job Job identifier
   * @param period_ms Period in ms; 0 disables the job
   */
  void BMS_Acq_SetJobPeriod(BMS_Acq_Job_t job, uint32_t period_ms);

  /**
   * @brief Get the current scheduling period for a job.
   */
  uint32_t BMS_Acq_GetJobPeriod(BMS_Acq_Job_t job);

  /**
   * @brief Enable or disable a job (preserves its period).
   */
  void BMS_Acq_SetJobEnabled(BMS_Acq_Job_t job, bool enabled);

  bool BMS_Acq_IsJobEnabled(BMS_Acq_Job_t job);

  /**
   * @brief Read stats for a job.
   */
  const BMS_Acq_JobStats_t *BMS_Acq_GetJobStats(BMS_Acq_Job_t job);

  /**
   * @brief Human-readable job name.
   */
  const char *BMS_Acq_GetJobName(BMS_Acq_Job_t job);

  /**
   * @brief Execute a single scheduler pass.
   *
   * Called from StartADBMSTask. Runs every due job and drains pending
   * writes from the write-request bitmask. Non-blocking except for the
   * SPI transactions themselves.
   */
  void BMS_Acq_ServiceScheduler(void);

  /**
   * @brief Run one specific job immediately (bypassing the schedule).
   *
   * Used for the CLI "force read" and during initial boot.
   * @return OK or first error encountered.
   */
  ADBMS_Error_t BMS_Acq_RunJobNow(BMS_Acq_Job_t job);

  /**
   * @brief Drain pending register-group writes immediately.
   *
   * Called by the acq task between scheduled jobs. Exposed for tests/CLI.
   * @return Bitmask of register groups written (for logging/diagnostics).
   */
  uint32_t BMS_Acq_DrainPendingWrites(void);

  /**
   * @brief Configure flags passed to ADCV when starting the cell ADC.
   *
   * @param rd Redundancy bit (0 = skip, 1 = run S-ADC in parallel)
   * @param dcp Discharge-permitted bit
   * @param ow Open-wire test mode (0..3)
   */
  void BMS_Acq_SetCellADCOptions(uint8_t rd, uint8_t dcp, uint8_t ow);

  void BMS_Acq_GetCellADCOptions(uint8_t *rd, uint8_t *dcp, uint8_t *ow);

  /**
   * @brief Get count of consecutive PEC errors observed by acq task.
   */
  uint32_t BMS_Acq_GetConsecutivePECErrors(void);

  /**
   * @brief Reset consecutive PEC counter (e.g. after user acknowledgement).
   */
  void BMS_Acq_ResetConsecutivePECErrors(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_BMS_ACQUISITION_H */
