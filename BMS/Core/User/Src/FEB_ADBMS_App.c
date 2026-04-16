/**
 * @file FEB_ADBMS_App.c
 * @brief BMS Application Layer for ADBMS6830B
 * @author Formula Electric @ Berkeley
 *
 * Implementation of battery monitoring, temperature sensing, and cell balancing.
 */

#include "FEB_ADBMS_App.h"
#include "ADBMS6830B_Registers.h"
#include "FEB_ADBMS_Platform.h"
#include "FEB_Const.h"
#include "cmsis_os.h"
#include "feb_log.h"
#include <math.h>
#include <string.h>
#include <float.h>

#define TAG_APP "[BMS_APP]"

/*============================================================================
 * External References
 *============================================================================*/

extern osMutexId_t ADBMSMutexHandle;

/*============================================================================
 * Global State
 *============================================================================*/

BMS_PackData_t g_bms_pack = {0};

/* Legacy error type for compatibility */
static uint8_t s_legacy_error_type = 0;

/*============================================================================
 * Private Function Prototypes
 *============================================================================*/

static void _get_thresholds_for_mode(uint16_t *uv_mv, uint16_t *ov_mv);
static void _process_cell_voltages(void);
static void _validate_temperatures(uint8_t min_bank, uint8_t min_sensor, uint8_t max_bank, uint8_t max_sensor);
static float _voltage_to_temperature(int32_t voltage_mv);
static void _set_mux_channel(uint8_t ic_index, uint8_t channel);
static uint16_t _get_current_gpo(uint8_t ic_index);
static bool _is_valid_serial_id(const uint8_t *sid);

/*============================================================================
 * Initialization
 *============================================================================*/

BMS_AppError_t BMS_App_Init(void)
{
  LOG_I(TAG_APP, "Initializing BMS Application Layer");

  /* Clear global state */
  memset(&g_bms_pack, 0, sizeof(g_bms_pack));

  /* 1. Platform initialization (DWT cycle counter for us delays) */
  FEB_ADBMS_Platform_Init();

  /* 2. Initialize ADBMS driver with number of ICs */
  ADBMS_Error_t err = ADBMS_Init(BMS_TOTAL_ICS);
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_Init failed: %d", err);
    return BMS_APP_ERR_INIT;
  }

  /* 3. Wake up all ICs from sleep mode */
  err = ADBMS_WakeUp();
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_WakeUp failed: %d", err);
    return BMS_APP_ERR_INIT;
  }

  /* Short delay after wakeup */
  osDelay(pdMS_TO_TICKS(5));

  /* 4. Soft reset all ICs */
  err = ADBMS_SoftReset();
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_SoftReset failed: %d", err);
    return BMS_APP_ERR_INIT;
  }

  /* Wait for reset to complete */
  osDelay(pdMS_TO_TICKS(10));

  /* Wake again after reset */
  ADBMS_WakeUp();
  osDelay(pdMS_TO_TICKS(2));

  /* 5. Configure each IC */
  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    /* Enable reference voltage */
    ADBMS_SetRefOn(ic, true);

    /* Set undervoltage threshold */
    ADBMS_SetUVThreshold(ic, BMS_CELL_UV_NORMAL_MV);

    /* Set overvoltage threshold */
    ADBMS_SetOVThreshold(ic, BMS_CELL_OV_NORMAL_MV);

    /* Set discharge timeout to maximum */
    ADBMS_SetDischargeTimeout(ic, BMS_DISCHARGE_TIMEOUT_CODE);

    /* Clear all discharge (no balancing initially) */
    ADBMS_SetDischarge(ic, 0x0000);

    /* Set GPO for MUX select to channel 0 (default) */
    _set_mux_channel(ic, 0);
  }

  /* 6. Write configuration to all ICs */
  err = ADBMS_WriteConfig();
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_WriteConfig failed: %d", err);
    return BMS_APP_ERR_COMM;
  }

  /* 7. Full register read to verify communication */
  err = ADBMS_ReadAllRegistersToCache();
  if (err != ADBMS_OK)
  {
    LOG_E(TAG_APP, "ADBMS_ReadAllRegistersToCache failed: %d", err);
    return BMS_APP_ERR_COMM;
  }

  /* 8. Read and validate serial IDs */
  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    uint8_t sid[6];
    ADBMS_GetSerialID(ic, sid);

    /* Store in pack data */
    uint8_t bank = ic / BMS_ICS_PER_BANK;
    uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;
    memcpy(g_bms_pack.banks[bank].ics[ic_in_bank].serial_id, sid, 6);

    if (!_is_valid_serial_id(sid))
    {
      LOG_E(TAG_APP, "IC %d has invalid serial ID", ic);
      return BMS_APP_ERR_INIT;
    }

    LOG_I(TAG_APP, "IC %d SID: %02X%02X%02X%02X%02X%02X", ic, sid[0], sid[1], sid[2], sid[3], sid[4], sid[5]);
  }

  /* 9. Verify communication status */
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
 * Voltage Processing
 *============================================================================*/

