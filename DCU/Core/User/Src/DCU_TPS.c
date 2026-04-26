/**
 ******************************************************************************
 * @file           : DCU_TPS.c
 * @brief          : TPS2482 power monitoring for DCU
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_TPS.h"
#include "i2c.h"
#include "feb_tps.h"
#include "feb_log.h"
#include <string.h>
#include <stdint.h>

/* TPS2482 Configuration (same as PCU) */
#define TPS_SHUNT_RESISTOR_OHMS 0.012f /* 12 milliohm shunt resistor */
#define TPS_MAX_CURRENT_A 4.0f         /* Maximum current in Amps (4A fuse) */

/* Device handle */
static FEB_TPS_Handle_t g_tps_handle = NULL;

/* Cached data */
static DCU_TPS_Data_t g_tps_data = {0};

/**
 * @brief Route TPS library log messages to system logger
 */
static void tps_log_callback(FEB_TPS_LogLevel_t level, const char *msg)
{
  switch (level)
  {
  case FEB_TPS_LOG_ERROR:
    LOG_E(TAG_TPS, "%s", msg);
    break;
  case FEB_TPS_LOG_WARN:
    LOG_W(TAG_TPS, "%s", msg);
    break;
  case FEB_TPS_LOG_INFO:
    LOG_I(TAG_TPS, "%s", msg);
    break;
  case FEB_TPS_LOG_DEBUG:
    LOG_D(TAG_TPS, "%s", msg);
    break;
  default:
    break;
  }
}

void DCU_TPS_Init(void)
{
  g_tps_handle = NULL;
  memset(&g_tps_data, 0, sizeof(g_tps_data));
  g_tps_data.valid = false;
  LOG_I(TAG_TPS, "TPS subsystem initialized");
}

void DCU_TPS_Update(void)
{
  /* Initialize library and register device on first call */
  if (g_tps_handle == NULL)
  {
    FEB_TPS_LibConfig_t lib_cfg = {
        .log_func = tps_log_callback,
        .log_level = FEB_TPS_LOG_INFO,
    };

    FEB_TPS_Status_t init_status = FEB_TPS_Init(&lib_cfg);
    if (init_status != FEB_TPS_OK)
    {
      LOG_E(TAG_TPS, "TPS library init failed: %s", FEB_TPS_StatusToString(init_status));
      return;
    }

    FEB_TPS_DeviceConfig_t dev_cfg = {
        .hi2c = &hi2c1,
        .i2c_addr = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND), /* 0x40 */
        .r_shunt_ohms = TPS_SHUNT_RESISTOR_OHMS,
        .i_max_amps = TPS_MAX_CURRENT_A,
        .config_reg = FEB_TPS_CONFIG_DEFAULT,
        .name = "DCU",
    };

    FEB_TPS_Status_t status = FEB_TPS_DeviceRegister(&dev_cfg, &g_tps_handle);
    if (status != FEB_TPS_OK)
    {
      LOG_E(TAG_TPS, "TPS device register failed: %s", FEB_TPS_StatusToString(status));
      return;
    }

    LOG_I(TAG_TPS, "TPS device registered");
  }

  /* Poll measurements */
  FEB_TPS_MeasurementScaled_t scaled;
  FEB_TPS_Status_t status = FEB_TPS_PollScaled(g_tps_handle, &scaled);

  if (status == FEB_TPS_OK)
  {
    /* Update cached data with clamping to prevent overflow */
    g_tps_data.bus_voltage_mv = (scaled.bus_voltage_mv > UINT16_MAX) ? UINT16_MAX : (uint16_t)scaled.bus_voltage_mv;
    g_tps_data.current_ma = (scaled.current_ma > INT16_MAX)   ? INT16_MAX
                            : (scaled.current_ma < INT16_MIN) ? INT16_MIN
                                                              : (int16_t)scaled.current_ma;
    g_tps_data.shunt_voltage_uv = scaled.shunt_voltage_uv;
    g_tps_data.valid = true;
  }
  else
  {
    /* Mark data as invalid on poll failure */
    g_tps_data.valid = false;
    LOG_D(TAG_TPS, "TPS poll failed: %s", FEB_TPS_StatusToString(status));
  }
}

void DCU_TPS_GetData(DCU_TPS_Data_t *data)
{
  if (data != NULL)
  {
    *data = g_tps_data;
  }
}
