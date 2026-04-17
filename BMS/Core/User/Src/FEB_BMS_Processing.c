/**
 * @file FEB_BMS_Processing.c
 * @brief BMS data interpretation & fault detection.
 *
 * Sole writer of g_bms_pack. Reads raw register data from g_adbms using
 * the ADBMS seqlock. Stages control writes (balancing, config) back into
 * g_adbms and flags them pending for the acquisition task to transmit.
 */

#include "FEB_BMS_Processing.h"
#include "FEB_BMS_Acquisition.h"
#include "ADBMS6830B_Registers.h"
#include "BMS_HW_Config.h"
#include "FEB_ADBMS_App.h"
#include "cmsis_os.h"
#include "feb_log.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define TAG_PROC "[BMS_PROC]"

extern BMS_PackData_t g_bms_pack;

/*============================================================================
 * Configuration cache (staged + exposed to CLI)
 *============================================================================*/

typedef struct
{
  ADBMS_FC_t fc;
  ADBMS_CTH_t cth;
  bool owrng;
  uint8_t owa;
  bool soakon;

  /* Runtime flags for the cell-ADC command - consumed by BMS_Acquisition */
  uint8_t rd; /* 0 or 1 - redundancy */
  uint8_t ow; /* 0..3 - open-wire test mode */

  bool balancing_enabled;
} ProcConfig_t;

static ProcConfig_t s_cfg = {
    .fc = ADBMS_FC_0,
    .cth = ADBMS_CTH_8_1_MV,
    .owrng = false,
    .owa = 0,
    .soakon = false,
    .rd = 1,
    .ow = 0,
    .balancing_enabled = true,
};

/*============================================================================
 * Pending requests from non-processing tasks (CLI / state machine)
 *
 * Drained inside BMS_Proc_RunFrame under the _pack_begin_write/_pack_end_write
 * bracket so callers see consistent pack snapshots.
 *============================================================================*/

static volatile bool s_req_clear_error = false;
static volatile bool s_req_mode_pending = false;
static volatile BMS_OpMode_t s_req_mode = BMS_MODE_NORMAL;

/*============================================================================
 * Error helpers (operate directly on g_bms_pack; only called from proc task)
 *============================================================================*/

static void _raise_error(BMS_ErrorSource_t src, BMS_AppError_t err, uint8_t bank, uint8_t ic, uint8_t cell)
{
  switch (src)
  {
  case BMS_ERR_SRC_VOLTAGE:
    g_bms_pack.voltage_error = err;
    break;
  case BMS_ERR_SRC_TEMP:
    g_bms_pack.temp_error = err;
    g_bms_pack.temp_error_bank = bank;
    g_bms_pack.temp_error_sensor = cell;
    break;
  case BMS_ERR_SRC_COMM:
    g_bms_pack.comm_error = err;
    break;
  default:
    break;
  }
  g_bms_pack.active_error_mask |= (uint32_t)src;
  g_bms_pack.last_error = err;
  g_bms_pack.error_bank = bank;
  g_bms_pack.error_ic = ic;
  g_bms_pack.error_cell = cell;
}

static void _clear_error(BMS_ErrorSource_t src)
{
  switch (src)
  {
  case BMS_ERR_SRC_VOLTAGE:
    g_bms_pack.voltage_error = BMS_APP_OK;
    break;
  case BMS_ERR_SRC_TEMP:
    g_bms_pack.temp_error = BMS_APP_OK;
    break;
  case BMS_ERR_SRC_COMM:
    g_bms_pack.comm_error = BMS_APP_OK;
    break;
  default:
    break;
  }
  g_bms_pack.active_error_mask &= ~(uint32_t)src;
  if (g_bms_pack.active_error_mask == 0)
  {
    g_bms_pack.last_error = BMS_APP_OK;
  }
}