BMS_AppError_t BMS_App_ProcessVoltage(void)
{
  if (!g_bms_pack.initialized)
  {
    return BMS_APP_ERR_INIT;
  }

  /* Clear previous error - only return error if this cycle has a fault */
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

  /* Clear previous error - only return error if this cycle has a fault */
  g_bms_pack.last_error = BMS_APP_OK;

  ADBMS_Error_t err;
  float pack_min_temp = FLT_MAX;
  float pack_max_temp = -FLT_MAX;
  float temp_sum = 0.0f;
  uint16_t temp_count = 0;

  /* Track location of min/max temperatures for fault reporting */
  uint8_t min_temp_bank = 0;
  uint8_t min_temp_sensor = 0;
  uint8_t max_temp_bank = 0;
  uint8_t max_temp_sensor = 0;

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

        /* Calculate indices - include IC offset within bank for multi-IC support */
        uint8_t bank = ic / BMS_ICS_PER_BANK;
        uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;
        uint8_t sensors_per_ic = BMS_TEMP_NUM_MUXES * BMS_TEMP_SENSORS_PER_MUX;
        uint8_t sensor_idx = (ic_in_bank * sensors_per_ic) + (mux * BMS_TEMP_SENSORS_PER_MUX) + mux_ch;

        /* Validate and store */
        if (temp_C > -40.0f && temp_C < 85.0f)
        {
          g_bms_pack.banks[bank].temp_sensors_C[sensor_idx] = temp_C;
          g_bms_pack.banks[bank].temp_violations[sensor_idx] = 0;

          /* Track aggregates and location of extremes */
          if (temp_C < pack_min_temp)
          {
            pack_min_temp = temp_C;
            min_temp_bank = bank;
            min_temp_sensor = sensor_idx;
          }
          if (temp_C > pack_max_temp)
          {
            pack_max_temp = temp_C;
            max_temp_bank = bank;
            max_temp_sensor = sensor_idx;
          }
          temp_sum += temp_C;
          temp_count++;
        }
        else
        {
          /* Invalid reading - possible open/short */
          g_bms_pack.banks[bank].temp_violations[sensor_idx]++;
          if (g_bms_pack.banks[bank].temp_violations[sensor_idx] >= BMS_TEMP_ERROR_THRESHOLD)
          {
            g_bms_pack.last_error = BMS_APP_ERR_SENSOR;
            LOG_W(TAG_APP, "Temp sensor fault: Bank %d IC %d Sensor %d", bank, ic_in_bank, sensor_idx);
          }
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
  }

  /* Store location of temperature extremes for fault reporting */
  /* Temporarily store in error fields (will be used by _validate_temperatures) */
  g_bms_pack.error_bank = max_temp_bank;
  g_bms_pack.error_cell = max_temp_sensor;

  /* Validate temperatures */
  _validate_temperatures(min_temp_bank, min_temp_sensor, max_temp_bank, max_temp_sensor);

  g_bms_pack.temp_read_count++;
  g_bms_pack.temp_valid = true;

  return g_bms_pack.last_error;
}

