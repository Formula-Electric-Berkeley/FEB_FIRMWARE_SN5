/**
 * @file FEB_ADBMS_App.c
 * @brief BMS Application Layer (legacy control + getter surface).
 *
 * Periodic data acquisition lives in FEB_BMS_Acquisition; data
 * interpretation and fault detection live in FEB_BMS_Processing. This
 * file retains:
 *   - The one-shot boot init (BMS_App_Init)
 *   - Legacy getters read by CAN/state-machine
 *   - BMS_App_SetMode / BMS_App_StopBalancing that stage writes via the
 *     pending-writes bitmask
 *   - Legacy FEB_* compatibility shims for SM/other consumers
 */

#include "FEB_ADBMS_App.h"
#include "FEB_BMS_Processing.h"
#include "ADBMS6830B_Registers.h"
#include "FEB_ADBMS_Platform.h"
#include "BMS_HW_Config.h"
#include "cmsis_os.h"
#include "feb_log.h"
#include <float.h>
#include <string.h>

#define TAG_APP "[BMS_APP]"

#define BMS_SERIAL_ID_LEN (sizeof(((BMS_ICData_t *)0)->serial_id))

/*============================================================================
 * Global State
 *============================================================================*/

BMS_PackData_t g_bms_pack = {0};

/* Legacy error type for compatibility with SM code. */
static uint8_t s_legacy_error_type = 0;

/* Consecutive temperature violation counters for pack-level checks */
static uint8_t s_temp_high_violations = 0;
static uint8_t s_temp_low_violations = 0;

/*============================================================================
 * Private helpers
 *============================================================================*/

static bool _is_valid_serial_id(const uint8_t *sid)
{
  bool all_zero = true;
  bool all_ff = true;
  for (size_t i = 0; i < BMS_SERIAL_ID_LEN; i++)
  {
    if (sid[i] != 0x00)
      all_zero = false;
    if (sid[i] != 0xFF)
      all_ff = false;
  }
  return !(all_zero || all_ff);
}

static void _init_set_mux_channel_zero(uint8_t ic_index)
{
  ADBMS_Memory_t *mem = ADBMS_GetMemory(ic_index);
  if (mem == NULL)
    return;
  CFGARA_t cfg;
  CFGARA_DECODE(mem->cfga.raw, &cfg);
  uint16_t gpo = cfg.GPO;
  gpo &= ~BMS_MUX_SEL_MASK;
  ADBMS_SetGPO(ic_index, gpo);
}

/*============================================================================
 * Initialization
 *============================================================================*/