static void _stage_mode_thresholds(BMS_OpMode_t mode)
{
  uint16_t uv_mv, ov_mv;
  switch (mode)
  {
  case BMS_MODE_CHARGING:
    uv_mv = BMS_CELL_UV_CHARGING_MV;
    ov_mv = BMS_CELL_OV_CHARGING_MV;
    break;
  case BMS_MODE_BALANCING:
    uv_mv = BMS_CELL_UV_BALANCING_MV;
    ov_mv = BMS_CELL_OV_BALANCING_MV;
    break;
  case BMS_MODE_NORMAL:
  default:
    uv_mv = BMS_CELL_UV_NORMAL_MV;
    ov_mv = BMS_CELL_OV_NORMAL_MV;
    break;
  }

  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    ADBMS_SetUVThreshold(ic, uv_mv);
    ADBMS_SetOVThreshold(ic, ov_mv);
  }
  ADBMS_RequestWrite(ADBMS_REG_CFGB);
  LOG_I(TAG_PROC, "Mode %d staged: UV=%dmV OV=%dmV", mode, uv_mv, ov_mv);
}

static void _drain_pending_requests(void)
{
  if (s_req_clear_error)
  {
    g_bms_pack.active_error_mask = 0;
    g_bms_pack.voltage_error = BMS_APP_OK;
    g_bms_pack.temp_error = BMS_APP_OK;
    g_bms_pack.comm_error = BMS_APP_OK;
    g_bms_pack.last_error = BMS_APP_OK;
    g_bms_pack.consecutive_pec_errors = 0;
    s_req_clear_error = false;
  }

  if (s_req_mode_pending)
  {
    BMS_OpMode_t mode = s_req_mode;
    g_bms_pack.mode = mode;
    s_req_mode_pending = false;
    _stage_mode_thresholds(mode);
  }
}

static void _get_thresholds_for_mode(uint16_t *uv_mv, uint16_t *ov_mv)
{
  switch (g_bms_pack.mode)
  {
  case BMS_MODE_CHARGING:
    *uv_mv = BMS_CELL_UV_CHARGING_MV;
    *ov_mv = BMS_CELL_OV_CHARGING_MV;
    break;
  case BMS_MODE_BALANCING:
    *uv_mv = BMS_CELL_UV_BALANCING_MV;
    *ov_mv = BMS_CELL_OV_BALANCING_MV;
    break;
  case BMS_MODE_NORMAL:
  default:
    *uv_mv = BMS_CELL_UV_NORMAL_MV;
    *ov_mv = BMS_CELL_OV_NORMAL_MV;
    break;
  }
}

static float _voltage_to_temperature(int32_t voltage_mv)
{
  float V = (float)voltage_mv;
  if (V < THERM_MIN_VOLTAGE_MV)
    return -999.0f;
  if (V > THERM_MAX_VOLTAGE_MV)
    return 999.0f;

  float R_th = V * THERM_R_PULLUP_OHMS / (THERM_VS_MV - V);
  float ln_ratio = logf(R_th / THERM_R_REF_OHMS);
  float inv_T = THERM_INV_T_REF + (THERM_INV_BETA * ln_ratio);
  float T_kelvin = 1.0f / inv_T;
  return T_kelvin - THERM_KELVIN_OFFSET;
}

/*============================================================================
 * Pack seqlock helpers
 *============================================================================*/

static inline void _pack_begin_write(void)
{
  g_bms_pack.snapshot_seq++;
  __asm volatile("dmb" ::: "memory");
}

static inline void _pack_end_write(void)
{
  __asm volatile("dmb" ::: "memory");
  g_bms_pack.snapshot_seq++;
  g_bms_pack.last_update_tick_ms = osKernelGetTickCount();
}

uint32_t BMS_Pack_SeqBegin(void)
{
  uint32_t s;
  do
  {
    s = g_bms_pack.snapshot_seq;
  } while (s & 1u);
  __asm volatile("dmb" ::: "memory");
  return s;
}

bool BMS_Pack_SeqRetry(uint32_t begin_seq)
{
  __asm volatile("dmb" ::: "memory");
  return (g_bms_pack.snapshot_seq != begin_seq);
}

bool BMS_Pack_Snapshot(BMS_PackData_t *out, uint32_t max_retries)
{
  if (out == NULL)
    return false;
  for (uint32_t attempt = 0; attempt < max_retries; attempt++)
  {
    uint32_t s = BMS_Pack_SeqBegin();
    memcpy(out, &g_bms_pack, sizeof(*out));
    if (!BMS_Pack_SeqRetry(s))
      return true;
  }
  memcpy(out, &g_bms_pack, sizeof(*out));
  return false;
}

