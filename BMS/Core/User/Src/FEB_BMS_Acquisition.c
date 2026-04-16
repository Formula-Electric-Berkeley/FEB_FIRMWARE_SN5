/**
 * @file FEB_BMS_Acquisition.c
 * @brief Scheduled ADBMS register acquisition (SPI owner).
 */

#include "FEB_BMS_Acquisition.h"
#include "ADBMS6830B_Registers.h"
#include "BMS_HW_Config.h"
#include "cmsis_os.h"
#include "feb_log.h"
#include <string.h>

#define TAG_ACQ "[BMS_ACQ]"

/*============================================================================
 * Job Table
 *============================================================================*/

typedef struct
{
  const char *name;
  uint32_t period_ms;
  uint32_t last_due_tick;
  bool enabled;
  BMS_Acq_JobStats_t stats;
  ADBMS_Error_t (*run)(void);
} Job_t;

static ADBMS_Error_t _job_cell_voltages(void);
static ADBMS_Error_t _job_aux_scan(void);
static ADBMS_Error_t _job_status(void);
static ADBMS_Error_t _job_filtered(void);
static ADBMS_Error_t _job_averaged(void);
static ADBMS_Error_t _job_serial_id(void);

static Job_t s_jobs[BMS_ACQ_JOB_COUNT] = {
    [BMS_ACQ_JOB_CELL_VOLTAGES] = {.name = "CELL_V",
                                   .period_ms = BMS_VOLTAGE_INTERVAL_MS,
                                   .enabled = true,
                                   .run = _job_cell_voltages},
    [BMS_ACQ_JOB_AUX_SCAN] = {.name = "AUX_SCAN",
                              .period_ms = BMS_TEMP_INTERVAL_MS,
                              .enabled = true,
                              .run = _job_aux_scan},
    [BMS_ACQ_JOB_STATUS] = {.name = "STATUS", .period_ms = 1000, .enabled = true, .run = _job_status},
    [BMS_ACQ_JOB_FILTERED] = {.name = "FILTERED", .period_ms = 200, .enabled = false, .run = _job_filtered},
    [BMS_ACQ_JOB_AVERAGED] = {.name = "AVERAGED", .period_ms = 1000, .enabled = false, .run = _job_averaged},
    [BMS_ACQ_JOB_SERIAL_ID] = {.name = "SID_CHECK", .period_ms = 60000, .enabled = true, .run = _job_serial_id},
};

static uint32_t s_consecutive_pec_errors = 0;

/* Runtime-configurable cell-ADC options (used by _job_cell_voltages).
 * rd=1 (redundant), dcp=1 (discharge permitted), ow=0 (no open-wire test). */
static uint8_t s_cell_rd = 1;
static uint8_t s_cell_dcp = 1;
static uint8_t s_cell_ow = 0;

void BMS_Acq_SetCellADCOptions(uint8_t rd, uint8_t dcp, uint8_t ow)
{
  s_cell_rd = rd ? 1 : 0;
  s_cell_dcp = dcp ? 1 : 0;
  s_cell_ow = (ow > 3) ? 3 : ow;
}

void BMS_Acq_GetCellADCOptions(uint8_t *rd, uint8_t *dcp, uint8_t *ow)
{
  if (rd)
    *rd = s_cell_rd;
  if (dcp)
    *dcp = s_cell_dcp;
  if (ow)
    *ow = s_cell_ow;
}

/*============================================================================
 * Public API
 *============================================================================*/

void BMS_Acq_Init(void)
{
  uint32_t now = osKernelGetTickCount();
  for (uint8_t j = 0; j < BMS_ACQ_JOB_COUNT; j++)
  {
    s_jobs[j].last_due_tick = now;
    memset(&s_jobs[j].stats, 0, sizeof(s_jobs[j].stats));
  }
  s_consecutive_pec_errors = 0;
}

void BMS_Acq_SetJobPeriod(BMS_Acq_Job_t job, uint32_t period_ms)
{
  if ((unsigned)job >= BMS_ACQ_JOB_COUNT)
    return;
  s_jobs[job].period_ms = period_ms;
}

uint32_t BMS_Acq_GetJobPeriod(BMS_Acq_Job_t job)
{
  if ((unsigned)job >= BMS_ACQ_JOB_COUNT)
    return 0;
  return s_jobs[job].period_ms;
}