BMS_AppError_t BMS_App_Init(void)
{
  LOG_I(TAG_APP, "Initializing BMS Application Layer");

  memset(&g_bms_pack, 0, sizeof(g_bms_pack));

  FEB_ADBMS_Platform_Init();

  ADBMS_Error_t err = ADBMS_Init(BMS_TOTAL_ICS);
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_Init failed: %d", err);
    return BMS_APP_ERR_INIT;
  }

  ADBMS_SetActiveCellMask((1u << BMS_CELLS_PER_IC) - 1u);

  err = ADBMS_WakeUp();
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_WakeUp failed: %d", err);
    return BMS_APP_ERR_INIT;
  }
  osDelay(pdMS_TO_TICKS(5));

  err = ADBMS_SoftReset();
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_SoftReset failed: %d", err);
    return BMS_APP_ERR_INIT;
  }
  osDelay(pdMS_TO_TICKS(10));

  err = ADBMS_WakeUp();
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_WakeUp failed: %d", err);
    return BMS_APP_ERR_INIT;
  }
  osDelay(pdMS_TO_TICKS(2));

  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    ADBMS_SetRefOn(ic, true);
    ADBMS_SetUVThreshold(ic, BMS_CELL_UV_NORMAL_MV);
    ADBMS_SetOVThreshold(ic, BMS_CELL_OV_NORMAL_MV);
    ADBMS_SetDischargeTimeout(ic, BMS_DISCHARGE_TIMEOUT_CODE);
    ADBMS_SetDischarge(ic, 0x0000);
    _init_set_mux_channel_zero(ic);
  }

  err = ADBMS_WriteConfig();
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_WriteConfig failed: %d", err);
    return BMS_APP_ERR_COMM;
  }

  err = ADBMS_ReadAllRegistersToCache();
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_ReadAllRegistersToCache failed: %d", err);
    return BMS_APP_ERR_COMM;
  }

  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    uint8_t sid[BMS_SERIAL_ID_LEN];
    ADBMS_GetSerialID(ic, sid);

    uint8_t bank = ic / BMS_ICS_PER_BANK;
    uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;
    memcpy(g_bms_pack.banks[bank].ics[ic_in_bank].serial_id, sid, BMS_SERIAL_ID_LEN);

    if (!_is_valid_serial_id(sid))
    {
      LOG_E(TAG_APP, "IC %d has invalid serial ID", ic);
      return BMS_APP_ERR_INIT;
    }
    LOG_I(TAG_APP, "IC %d SID: %02X%02X%02X%02X%02X%02X", ic, sid[0], sid[1], sid[2], sid[3], sid[4], sid[5]);
  }

  if (!ADBMS_IsChainCommOk())
  {
    LOG_E(TAG_APP, "Communication check failed, mask: 0x%04X", ADBMS_GetFailedICMask());
    return BMS_APP_ERR_COMM;
  }

  g_bms_pack.initialized = true;
  g_bms_pack.mode = BMS_MODE_NORMAL;

  LOG_I(TAG_APP, "BMS Application Layer initialized successfully");
  return BMS_APP_OK;
}

/*============================================================================
 * Balancing / mode (legacy API surface)
 *============================================================================*/

<<<<<<< HEAD
=======
BMS_AppError_t BMS_App_ProcessVoltage(void)
{
  if (!g_bms_pack.initialized)
  {
    return BMS_APP_ERR_INIT;
  }

  /* Reset error state at start of each processing pass */
  g_bms_pack.last_error = BMS_APP_OK;

  ADBMS_Error_t err;

  /* 1. Start C-ADC conversion
   * rd=1 (redundant), cont=0 (single), dcp=1 (discharge permitted),
   * rstf=0 (no reset), ow=0 (no open wire) */
  err = ADBMS_StartCellADC(1, 0, 1, 0, 0);
  if (err != ADBMS_OK)
  {
    g_bms_pack.total_pec_errors++;
    return BMS_APP_ERR_COMM;
  }

  /* 2. Poll for C-ADC completion */
  err = ADBMS_PollCADC(BMS_ADC_POLL_TIMEOUT_MS);
  if (err != ADBMS_OK)
  {
    LOG_W(TAG_APP, "C-ADC poll timeout");
    return BMS_APP_ERR_COMM;
  }

  /* 3. Start S-ADC conversion for redundancy check
   * cont=0, dcp=1, ow=0 */
  err = ADBMS_StartSADC(0, 1, 0);
  if (err != ADBMS_OK)
  {
    g_bms_pack.total_pec_errors++;
    return BMS_APP_ERR_COMM;
  }

  /* 4. Poll for S-ADC completion */
  err = ADBMS_PollSADC(BMS_ADC_POLL_TIMEOUT_MS);
  if (err != ADBMS_OK)
  {
    LOG_W(TAG_APP, "S-ADC poll timeout");
    return BMS_APP_ERR_COMM;
  }

  /* 5. Read all cell voltages (bulk read) */
  err = ADBMS_ReadAllCellVoltages();
  if (err != ADBMS_OK)
  {
    g_bms_pack.total_pec_errors++;
    return BMS_APP_ERR_COMM;
  }

  /* 6. Read all S-voltages for redundancy verification */
  err = ADBMS_ReadAllSVoltages();
  if (err != ADBMS_OK)
  {
    g_bms_pack.total_pec_errors++;
    /* Continue anyway - S is optional */
  }

  /* 7. Read status D for UV/OV flags */
  ADBMS_ReadStatusD();

  /* 8. Process and validate voltages */
  _process_cell_voltages();

  g_bms_pack.voltage_read_count++;
  g_bms_pack.voltage_valid = true;

  return g_bms_pack.last_error;
}