/*============================================================================
 * Voltage processing
 *============================================================================*/

static void _process_voltages(bool voltage_data_fresh)
{
  if (!voltage_data_fresh)
    return;

  _clear_error(BMS_ERR_SRC_VOLTAGE);

  uint16_t uv_threshold_mv, ov_threshold_mv;
  _get_thresholds_for_mode(&uv_threshold_mv, &ov_threshold_mv);

  float pack_total = 0.0f;
  float pack_min = FLT_MAX;
  float pack_max = -FLT_MAX;

  for (uint8_t bank = 0; bank < BMS_NUM_BANKS; bank++)
  {
    float bank_total = 0.0f;
    float bank_min = FLT_MAX;
    float bank_max = -FLT_MAX;

    for (uint8_t ic_idx = 0; ic_idx < BMS_ICS_PER_BANK; ic_idx++)
    {
      uint8_t global_ic = bank * BMS_ICS_PER_BANK + ic_idx;

      /* Pull PEC/comm info straight off the driver (it's a snapshot counter). */
      ADBMS_ICStatus_t *status = ADBMS_GetStatus(global_ic);
      if (status != NULL)
      {
        g_bms_pack.banks[bank].ics[ic_idx].comm_ok = status->comm_ok;
        g_bms_pack.banks[bank].ics[ic_idx].pec_errors = status->pec_error_count;
      }

      for (uint8_t cell = 0; cell < BMS_CELLS_PER_IC; cell++)
      {
        /* Raw mV readings sourced from g_adbms. The seqlock in
         * ADBMS_ReadRegister ensures the underlying bytes are consistent
         * as long as ADBMS_GetCellVoltage_mV performs a simple copy -
         * which it does (single 16-bit load). */
        int32_t v_c_mv = ADBMS_GetCellVoltage_mV(global_ic, cell);
        int32_t v_s_mv = ADBMS_GetSVoltage_mV(global_ic, cell);

        BMS_CellData_t *c = &g_bms_pack.banks[bank].ics[ic_idx].cells[cell];
        c->voltage_C_V = (float)v_c_mv / 1000.0f;
        c->voltage_S_V = (float)v_s_mv / 1000.0f;

        int32_t delta = (v_c_mv > v_s_mv) ? (v_c_mv - v_s_mv) : (v_s_mv - v_c_mv);
        if (delta > BMS_C_S_VOLTAGE_TOLERANCE_MV)
        {
          LOG_W(TAG_PROC, "C/S mismatch B%d IC%d C%d: C=%dmV S=%dmV", bank, ic_idx, cell, (int)v_c_mv, (int)v_s_mv);
        }

        if (v_c_mv < (int32_t)uv_threshold_mv)
        {
          if (c->uv_count < 0xFF)
            c->uv_count++;
          if (c->uv_count >= BMS_VOLTAGE_ERROR_THRESHOLD)
          {
            _raise_error(BMS_ERR_SRC_VOLTAGE, BMS_APP_ERR_VOLTAGE_UV, bank, ic_idx, cell);
            LOG_E(TAG_PROC, "UV fault: B%d IC%d C%d = %dmV < %dmV", bank, ic_idx, cell, (int)v_c_mv, uv_threshold_mv);
          }
        }
        else
        {
          c->uv_count = 0;
        }

        if (v_c_mv > (int32_t)ov_threshold_mv)
        {
          if (c->ov_count < 0xFF)
            c->ov_count++;
          if (c->ov_count >= BMS_VOLTAGE_ERROR_THRESHOLD)
          {
            _raise_error(BMS_ERR_SRC_VOLTAGE, BMS_APP_ERR_VOLTAGE_OV, bank, ic_idx, cell);
            LOG_E(TAG_PROC, "OV fault: B%d IC%d C%d = %dmV > %dmV", bank, ic_idx, cell, (int)v_c_mv, ov_threshold_mv);
          }
        }
        else
        {
          c->ov_count = 0;
        }

        bank_total += c->voltage_C_V;
        if (c->voltage_C_V < bank_min)
          bank_min = c->voltage_C_V;
        if (c->voltage_C_V > bank_max)
          bank_max = c->voltage_C_V;
      }
    }

    g_bms_pack.banks[bank].total_voltage_V = bank_total;
    g_bms_pack.banks[bank].min_voltage_V = bank_min;
    g_bms_pack.banks[bank].max_voltage_V = bank_max;
    g_bms_pack.banks[bank].voltage_valid = 1;

    pack_total += bank_total;
    if (bank_min < pack_min)
      pack_min = bank_min;
    if (bank_max > pack_max)
      pack_max = bank_max;
  }

  g_bms_pack.pack_voltage_V = pack_total;
  g_bms_pack.pack_min_cell_V = pack_min;
  g_bms_pack.pack_max_cell_V = pack_max;
  g_bms_pack.voltage_read_count++;
  g_bms_pack.voltage_valid = true;
}

