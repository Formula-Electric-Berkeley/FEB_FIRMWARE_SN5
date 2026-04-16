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

/*============================================================================
 * Global State
 *============================================================================*/

BMS_PackData_t g_bms_pack = {0};

/* Legacy error type for compatibility with SM code. */
static uint8_t s_legacy_error_type = 0;

/*============================================================================
 * Private helpers
 *============================================================================*/

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
  /* UV/OV live in CFGB so request that group. */
  ADBMS_RequestWrite(ADBMS_REG_CFGB);
  LOG_I(TAG_APP, "Mode %d staged: UV=%dmV OV=%dmV", mode, uv_mv, ov_mv);
}

static bool _is_valid_serial_id(const uint8_t *sid)
{
  bool all_zero = true;
  bool all_ff = true;
  for (int i = 0; i < 6; i++)
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

  ADBMS_WakeUp();
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
    uint8_t sid[6];
    ADBMS_GetSerialID(ic, sid);

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
  g_bms_pack.mode = mode;
  _stage_mode_thresholds(mode);
  return BMS_APP_OK;
}

BMS_OpMode_t BMS_App_GetMode(void)
{
  return g_bms_pack.mode;
}

/*============================================================================
 * Getters
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
    return 0.0f;
  return g_bms_pack.banks[bank].ics[ic].cells[cell].voltage_C_V;
}

float BMS_App_GetCellVoltageS(uint8_t bank, uint8_t ic, uint8_t cell)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK || cell >= BMS_CELLS_PER_IC)
    return 0.0f;
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
    return 0.0f;
  return g_bms_pack.banks[bank].temp_sensors_C[sensor_idx];
}

float BMS_App_GetICTemp(uint8_t bank, uint8_t ic)
{
  if (bank >= BMS_NUM_BANKS || ic >= BMS_ICS_PER_BANK)
    return 0.0f;
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
  g_bms_pack.active_error_mask = 0;
  g_bms_pack.voltage_error = BMS_APP_OK;
  g_bms_pack.temp_error = BMS_APP_OK;
  g_bms_pack.comm_error = BMS_APP_OK;
  g_bms_pack.last_error = BMS_APP_OK;
  g_bms_pack.consecutive_pec_errors = 0;
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