static void _process_cell_voltages(void)
{
  float pack_total = 0.0f;
  float pack_min = FLT_MAX;
  float pack_max = -FLT_MAX;

  uint16_t uv_threshold_mv, ov_threshold_mv;
  _get_thresholds_for_mode(&uv_threshold_mv, &ov_threshold_mv);

  for (uint8_t bank = 0; bank < BMS_NUM_BANKS; bank++)
  {
    float bank_total = 0.0f;
    float bank_min = FLT_MAX;
    float bank_max = -FLT_MAX;

    for (uint8_t ic_idx = 0; ic_idx < BMS_ICS_PER_BANK; ic_idx++)
    {
      uint8_t global_ic = bank * BMS_ICS_PER_BANK + ic_idx;

      /* Update comm status from driver */
      ADBMS_ICStatus_t *status = ADBMS_GetStatus(global_ic);
      if (status != NULL)
      {
        g_bms_pack.banks[bank].ics[ic_idx].comm_ok = status->comm_ok;
        g_bms_pack.banks[bank].ics[ic_idx].pec_errors = status->pec_error_count;
      }

      for (uint8_t cell = 0; cell < BMS_CELLS_PER_IC; cell++)
      {
        /* Get C-ADC and S-ADC voltages */
        int32_t v_c_mv = ADBMS_GetCellVoltage_mV(global_ic, cell);
        int32_t v_s_mv = ADBMS_GetSVoltage_mV(global_ic, cell);

        BMS_CellData_t *c = &g_bms_pack.banks[bank].ics[ic_idx].cells[cell];
        c->voltage_C_V = (float)v_c_mv / 1000.0f;
        c->voltage_S_V = (float)v_s_mv / 1000.0f;

        /* Redundancy check (C vs S) */
        int32_t delta = (v_c_mv > v_s_mv) ? (v_c_mv - v_s_mv) : (v_s_mv - v_c_mv);
        if (delta > BMS_C_S_VOLTAGE_TOLERANCE_MV)
        {
          /* Log warning but don't fault immediately */
          LOG_W(TAG_APP, "C/S mismatch B%d IC%d C%d: C=%dmV S=%dmV", bank, ic_idx, cell, (int)v_c_mv, (int)v_s_mv);
        }

        /* Undervoltage check */
        if (v_c_mv < (int32_t)uv_threshold_mv)
        {
          c->uv_count++;
          if (c->uv_count >= BMS_VOLTAGE_ERROR_THRESHOLD)
          {
            g_bms_pack.last_error = BMS_APP_ERR_VOLTAGE_UV;
            g_bms_pack.error_bank = bank;
            g_bms_pack.error_ic = ic_idx;
            g_bms_pack.error_cell = cell;
            LOG_E(TAG_APP, "UV fault: B%d IC%d C%d = %dmV < %dmV", bank, ic_idx, cell, (int)v_c_mv, uv_threshold_mv);
          }
        }
        else
        {
          c->uv_count = 0;
        }

        /* Overvoltage check */
        if (v_c_mv > (int32_t)ov_threshold_mv)
        {
          c->ov_count++;
          if (c->ov_count >= BMS_VOLTAGE_ERROR_THRESHOLD)
          {
            g_bms_pack.last_error = BMS_APP_ERR_VOLTAGE_OV;
            g_bms_pack.error_bank = bank;
            g_bms_pack.error_ic = ic_idx;
            g_bms_pack.error_cell = cell;
            LOG_E(TAG_APP, "OV fault: B%d IC%d C%d = %dmV > %dmV", bank, ic_idx, cell, (int)v_c_mv, ov_threshold_mv);
          }
        }
        else
        {
          c->ov_count = 0;
        }

        /* Accumulate totals */
        bank_total += c->voltage_C_V;
        if (c->voltage_C_V < bank_min)
          bank_min = c->voltage_C_V;
        if (c->voltage_C_V > bank_max)
          bank_max = c->voltage_C_V;
      }
    }

    /* Store bank aggregates */
    g_bms_pack.banks[bank].total_voltage_V = bank_total;
    g_bms_pack.banks[bank].min_voltage_V = bank_min;
    g_bms_pack.banks[bank].max_voltage_V = bank_max;
    g_bms_pack.banks[bank].voltage_valid = 1;

    /* Accumulate pack totals */
    pack_total += bank_total;
    if (bank_min < pack_min)
      pack_min = bank_min;
    if (bank_max > pack_max)
      pack_max = bank_max;
  }

  /* Store pack aggregates */
  g_bms_pack.pack_voltage_V = pack_total;
  g_bms_pack.pack_min_cell_V = pack_min;
  g_bms_pack.pack_max_cell_V = pack_max;
}