/*============================================================================
 * Temperature processing
 *============================================================================*/

static void _process_temperatures(bool temp_data_fresh)
{
  if (!temp_data_fresh)
    return;

  _clear_error(BMS_ERR_SRC_TEMP);

  /* Note: raw AUX voltages represent the CURRENT mux channel (the last
   * channel scanned by the acquisition task). Because the acq AUX_SCAN
   * job walks 0..N-1 sequentially within one pass, we rely on it having
   * stored results per MUX channel into cache between channels. Since
   * our cache only keeps the most recent AUX readings (no per-channel
   * slots), we infer the channel by tracking it internally.
   *
   * For now, the temperature processing is driven off of whatever is in
   * the AUX registers - meaning we get the last-scanned channel's values.
   * The acq task cycles channels; a processing pass may update only one
   * mux_ch per frame. Callers treat pack_min/max_temp_C as converging.
   *
   * TODO(decouple): persist per-channel AUX readings in g_adbms directly
   * so processing can always render all 7 channels. For SN5 hardware
   * (single mux array, 7 ch) this is acceptable transient behaviour.
   */

  const float temp_sensor_min_C = (float)BMS_TEMP_SENSOR_MIN_DC / 10.0f;
  const float temp_sensor_max_C = (float)BMS_TEMP_SENSOR_MAX_DC / 10.0f;

  /* Decode the MUX channel currently selected (via CFGA.GPO bits). */
  uint8_t mux_ch = 0;
  {
    ADBMS_Memory_t *mem = ADBMS_GetMemory(0);
    if (mem != NULL)
    {
      CFGARA_t cfg;
      CFGARA_DECODE(mem->cfga.raw, &cfg);
      uint16_t gpo = cfg.GPO;
      if (gpo & (1u << BMS_MUX_SEL1_BIT))
        mux_ch |= 0x01;
      if (gpo & (1u << BMS_MUX_SEL2_BIT))
        mux_ch |= 0x02;
      if (gpo & (1u << BMS_MUX_SEL3_BIT))
        mux_ch |= 0x04;
    }
  }
  if (mux_ch >= BMS_TEMP_SENSORS_PER_MUX)
    return;

  for (uint8_t mux = 0; mux < BMS_TEMP_NUM_MUXES; mux++)
  {
    for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
    {
      int32_t gpio_mv = ADBMS_GetGPIOVoltage_mV(ic, mux);
      float temp_C = _voltage_to_temperature(gpio_mv);

      uint8_t bank = ic / BMS_ICS_PER_BANK;
      uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;
      uint8_t sensors_per_ic = BMS_TEMP_NUM_MUXES * BMS_TEMP_SENSORS_PER_MUX;
      uint8_t sensor_idx = (ic_in_bank * sensors_per_ic) + (mux * BMS_TEMP_SENSORS_PER_MUX) + mux_ch;

      if (temp_C >= temp_sensor_min_C && temp_C <= temp_sensor_max_C)
      {
        g_bms_pack.banks[bank].temp_sensors_C[sensor_idx] = temp_C;
        g_bms_pack.banks[bank].temp_violations[sensor_idx] = 0;
      }
      else
      {
        if (g_bms_pack.banks[bank].temp_violations[sensor_idx] < 0xFF)
          g_bms_pack.banks[bank].temp_violations[sensor_idx]++;
        if (g_bms_pack.banks[bank].temp_violations[sensor_idx] >= BMS_TEMP_ERROR_THRESHOLD)
        {
          _raise_error(BMS_ERR_SRC_TEMP, BMS_APP_ERR_SENSOR, bank, ic_in_bank, sensor_idx);
          LOG_W(TAG_PROC, "Temp sensor fault: Bank %d IC %d Sensor %d", bank, ic_in_bank, sensor_idx);
        }
      }
    }
  }

  /* Re-aggregate pack/bank stats across the full sensor table. */
  float pack_min_temp = FLT_MAX;
  float pack_max_temp = -FLT_MAX;
  float pack_sum = 0.0f;
  uint16_t pack_count = 0;
  uint8_t pk_min_bank = 0, pk_min_sensor = 0;
  uint8_t pk_max_bank = 0, pk_max_sensor = 0;

  for (uint8_t b = 0; b < BMS_NUM_BANKS; b++)
  {
    float bmin = FLT_MAX, bmax = -FLT_MAX, bsum = 0.0f;
    uint16_t bcount = 0;
    for (uint8_t s = 0; s < BMS_TEMP_TOTAL_SENSORS; s++)
    {
      float t = g_bms_pack.banks[b].temp_sensors_C[s];
      if (t < temp_sensor_min_C || t > temp_sensor_max_C)
        continue;
      if (t < bmin)
        bmin = t;
      if (t > bmax)
        bmax = t;
      bsum += t;
      bcount++;
      if (t < pack_min_temp)
      {
        pack_min_temp = t;
        pk_min_bank = b;
        pk_min_sensor = s;
      }
      if (t > pack_max_temp)
      {
        pack_max_temp = t;
        pk_max_bank = b;
        pk_max_sensor = s;
      }
      pack_sum += t;
      pack_count++;
    }
    if (bcount > 0)
    {
      g_bms_pack.banks[b].min_temp_C = bmin;
      g_bms_pack.banks[b].max_temp_C = bmax;
      g_bms_pack.banks[b].avg_temp_C = bsum / (float)bcount;
      g_bms_pack.banks[b].temp_valid = 1;
    }
    else
    {
      g_bms_pack.banks[b].temp_valid = 0;
    }
  }

  if (pack_count > 0)
  {
    g_bms_pack.pack_min_temp_C = pack_min_temp;
    g_bms_pack.pack_max_temp_C = pack_max_temp;
    g_bms_pack.pack_avg_temp_C = pack_sum / (float)pack_count;

    float max_temp_threshold = (float)BMS_CELL_MAX_TEMP_DC / 10.0f;
    float min_temp_threshold = (float)BMS_CELL_MIN_TEMP_DC / 10.0f;

    if (pack_max_temp > max_temp_threshold)
    {
      _raise_error(BMS_ERR_SRC_TEMP, BMS_APP_ERR_TEMP_HIGH, pk_max_bank, 0xFF, pk_max_sensor);
      LOG_E(TAG_PROC, "Over-temp: %.1fC > %.1fC (B%d S%d)", pack_max_temp, max_temp_threshold, pk_max_bank,
            pk_max_sensor);
    }
    if (pack_min_temp < min_temp_threshold)
    {
      _raise_error(BMS_ERR_SRC_TEMP, BMS_APP_ERR_TEMP_LOW, pk_min_bank, 0xFF, pk_min_sensor);
      LOG_E(TAG_PROC, "Under-temp: %.1fC < %.1fC (B%d S%d)", pack_min_temp, min_temp_threshold, pk_min_bank,
            pk_min_sensor);
    }
  }

  /* Internal IC die temperatures are sourced from STATA. */
  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    float ic_temp = ADBMS_GetInternalTemp_C(ic);
    uint8_t bank = ic / BMS_ICS_PER_BANK;
    uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;
    g_bms_pack.banks[bank].ics[ic_in_bank].internal_temp_C = ic_temp;
  }

  g_bms_pack.temp_read_count++;
  g_bms_pack.temp_valid = true;
}

