/**
 * @file FEB_BMS_Processing.h
 * @brief BMS data processing: raw register mirror -> engineering units.
 * @author Formula Electric @ Berkeley
 *
 * The processing module is the sole writer of g_bms_pack. Each frame it
 * snapshots raw data from g_adbms (via ADBMS_SeqBegin/Retry), converts
 * to engineering units, runs fault detection, and stages any control
 * writes (balancing discharge masks, config updates) back into g_adbms
 * with a pending-write flag for the acquisition task to transmit.
 *
 * This module does no SPI I/O directly.
 */

#ifndef FEB_BMS_PROCESSING_H
#define FEB_BMS_PROCESSING_H

#include "ADBMS6830B_Registers.h"
#include "FEB_ADBMS_App.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Initialise processing module state.
   */
  void BMS_Proc_Init(void);

  /**
   * @brief Run one processing frame.
   *
   * Called from the BMS processing task. Non-blocking.
   */
  void BMS_Proc_RunFrame(void);

  /*============================================================================
   * Pack seqlock helpers (consumers use these for lock-free reads)
   *============================================================================*/

  uint32_t BMS_Pack_SeqBegin(void);
  bool BMS_Pack_SeqRetry(uint32_t begin_seq);

  /**
   * @brief Copy a consistent snapshot of the pack data.
   *
   * @param out Destination
   * @param max_retries Bound the seqlock retry loop
   * @return true if consistent copy obtained, false if best-effort
   */
  bool BMS_Pack_Snapshot(BMS_PackData_t *out, uint32_t max_retries);

  /*============================================================================
   * Control-path staging (stages writes for the acquisition task to drain)
   *============================================================================*/

  /**
   * @brief Stage a balancing discharge mask for an IC and request CFGB write.
   *
   * @param ic_index IC
   * @param cell_mask 16-bit discharge bitmask
   */
  void BMS_Proc_RequestDischarge(uint8_t ic_index, uint16_t cell_mask);

  /**
   * @brief Stop all cell discharge and request write.
   */
  void BMS_Proc_RequestStopBalancing(void);

  /**
   * @brief Enable or disable balancing output entirely.
   *
   * @param enabled When false, BMS_Proc_RunFrame() never stages discharge writes.
   */
  void BMS_Proc_SetBalancingEnabled(bool enabled);

  bool BMS_Proc_IsBalancingEnabled(void);

  /*============================================================================
   * Hardware configuration (exposed via CLI)
   *
   * All setters stage a CFGA (or CFGB) write and flag the register group as
   * pending; the acquisition task drains the write on its next scheduler pass.
   *============================================================================*/

  /**
   * @brief Set the IIR filter coefficient for all ICs (CFGA.FC).
   *
   * Lower fc = wider bandwidth; higher fc = heavier filtering.
   */
  void BMS_Proc_SetIIRFilterCoeff(ADBMS_FC_t fc);

  /**
   * @brief Set C-ADC / S-ADC comparison threshold (CFGA.CTH).
   */
  void BMS_Proc_SetCSThreshold(ADBMS_CTH_t cth);

  /**
   * @brief Set AUX open-wire soak-time range (CFGA.OWRNG).
   */
  void BMS_Proc_SetOpenWireRange(bool long_range);

  /**
   * @brief Set AUX open-wire soak-time value (CFGA.OWA, 0..7).
   */
  void BMS_Proc_SetOpenWireTime(uint8_t owa);

  /**
   * @brief Enable or disable AUX soak-on (CFGA.SOAKON).
   */
  void BMS_Proc_SetSoakOn(bool on);

  /**
   * @brief Configure cell-ADC redundancy mode (C-ADC RD flag used by
   *        acquisition job). Also toggles whether FILTERED job is enabled.
   *
   * @param rd Non-zero to run C-ADC in redundant mode (runs S-ADC on same pin).
   */
  void BMS_Proc_SetRedundancyMode(uint8_t rd);

  /**
   * @brief Configure open-wire test on C-ADC (OW field, 0..3).
   */
  void BMS_Proc_SetOpenWireMode(uint8_t ow);

  /* Read-back of the staged settings */
  ADBMS_FC_t BMS_Proc_GetIIRFilterCoeff(void);
  ADBMS_CTH_t BMS_Proc_GetCSThreshold(void);
  bool BMS_Proc_GetOpenWireRange(void);
  uint8_t BMS_Proc_GetOpenWireTime(void);
  bool BMS_Proc_GetSoakOn(void);
  uint8_t BMS_Proc_GetRedundancyMode(void);
  uint8_t BMS_Proc_GetOpenWireMode(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_BMS_PROCESSING_H */