/*============================================================================
 * Temperature Processing
 *============================================================================*/

BMS_AppError_t BMS_App_ProcessTemperature(void)
{
  if (!g_bms_pack.initialized)
  {
    return BMS_APP_ERR_INIT;
  }

  /* Reset error state at start of each processing pass */
  g_bms_pack.last_error = BMS_APP_OK;

  ADBMS_Error_t err;
  float pack_min_temp = FLT_MAX;
  float pack_max_temp = -FLT_MAX;
  float temp_sum = 0.0f;
  uint16_t temp_count = 0;

  /* Scan all 7 MUX channels (sensors on IN1-IN7) */
  for (uint8_t mux_ch = 0; mux_ch < BMS_TEMP_SENSORS_PER_MUX; mux_ch++)
  {
    /* 1. Set MUX select lines via GPO (for all ICs) */
    for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
    {
      _set_mux_channel(ic, mux_ch);
    }

    /* Write GPO configuration */
    err = ADBMS_WriteRegister(ADBMS_REG_CFGA);
    if (err != ADBMS_OK)
    {
      LOG_W(TAG_APP, "Failed to write MUX channel %d", mux_ch);
      continue;
    }

    /* 2. Wait for MUX to settle */
    osDelay(pdMS_TO_TICKS(BMS_MUX_SETTLE_MS));

    /* 3. Start AUX ADC for all GPIO channels
     * ow=0, pup=0, ch=0 (all channels) */
    err = ADBMS_StartAuxADC(0, 0, 0);
    if (err != ADBMS_OK)
    {
      LOG_W(TAG_APP, "Failed to start AUX ADC for channel %d", mux_ch);
      continue;
    }

    /* 4. Poll for AUX ADC completion */
    err = ADBMS_PollAuxADC(BMS_ADC_POLL_TIMEOUT_MS);
    if (err != ADBMS_OK)
    {
      LOG_W(TAG_APP, "AUX ADC timeout for channel %d", mux_ch);
      continue;
    }

    /* 5. Read auxiliary registers */
    err = ADBMS_ReadAllAux();
    if (err != ADBMS_OK)
    {
      g_bms_pack.total_pec_errors++;
      continue;
    }

    /* 6. Process each MUX (M1-M6 on GPIO1-6) */
    for (uint8_t mux = 0; mux < BMS_TEMP_NUM_MUXES; mux++)
    {
      for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
      {
        /* Get GPIO voltage for this MUX output */
        int32_t gpio_mv = ADBMS_GetGPIOVoltage_mV(ic, mux);

        /* Convert voltage to temperature */
        float temp_C = _voltage_to_temperature(gpio_mv);

        /* Calculate indices */
        uint8_t bank = ic / BMS_ICS_PER_BANK;
        uint8_t sensor_idx = mux * BMS_TEMP_SENSORS_PER_MUX + mux_ch;

        /* Validate and store */
        if (temp_C > -40.0f && temp_C < 85.0f)
        {
          g_bms_pack.banks[bank].temp_sensors_C[sensor_idx] = temp_C;
          g_bms_pack.banks[bank].temp_violations[sensor_idx] = 0;

          /* Track aggregates */
          if (temp_C < pack_min_temp)
            pack_min_temp = temp_C;
          if (temp_C > pack_max_temp)
            pack_max_temp = temp_C;
          temp_sum += temp_C;
          temp_count++;
        }
        else
        {
          /* Invalid reading - possible open/short */
          g_bms_pack.banks[bank].temp_violations[sensor_idx]++;
        }
      }
    }
  }

  /* Read internal IC temperatures from Status A */
  err = ADBMS_ReadStatusA();
  if (err == ADBMS_OK)
  {
    for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
    {
      float ic_temp = ADBMS_GetInternalTemp_C(ic);
      uint8_t bank = ic / BMS_ICS_PER_BANK;
      uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;
      g_bms_pack.banks[bank].ics[ic_in_bank].internal_temp_C = ic_temp;
    }
  }

  /* Update pack aggregates */
  if (temp_count > 0)
  {
    g_bms_pack.pack_min_temp_C = pack_min_temp;
    g_bms_pack.pack_max_temp_C = pack_max_temp;
    g_bms_pack.pack_avg_temp_C = temp_sum / (float)temp_count;

    /* Update bank aggregates */
    for (uint8_t bank = 0; bank < BMS_NUM_BANKS; bank++)
    {
      g_bms_pack.banks[bank].min_temp_C = pack_min_temp;
      g_bms_pack.banks[bank].max_temp_C = pack_max_temp;
      g_bms_pack.banks[bank].avg_temp_C = g_bms_pack.pack_avg_temp_C;
      g_bms_pack.banks[bank].temp_valid = 1;
    }

    /* Validate temperatures and update read count only when we have valid data */
    _validate_temperatures();
    g_bms_pack.temp_read_count++;
    g_bms_pack.temp_valid = true;
  }
  else
  {
    /* No valid temperature readings - mark as invalid */
    g_bms_pack.temp_valid = false;
    for (uint8_t bank = 0; bank < BMS_NUM_BANKS; bank++)
    {
      g_bms_pack.banks[bank].temp_valid = 0;
    }
  }

  return g_bms_pack.last_error;
}