/*============================================================================
 * Balancing
 *============================================================================*/

static void _process_balancing(void)
{
  if (!s_cfg.balancing_enabled)
    return;
  if (g_bms_pack.mode != BMS_MODE_BALANCING)
    return;
  if (!g_bms_pack.voltage_valid)
    return;

  float balance_max_temp_C = (float)BMS_BALANCE_MAX_TEMP_DC / 10.0f;
  if (g_bms_pack.temp_valid && g_bms_pack.pack_max_temp_C > balance_max_temp_C)
  {
    LOG_W(TAG_PROC, "Temperature too high for balancing: %.1fC", g_bms_pack.pack_max_temp_C);
    BMS_Proc_RequestStopBalancing();
    return;
  }

  float min_V = g_bms_pack.pack_min_cell_V;
  float threshold_V = min_V + ((float)BMS_BALANCE_THRESHOLD_MV / 1000.0f);

  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    uint16_t discharge_mask = 0;
    uint8_t bank = ic / BMS_ICS_PER_BANK;
    uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;

    for (uint8_t cell = 0; cell < BMS_CELLS_PER_IC; cell++)
    {
      BMS_CellData_t *c = &g_bms_pack.banks[bank].ics[ic_in_bank].cells[cell];
      if (c->voltage_C_V > threshold_V)
      {
        discharge_mask |= (1u << cell);
        c->is_discharging = 1;
      }
      else
      {
        c->is_discharging = 0;
      }
    }

    ADBMS_SetDischarge(ic, discharge_mask);
  }

  /* Stage pending CFGB write (discharge bits live in CFGB) and CFGA for
   * any companion settings. We only need CFGB for discharge, but a single
   * request covers both if the acquisition task is in the middle of
   * another AUX scan (which touches CFGA). */
  ADBMS_RequestWrite(ADBMS_REG_CFGB);
}