static void _validate_temperatures(uint8_t min_bank, uint8_t min_sensor, uint8_t max_bank, uint8_t max_sensor)
{
  /* Convert thresholds from deci-Celsius to Celsius */
  float max_temp_threshold = (float)BMS_CELL_MAX_TEMP_DC / 10.0f;
  float min_temp_threshold = (float)BMS_CELL_MIN_TEMP_DC / 10.0f;

  if (g_bms_pack.pack_max_temp_C > max_temp_threshold)
  {
    g_bms_pack.last_error = BMS_APP_ERR_TEMP_HIGH;
    g_bms_pack.error_bank = max_bank;
    g_bms_pack.error_ic = 0; /* Sensor index stored in error_cell */
    g_bms_pack.error_cell = max_sensor;
    LOG_E(TAG_APP, "Over-temperature: %.1fC > %.1fC (Bank %d Sensor %d)", g_bms_pack.pack_max_temp_C,
          max_temp_threshold, max_bank, max_sensor);
  }

  if (g_bms_pack.pack_min_temp_C < min_temp_threshold)
  {
    g_bms_pack.last_error = BMS_APP_ERR_TEMP_LOW;
    g_bms_pack.error_bank = min_bank;
    g_bms_pack.error_ic = 0; /* Sensor index stored in error_cell */
    g_bms_pack.error_cell = min_sensor;
    LOG_E(TAG_APP, "Under-temperature: %.1fC < %.1fC (Bank %d Sensor %d)", g_bms_pack.pack_min_temp_C,
          min_temp_threshold, min_bank, min_sensor);
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

void BMS_App_StopBalancing(void)
{
  /* Clear all discharge bits */
  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    ADBMS_SetDischarge(ic, 0x0000);
  }

  ADBMS_WriteConfig();
  ADBMS_MuteDischarge();

  /* Clear discharge flags in pack data */
  for (uint8_t bank = 0; bank < BMS_NUM_BANKS; bank++)
  {
    for (uint8_t ic = 0; ic < BMS_ICS_PER_BANK; ic++)
    {
      for (uint8_t cell = 0; cell < BMS_CELLS_PER_IC; cell++)
      {
        g_bms_pack.banks[bank].ics[ic].cells[cell].is_discharging = 0;
      }
    }
  }

  LOG_I(TAG_APP, "Balancing stopped");
}

bool BMS_App_IsBalancingNeeded(void)
{
  float delta = g_bms_pack.pack_max_cell_V - g_bms_pack.pack_min_cell_V;
  float hysteresis_V = (float)BMS_BALANCE_HYSTERESIS_MV / 1000.0f;
  return (delta > hysteresis_V);
}

/*============================================================================
 * Mode Control
 *============================================================================*/

void BMS_App_SetMode(BMS_OpMode_t mode)
{
  g_bms_pack.mode = mode;
}

BMS_OpMode_t BMS_App_GetMode(void)
{
  return g_bms_pack.mode;
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

/*============================================================================
 * Thread-Safe Getters
 *============================================================================*/

float BMS_App_GetPackVoltage(void)
{
  return g_bms_pack.pack_voltage_V;
}

float BMS_App_GetMinCellVoltage(void)
{
  return g_bms_pack.pack_min_cell_V;
}

float BMS_App_GetMaxCellVoltage(void)
{
  return g_bms_pack.pack_max_cell_V;
}

float BMS_App_GetCellVoltage(uint8_t bank, uint8_t ic, uint8_t cell)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK || cell >= BMS_CELLS_PER_IC)
  {
    return 0.0f;
  }
  return g_bms_pack.banks[bank].ics[ic].cells[cell].voltage_C_V;
}

float BMS_App_GetCellVoltageS(uint8_t bank, uint8_t ic, uint8_t cell)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK || cell >= BMS_CELLS_PER_IC)
  {
    return 0.0f;
  }
  return g_bms_pack.banks[bank].ics[ic].cells[cell].voltage_S_V;
}

float BMS_App_GetMinTemp(void)
{
  return g_bms_pack.pack_min_temp_C;
}

float BMS_App_GetMaxTemp(void)
{
  return g_bms_pack.pack_max_temp_C;
}

float BMS_App_GetAvgTemp(void)
{
  return g_bms_pack.pack_avg_temp_C;
}

float BMS_App_GetTempSensor(uint8_t bank, uint8_t sensor_idx)
{
  if (bank >= BMS_NUM_BANKS || sensor_idx >= BMS_TEMP_TOTAL_SENSORS)
  {
    return 0.0f;
  }
  return g_bms_pack.banks[bank].temp_sensors_C[sensor_idx];
}

float BMS_App_GetICTemp(uint8_t bank, uint8_t ic)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK)
  {
    return 0.0f;
  }
  return g_bms_pack.banks[bank].ics[ic].internal_temp_C;
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
  g_bms_pack.last_error = BMS_APP_OK;
}

const BMS_PackData_t *BMS_App_GetPackData(void)
{
  return &g_bms_pack;
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
  print_fn("Total PEC Errors: %lu\n", g_bms_pack.total_pec_errors);
  print_fn("Last Error: %d\n", g_bms_pack.last_error);

  /* Dump individual cell voltages */
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

uint32_t BMS_App_GetICPECErrors(uint8_t bank, uint8_t ic)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK)
  {
    return 0;
  }
  return g_bms_pack.banks[bank].ics[ic].pec_errors;
}

uint32_t BMS_App_GetTotalPECErrors(void)
{
  return g_bms_pack.total_pec_errors;
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

static bool _is_valid_serial_id(const uint8_t *sid)
{
  /* Check for all zeros */
  bool all_zero = true;
  for (int i = 0; i < 6; i++)
  {
    if (sid[i] != 0x00)
    {
      all_zero = false;
      break;
    }
  }
  if (all_zero)
    return false;

  /* Check for all 0xFF */
  bool all_ff = true;
  for (int i = 0; i < 6; i++)
  {
    if (sid[i] != 0xFF)
    {
      all_ff = false;
      break;
    }
  }
  if (all_ff)
    return false;

  return true;
}

/*============================================================================
 * Legacy Compatibility Functions
 *============================================================================*/

void FEB_Stop_Balance(void)
{
  BMS_App_StopBalancing();
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