static void _validate_temperatures(void)
{
  /* Convert thresholds from deci-Celsius to Celsius */
  float max_temp_C = (float)BMS_CELL_MAX_TEMP_DC / 10.0f;
  float min_temp_C = (float)BMS_CELL_MIN_TEMP_DC / 10.0f;

  /* Over-temperature check with consecutive violations */
  if (g_bms_pack.pack_max_temp_C > max_temp_C)
  {
    s_temp_high_violations++;
    if (s_temp_high_violations >= BMS_TEMP_ERROR_THRESHOLD)
    {
      g_bms_pack.last_error = BMS_APP_ERR_TEMP_HIGH;
      LOG_E(TAG_APP, "Over-temperature: %.1fC > %.1fC (%d consecutive)", g_bms_pack.pack_max_temp_C, max_temp_C,
            s_temp_high_violations);
    }
  }
  else
  {
    s_temp_high_violations = 0;
  }

  /* Under-temperature check with consecutive violations */
  if (g_bms_pack.pack_min_temp_C < min_temp_C)
  {
    s_temp_low_violations++;
    if (s_temp_low_violations >= BMS_TEMP_ERROR_THRESHOLD)
    {
      g_bms_pack.last_error = BMS_APP_ERR_TEMP_LOW;
      LOG_E(TAG_APP, "Under-temperature: %.1fC < %.1fC (%d consecutive)", g_bms_pack.pack_min_temp_C, min_temp_C,
            s_temp_low_violations);
    }
  }
  else
  {
    s_temp_low_violations = 0;
  }
}

static float _voltage_to_temperature(int32_t voltage_mv)
{
  float V = (float)voltage_mv;

  /* Bounds check for open/short circuit */
  if (V < THERM_MIN_VOLTAGE_MV)
  {
    return -999.0f; /* Open circuit */
  }
  if (V > THERM_MAX_VOLTAGE_MV)
  {
    return 999.0f; /* Short circuit */
  }

  /* Calculate thermistor resistance from voltage divider
   * V = Vs * R_th / (R1 + R_th)
   * Solving for R_th: R_th = V * R1 / (Vs - V) */
  float R_th = V * THERM_R_PULLUP_OHMS / (THERM_VS_MV - V);

  /* Apply Beta formula (simplified Steinhart-Hart)
   * 1/T = 1/T_ref + (1/B) * ln(R/R_ref) */
  float ln_ratio = logf(R_th / THERM_R_REF_OHMS);
  float inv_T = THERM_INV_T_REF + (THERM_INV_BETA * ln_ratio);
  float T_kelvin = 1.0f / inv_T;

  return T_kelvin - THERM_KELVIN_OFFSET;
}