/*============================================================================
 * Public frame runner
 *============================================================================*/

void BMS_Proc_Init(void)
{
  /* Nothing to allocate; s_cfg already has defaults. Reset the pack seq. */
  __atomic_store_n(&g_bms_pack.snapshot_seq, 0u, __ATOMIC_RELEASE);
  g_bms_pack.last_update_tick_ms = 0;
}

void BMS_Proc_RunFrame(void)
{
  if (!g_bms_pack.initialized)
    return;

  /* Check freshness: we only process what's fresh.
   * All timestamps here are in milliseconds, sourced from the same
   * ADBMS_Platform_GetTickMs() counter that stamps the register cache. */
  uint32_t now_ms = ADBMS_Platform_GetTickMs();
  uint32_t cv_tick = ADBMS_GetRegisterLastTickMs(ADBMS_REG_CVALL);
  uint32_t aux_tick = ADBMS_GetRegisterLastTickMs(ADBMS_REG_AUXA);
  uint32_t statd_tick = ADBMS_GetRegisterLastTickMs(ADBMS_REG_STATD);

  /* "Fresh" window generous enough to tolerate one missed cycle. */
  const uint32_t fresh_v_ms = BMS_VOLTAGE_INTERVAL_MS * 3u;
  const uint32_t fresh_t_ms = BMS_TEMP_INTERVAL_MS * 3u;

  bool v_fresh = (cv_tick != 0) && ((now_ms - cv_tick) <= fresh_v_ms);
  bool t_fresh = (aux_tick != 0) && ((now_ms - aux_tick) <= fresh_t_ms);

  _pack_begin_write();

  _drain_pending_requests();

  if (v_fresh)
  {
    _process_voltages(true);
  }
  else if (cv_tick == 0)
  {
    g_bms_pack.voltage_valid = false;
  }

  /* STATD carries hardware UV/OV flags - surface them as comm freshness. */
  if (statd_tick != 0)
  {
    _clear_error(BMS_ERR_SRC_COMM);
  }

  if (t_fresh)
  {
    _process_temperatures(true);
  }

  _process_balancing();

  _pack_end_write();
}

/*============================================================================
 * Control-path staging API
 *============================================================================*/

void BMS_Proc_RequestDischarge(uint8_t ic_index, uint16_t cell_mask)
{
  ADBMS_SetDischarge(ic_index, cell_mask);
  ADBMS_RequestWrite(ADBMS_REG_CFGB);
}