void BMS_Acq_SetJobEnabled(BMS_Acq_Job_t job, bool enabled)
{
  if ((unsigned)job >= BMS_ACQ_JOB_COUNT)
    return;
  s_jobs[job].enabled = enabled;
}

bool BMS_Acq_IsJobEnabled(BMS_Acq_Job_t job)
{
  if ((unsigned)job >= BMS_ACQ_JOB_COUNT)
    return false;
  return s_jobs[job].enabled;
}

const BMS_Acq_JobStats_t *BMS_Acq_GetJobStats(BMS_Acq_Job_t job)
{
  if ((unsigned)job >= BMS_ACQ_JOB_COUNT)
    return NULL;
  return &s_jobs[job].stats;
}

const char *BMS_Acq_GetJobName(BMS_Acq_Job_t job)
{
  if ((unsigned)job >= BMS_ACQ_JOB_COUNT)
    return "?";
  return s_jobs[job].name;
}

uint32_t BMS_Acq_GetConsecutivePECErrors(void)
{
  return s_consecutive_pec_errors;
}

void BMS_Acq_ResetConsecutivePECErrors(void)
{
  s_consecutive_pec_errors = 0;
}

/*============================================================================
 * Scheduler
 *============================================================================*/

static void _track_result(BMS_Acq_Job_t job, ADBMS_Error_t err, uint32_t start_tick)
{
  s_jobs[job].stats.runs++;
  s_jobs[job].stats.last_duration_ticks = osKernelGetTickCount() - start_tick;
  s_jobs[job].stats.last_run_tick = start_tick;

  if (err != ADBMS_OK)
  {
    s_jobs[job].stats.errors++;
    if (err == ADBMS_ERR_PEC || err == ADBMS_ERR_SPI || err == ADBMS_ERR_TIMEOUT)
    {
      if (s_consecutive_pec_errors < UINT32_MAX)
        s_consecutive_pec_errors++;
    }
  }
  else
  {
    s_consecutive_pec_errors = 0;
  }
}

uint32_t BMS_Acq_DrainPendingWrites(void)
{
  uint32_t pending = ADBMS_ConsumePendingWrites();
  if (pending == 0)
    return 0;

  for (int reg = 0; reg < ADBMS_REG_COUNT; reg++)
  {
    if (!(pending & (1u << reg)))
      continue;
    ADBMS_Error_t err = ADBMS_WriteRegister((ADBMS_RegGroup_t)reg);
    if (err != ADBMS_OK)
    {
      LOG_W(TAG_ACQ, "Pending write reg=%d failed: %d", reg, err);
    }
  }
  return pending;
}

ADBMS_Error_t BMS_Acq_RunJobNow(BMS_Acq_Job_t job)
{
  if ((unsigned)job >= BMS_ACQ_JOB_COUNT)
    return ADBMS_ERR_INVALID_PARAM;
  uint32_t start = osKernelGetTickCount();
  ADBMS_Error_t err = s_jobs[job].run();
  _track_result(job, err, start);
  return err;
}

void BMS_Acq_ServiceScheduler(void)
{
  /* Writes have priority: drain before any scheduled reads so the
   * control path (e.g. discharge mask changes) has minimum latency. */
  BMS_Acq_DrainPendingWrites();

  uint32_t now = osKernelGetTickCount();
  for (uint8_t j = 0; j < BMS_ACQ_JOB_COUNT; j++)
  {
    Job_t *jp = &s_jobs[j];
    if (!jp->enabled || jp->period_ms == 0)
      continue;

    if ((now - jp->last_due_tick) >= pdMS_TO_TICKS(jp->period_ms))
    {
      jp->last_due_tick = now;
      uint32_t start = now;
      ADBMS_Error_t err = jp->run();
      _track_result((BMS_Acq_Job_t)j, err, start);

      /* Give write requests another chance between long-running jobs. */
      if (j == BMS_ACQ_JOB_CELL_VOLTAGES || j == BMS_ACQ_JOB_AUX_SCAN)
      {
        BMS_Acq_DrainPendingWrites();
      }

      /* Update 'now' for remaining jobs in this pass. */
      now = osKernelGetTickCount();
    }
  }
}