/*============================================================================
 * MUX Control
 *============================================================================*/

static void _set_mux_channel(uint8_t ic_index, uint8_t channel)
{
  /* Get current GPO value and preserve non-MUX bits */
  uint16_t gpo = _get_current_gpo(ic_index);

  /* Clear MUX select bits */
  gpo &= ~BMS_MUX_SEL_MASK;

  /* Set new channel (3-bit value)
   * Channel 0 = IN1, Channel 1 = IN2, ..., Channel 6 = IN7 */
  if (channel & 0x01)
    gpo |= (1 << BMS_MUX_SEL1_BIT);
  if (channel & 0x02)
    gpo |= (1 << BMS_MUX_SEL2_BIT);
  if (channel & 0x04)
    gpo |= (1 << BMS_MUX_SEL3_BIT);

  /* Write to memory (will be applied on next WriteRegister) */
  ADBMS_SetGPO(ic_index, gpo);
}

static uint16_t _get_current_gpo(uint8_t ic_index)
{
  ADBMS_Memory_t *mem = ADBMS_GetMemory(ic_index);
  if (mem == NULL)
    return 0;

  /* Extract GPO from CFGA register (bits in parsed structure) */
  CFGARA_t cfg;
  CFGARA_DECODE(mem->cfga.raw, &cfg);

  return cfg.GPO;
}

/*============================================================================
 * Cell Balancing
 *============================================================================*/

BMS_AppError_t BMS_App_ProcessBalancing(void)
{
  if (!g_bms_pack.initialized)
  {
    return BMS_APP_ERR_INIT;
  }

  /* Check temperature limit - stop balancing if too hot */
  float balance_max_temp_C = (float)BMS_BALANCE_MAX_TEMP_DC / 10.0f;
  if (g_bms_pack.pack_max_temp_C > balance_max_temp_C)
  {
    LOG_W(TAG_APP, "Temperature too high for balancing: %.1fC", g_bms_pack.pack_max_temp_C);
    BMS_App_StopBalancing();
    return BMS_APP_OK;
  }

  /* Find minimum cell voltage */
  float min_V = g_bms_pack.pack_min_cell_V;
  float threshold_V = min_V + ((float)BMS_BALANCE_THRESHOLD_MV / 1000.0f);

  /* Set discharge on cells above threshold */
  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    uint16_t discharge_mask = 0;

    for (uint8_t cell = 0; cell < BMS_CELLS_PER_IC; cell++)
    {
      uint8_t bank = ic / BMS_ICS_PER_BANK;
      uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;
      BMS_CellData_t *c = &g_bms_pack.banks[bank].ics[ic_in_bank].cells[cell];

      if (c->voltage_C_V > threshold_V)
      {
        discharge_mask |= (1 << cell);
        c->is_discharging = 1;
      }
      else
      {
        c->is_discharging = 0;
      }
    }

    ADBMS_SetDischarge(ic, discharge_mask);
  }

  /* Write configuration to enable discharge */
  ADBMS_Error_t err = ADBMS_WriteConfig();
  if (err != ADBMS_OK)
  {
    LOG_W(TAG_APP, "Failed to write discharge config");
    return BMS_APP_ERR_COMM;
  }

  /* Unmute discharge to activate */
  ADBMS_UnmuteDischarge();

  return BMS_APP_OK;
}

>>>>>>> d851afd (debugging)
void BMS_App_StopBalancing(void)
{
  BMS_Proc_RequestStopBalancing();
  LOG_I(TAG_APP, "Balancing stop requested");
}