void BMS_Proc_RequestStopBalancing(void)
{
  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    ADBMS_SetDischarge(ic, 0x0000);

    uint8_t bank = ic / BMS_ICS_PER_BANK;
    uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;
    for (uint8_t cell = 0; cell < BMS_CELLS_PER_IC; cell++)
    {
      g_bms_pack.banks[bank].ics[ic_in_bank].cells[cell].is_discharging = 0;
    }
  }
  ADBMS_RequestWrite(ADBMS_REG_CFGB);
}

void BMS_Proc_RequestSetMode(BMS_OpMode_t mode)
{
  s_req_mode = mode;
  __asm volatile("dmb" ::: "memory");
  s_req_mode_pending = true;
}

void BMS_Proc_RequestClearError(void)
{
  s_req_clear_error = true;
}

void BMS_Proc_SetBalancingEnabled(bool enabled)
{
  s_cfg.balancing_enabled = enabled;
  if (!enabled)
    BMS_Proc_RequestStopBalancing();
}

bool BMS_Proc_IsBalancingEnabled(void)
{
  return s_cfg.balancing_enabled;
}

/*============================================================================
 * HW configuration setters (stage + request write)
 *============================================================================*/

static void _stage_cfga_all(void)
{
  ADBMS_RequestWrite(ADBMS_REG_CFGA);
}

void BMS_Proc_SetIIRFilterCoeff(ADBMS_FC_t fc)
{
  s_cfg.fc = fc;
  ADBMS_SetFC(0xFF, fc);
  _stage_cfga_all();
  LOG_I(TAG_PROC, "IIR FC staged: %d", (int)fc);
}

void BMS_Proc_SetCSThreshold(ADBMS_CTH_t cth)
{
  s_cfg.cth = cth;
  ADBMS_SetCTH(0xFF, cth);
  _stage_cfga_all();
  LOG_I(TAG_PROC, "C/S threshold staged: %d", (int)cth);
}

void BMS_Proc_SetOpenWireRange(bool long_range)
{
  s_cfg.owrng = long_range;
  ADBMS_SetOwrng(0xFF, long_range);
  _stage_cfga_all();
  LOG_I(TAG_PROC, "OW range staged: %s", long_range ? "long" : "short");
}

void BMS_Proc_SetOpenWireTime(uint8_t owa)
{
  if (owa > 7)
    owa = 7;
  s_cfg.owa = owa;
  ADBMS_SetOWA(0xFF, owa);
  _stage_cfga_all();
  LOG_I(TAG_PROC, "OWA staged: %u", (unsigned)owa);
}

void BMS_Proc_SetSoakOn(bool on)
{
  s_cfg.soakon = on;
  ADBMS_SetSoakOn(0xFF, on);
  _stage_cfga_all();
  LOG_I(TAG_PROC, "SOAKON staged: %s", on ? "on" : "off");
}

void BMS_Proc_SetRedundancyMode(uint8_t rd)
{
  s_cfg.rd = rd ? 1 : 0;
  BMS_Acq_SetCellADCOptions(s_cfg.rd, 1, s_cfg.ow);
  LOG_I(TAG_PROC, "C-ADC RD set: %u", (unsigned)s_cfg.rd);
}

void BMS_Proc_SetOpenWireMode(uint8_t ow)
{
  if (ow > 3)
    ow = 3;
  s_cfg.ow = ow;
  BMS_Acq_SetCellADCOptions(s_cfg.rd, 1, s_cfg.ow);
  LOG_I(TAG_PROC, "C-ADC OW set: %u", (unsigned)ow);
}

ADBMS_FC_t BMS_Proc_GetIIRFilterCoeff(void)
{
  return s_cfg.fc;
}
ADBMS_CTH_t BMS_Proc_GetCSThreshold(void)
{
  return s_cfg.cth;
}
bool BMS_Proc_GetOpenWireRange(void)
{
  return s_cfg.owrng;
}
uint8_t BMS_Proc_GetOpenWireTime(void)
{
  return s_cfg.owa;
}
bool BMS_Proc_GetSoakOn(void)
{
  return s_cfg.soakon;
}
uint8_t BMS_Proc_GetRedundancyMode(void)
{
  return s_cfg.rd;
}
uint8_t BMS_Proc_GetOpenWireMode(void)
{
  return s_cfg.ow;
}