/*============================================================================
 * Jobs
 *============================================================================*/

static ADBMS_Error_t _job_cell_voltages(void)
{
  ADBMS_Error_t err = ADBMS_StartCellADC(s_cell_rd, 0, s_cell_dcp, 0, s_cell_ow);
  if (err != ADBMS_OK)
    return err;

  err = ADBMS_PollCADC(BMS_ADC_POLL_TIMEOUT_MS);
  if (err != ADBMS_OK)
    return err;

  if (s_cell_rd)
  {
    err = ADBMS_StartSADC(0, s_cell_dcp, s_cell_ow);
    if (err != ADBMS_OK)
      return err;

    err = ADBMS_PollSADC(BMS_ADC_POLL_TIMEOUT_MS);
    if (err != ADBMS_OK)
      return err;
  }

  err = ADBMS_ReadAllCellVoltages();
  if (err != ADBMS_OK)
    return err;

  if (s_cell_rd)
  {
    (void)ADBMS_ReadAllSVoltages();
  }

  (void)ADBMS_ReadStatusD();
  return ADBMS_OK;
}

/* MUX-select helper: updates GPO bits without touching other CFGA fields.
 * Staged in memory only; the WRCFGA write is done inside _job_aux_scan. */
static void _stage_mux_channel(uint8_t ic_index, uint8_t channel)
{
  ADBMS_Memory_t *mem = ADBMS_GetMemory(ic_index);
  if (mem == NULL)
    return;

  CFGARA_t cfg;
  CFGARA_DECODE(mem->cfga.raw, &cfg);

  uint16_t gpo = cfg.GPO;
  gpo &= ~BMS_MUX_SEL_MASK;
  if (channel & 0x01)
    gpo |= (1u << BMS_MUX_SEL1_BIT);
  if (channel & 0x02)
    gpo |= (1u << BMS_MUX_SEL2_BIT);
  if (channel & 0x04)
    gpo |= (1u << BMS_MUX_SEL3_BIT);

  ADBMS_SetGPO(ic_index, gpo);
}

static ADBMS_Error_t _job_aux_scan(void)
{
  ADBMS_Error_t err = ADBMS_OK;

  for (uint8_t mux_ch = 0; mux_ch < BMS_TEMP_SENSORS_PER_MUX; mux_ch++)
  {
    for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
    {
      _stage_mux_channel(ic, mux_ch);
    }

    err = ADBMS_WriteRegister(ADBMS_REG_CFGA);
    if (err != ADBMS_OK)
    {
      LOG_W(TAG_ACQ, "MUX CFGA write ch=%d err=%d", mux_ch, err);
      continue;
    }

    osDelay(pdMS_TO_TICKS(BMS_MUX_SETTLE_MS));

    err = ADBMS_StartAuxADC(0, 0, 0);
    if (err != ADBMS_OK)
      continue;

    err = ADBMS_PollAuxADC(BMS_ADC_POLL_TIMEOUT_MS);
    if (err != ADBMS_OK)
      continue;

    err = ADBMS_ReadAllAux();
    /* Per-channel failures are tolerated; processing task uses freshness
     * + in-range checks to ignore stale sensors. */
  }

  /* Internal IC die temps come from STATA. */
  (void)ADBMS_ReadStatusA();

  return ADBMS_OK;
}

static ADBMS_Error_t _job_status(void)
{
  ADBMS_Error_t rc = ADBMS_OK;
  ADBMS_Error_t e;

  e = ADBMS_ReadStatusA();
  if (e != ADBMS_OK)
    rc = e;
  e = ADBMS_ReadStatusB();
  if (e != ADBMS_OK)
    rc = e;
  e = ADBMS_ReadStatusC();
  if (e != ADBMS_OK)
    rc = e;
  e = ADBMS_ReadStatusD();
  if (e != ADBMS_OK)
    rc = e;
  e = ADBMS_ReadStatusE();
  if (e != ADBMS_OK)
    rc = e;

  return rc;
}

static ADBMS_Error_t _job_filtered(void)
{
  return ADBMS_ReadAllFilteredVoltages();
}

static ADBMS_Error_t _job_averaged(void)
{
  return ADBMS_ReadAllAveragedVoltages();
}

static ADBMS_Error_t _job_serial_id(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_SID);
}