bool BMS_App_IsBalancingNeeded(void)
{
  float delta = g_bms_pack.pack_max_cell_V - g_bms_pack.pack_min_cell_V;
  float hysteresis_V = (float)BMS_BALANCE_HYSTERESIS_MV / 1000.0f;
  return (delta > hysteresis_V);
}

BMS_AppError_t BMS_App_SetMode(BMS_OpMode_t mode)
{
  BMS_Proc_RequestSetMode(mode);
  return BMS_APP_OK;
}

BMS_OpMode_t BMS_App_GetMode(void)
{
  return g_bms_pack.mode;
}

/*============================================================================
<<<<<<< HEAD
 * Getters
=======
 * Thread-Safe Getters
 * All getters acquire ADBMSMutexHandle to prevent torn reads
>>>>>>> d851afd (debugging)
 *============================================================================*/

float BMS_App_GetPackVoltage(void)
{
  float result = 0.0f;
  if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
  {
    result = g_bms_pack.pack_voltage_V;
    osMutexRelease(ADBMSMutexHandle);
  }
  return result;
}
float BMS_App_GetMinCellVoltage(void)
{
  float result = 0.0f;
  if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
  {
    result = g_bms_pack.pack_min_cell_V;
    osMutexRelease(ADBMSMutexHandle);
  }
  return result;
}
float BMS_App_GetMaxCellVoltage(void)
{
  float result = 0.0f;
  if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
  {
    result = g_bms_pack.pack_max_cell_V;
    osMutexRelease(ADBMSMutexHandle);
  }
  return result;
}

float BMS_App_GetCellVoltage(uint8_t bank, uint8_t ic, uint8_t cell)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK || cell >= BMS_CELLS_PER_IC)
    return 0.0f;
<<<<<<< HEAD
  return g_bms_pack.banks[bank].ics[ic].cells[cell].voltage_C_V;
=======
}
float result = 0.0f;
if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
{
  result = g_bms_pack.banks[bank].ics[ic].cells[cell].voltage_C_V;
  osMutexRelease(ADBMSMutexHandle);
}
return result;
>>>>>>> d851afd (debugging)
}

float BMS_App_GetCellVoltageS(uint8_t bank, uint8_t ic, uint8_t cell)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK || cell >= BMS_CELLS_PER_IC)
    return 0.0f;
<<<<<<< HEAD
  return g_bms_pack.banks[bank].ics[ic].cells[cell].voltage_S_V;
=======
}
float result = 0.0f;
if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
{
  result = g_bms_pack.banks[bank].ics[ic].cells[cell].voltage_S_V;
  osMutexRelease(ADBMSMutexHandle);
}
return result;
>>>>>>> d851afd (debugging)
}

float BMS_App_GetMinTemp(void)
{
  float result = 0.0f;
  if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
  {
    result = g_bms_pack.pack_min_temp_C;
    osMutexRelease(ADBMSMutexHandle);
  }
  return result;
}
float BMS_App_GetMaxTemp(void)
{
  float result = 0.0f;
  if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
  {
    result = g_bms_pack.pack_max_temp_C;
    osMutexRelease(ADBMSMutexHandle);
  }
  return result;
}
float BMS_App_GetAvgTemp(void)
{
  float result = 0.0f;
  if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
  {
    result = g_bms_pack.pack_avg_temp_C;
    osMutexRelease(ADBMSMutexHandle);
  }
  return result;
}

float BMS_App_GetTempSensor(uint8_t bank, uint8_t sensor_idx)
{
  if (bank >= BMS_NUM_BANKS || sensor_idx >= BMS_TEMP_TOTAL_SENSORS)
    return 0.0f;
<<<<<<< HEAD
  return g_bms_pack.banks[bank].temp_sensors_C[sensor_idx];
=======
}
float result = 0.0f;
if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
{
  result = g_bms_pack.banks[bank].temp_sensors_C[sensor_idx];
  osMutexRelease(ADBMSMutexHandle);
}
return result;
>>>>>>> d851afd (debugging)
}

float BMS_App_GetICTemp(uint8_t bank, uint8_t ic)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK)
    return 0.0f;
<<<<<<< HEAD
  return g_bms_pack.banks[bank].ics[ic].internal_temp_C;
=======
}
float result = 0.0f;
if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(10)) == osOK)
{
  result = g_bms_pack.banks[bank].ics[ic].internal_temp_C;
  osMutexRelease(ADBMSMutexHandle);
}
return result;
>>>>>>> d851afd (debugging)
}

bool BMS_App_IsInitialized(void)
{
  return g_bms_pack.initialized;
}
bool BMS_App_IsVoltageValid(void)
{
  return g_bms_pack.voltage_valid;
}
bool BMS_App_IsTempValid(void)
{
  return g_bms_pack.temp_valid;
}

BMS_AppError_t BMS_App_GetLastError(void)
{
  return g_bms_pack.last_error;
}

void BMS_App_ClearError(void)
{
  BMS_Proc_RequestClearError();
}

uint32_t BMS_App_GetActiveErrorMask(void)
{
  return g_bms_pack.active_error_mask;
}
uint8_t BMS_App_GetTempErrorBank(void)
{
  return g_bms_pack.temp_error_bank;
}
uint8_t BMS_App_GetTempErrorSensor(void)
{
  return g_bms_pack.temp_error_sensor;
}
const BMS_PackData_t *BMS_App_GetPackData(void)
{
  return &g_bms_pack;
}

uint32_t BMS_App_GetICPECErrors(uint8_t bank, uint8_t ic)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK)
    return 0;
  return g_bms_pack.banks[bank].ics[ic].pec_errors;
}

uint32_t BMS_App_GetTotalPECErrors(void)
{
  return g_bms_pack.total_pec_errors;
}

/*============================================================================
 * Diagnostics
 *============================================================================*/

void BMS_App_DumpDiagnostics(BMS_PrintFunc_t print_fn)
{
  if (print_fn == NULL)
    return;

  print_fn("=== BMS Pack Status ===\n");
  print_fn("Initialized: %s\n", g_bms_pack.initialized ? "Yes" : "No");
  print_fn("Mode: %d\n", g_bms_pack.mode);
  print_fn("Pack Voltage: %.2f V\n", g_bms_pack.pack_voltage_V);
  print_fn("Cell Range: %.3f - %.3f V\n", g_bms_pack.pack_min_cell_V, g_bms_pack.pack_max_cell_V);
  print_fn("Temp Range: %.1f - %.1f C\n", g_bms_pack.pack_min_temp_C, g_bms_pack.pack_max_temp_C);
  print_fn("Total PEC Errors: %lu\n", (unsigned long)g_bms_pack.total_pec_errors);
  print_fn("Last Error: %d\n", g_bms_pack.last_error);
  print_fn("Last Update: %lu ms\n", (unsigned long)g_bms_pack.last_update_tick_ms);

  for (uint8_t bank = 0; bank < BMS_NUM_BANKS; bank++)
  {
    for (uint8_t ic = 0; ic < BMS_ICS_PER_BANK; ic++)
    {
      print_fn("Bank %d IC %d:\n", bank, ic);
      for (uint8_t cell = 0; cell < BMS_CELLS_PER_IC; cell++)
      {
        BMS_CellData_t *c = &g_bms_pack.banks[bank].ics[ic].cells[cell];
        print_fn("  C%02d: %.3fV (S:%.3fV) %s\n", cell + 1, c->voltage_C_V, c->voltage_S_V,
                 c->is_discharging ? "[BAL]" : "");
      }
    }
  }
}

/*============================================================================
 * Legacy compat shims
 *============================================================================*/

void FEB_Stop_Balance(void)
{
  if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(50)) == osOK)
  {
    BMS_App_StopBalancing();
    osMutexRelease(ADBMSMutexHandle);
  }
}
float FEB_ADBMS_GET_ACC_Total_Voltage(void)
{
  return BMS_App_GetPackVoltage();
}
void FEB_ADBMS_Update_Error_Type(uint8_t error_type)
{
  s_legacy_error_type = error_type;
}
uint8_t FEB_ADBMS_Get_Error_Type(void)
{
  return s_legacy_error_type;
}
