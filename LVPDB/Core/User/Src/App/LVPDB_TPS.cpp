/**
 ******************************************************************************
 * @file           : LVPDB_TPS.cpp
 * @brief          : TPS2482 power rail management
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "LVPDB_TPS.h"
#include "cmsis_os2.h"
#include "feb_log.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;

#if FEB_TPS_USE_FREERTOS
extern osMutexId_t FEB_I2C_mutexHandle;
#endif
extern osMutexId_t tpsDataMutexHandle;

#define ADC_FILTER_EXPONENT 2

/* ============================================================================
 * TPS Device Handles and Data
 * ============================================================================ */

FEB_TPS_Handle_t tps_handles[NUM_TPS2482];

int16_t tps2482_current[NUM_TPS2482];
uint16_t tps2482_bus_voltage[NUM_TPS2482];
double tps2482_shunt_voltage[NUM_TPS2482];

int16_t tps2482_shunt_voltage_raw[NUM_TPS2482];

uint8_t tps2482_i2c_addresses[NUM_TPS2482];
GPIO_TypeDef *tps2482_en_ports[NUM_TPS2482 - 1];
uint16_t tps2482_en_pins[NUM_TPS2482 - 1];
GPIO_TypeDef *tps2482_pg_ports[NUM_TPS2482];
uint16_t tps2482_pg_pins[NUM_TPS2482];

namespace
{

// Device configurations consumed by tps_init_devices
const struct
{
  uint8_t i2c_addr;
  float i_max_amps;
  GPIO_TypeDef *en_port;
  uint16_t en_pin;
  GPIO_TypeDef *pg_port;
  uint16_t pg_pin;
  const char *name;
} tps_device_configs[NUM_TPS2482] = {
    // LV - Low Voltage Source (no EN pin)
    {LV_ADDR, LV_FUSE_MAX, nullptr, 0, LV_PG_GPIO_Port, LV_PG_Pin, "LV"},
    // SH - Shutdown Source
    {SH_ADDR, SH_FUSE_MAX, SH_EN_GPIO_Port, SH_EN_Pin, SH_PG_GPIO_Port, SH_PG_Pin, "SH"},
    // LT - Laptop Branch
    {LT_ADDR, LT_FUSE_MAX, LT_EN_GPIO_Port, LT_EN_Pin, LT_PG_GPIO_Port, LT_PG_Pin, "LT"},
    // BM_L - Braking Servo, Lidar
    {BM_L_ADDR, BM_L_FUSE_MAX, BM_L_EN_GPIO_Port, BM_L_EN_Pin, BM_L_PG_GPIO_Port, BM_L_PG_Pin, "BM_L"},
    // SM - Steering Motor
    {SM_ADDR, SM_FUSE_MAX, SM_EN_GPIO_Port, SM_EN_Pin, SM_PG_GPIO_Port, SM_PG_Pin, "SM"},
    // AF1_AF2 - Accumulator Fans
    {AF1_AF2_ADDR, AF1_AF2_FUSE_MAX, AF1_AF2_EN_GPIO_Port, AF1_AF2_EN_Pin, AF1_AF2_PG_GPIO_Port, AF1_AF2_PG_Pin,
     "AF1_AF2"},
    // CP_RF - Coolant Pump + Radiator Fans
    {CP_RF_ADDR, CP_RF_FUSE_MAX, CP_RF_EN_GPIO_Port, CP_RF_EN_Pin, CP_RF_PG_GPIO_Port, CP_RF_PG_Pin, "CP_RF"},
};

int16_t tps2482_current_raw[NUM_TPS2482];
uint16_t tps2482_bus_voltage_raw[NUM_TPS2482];

// Filtered current values
int32_t tps2482_current_filter[NUM_TPS2482];
bool tps2482_current_filter_init[NUM_TPS2482];

bool tps_power_good[NUM_TPS2482];
bool tps_polled_success[NUM_TPS2482];
bool tps_init_success = false;
uint8_t tps_registered_count = 0;

/**
 * Route TPS library log messages into the platform logging system with level mapping.
 *
 * @param level Log level provided by the TPS library.
 * @param msg   Null-terminated log message to forward.
 */
void tps_log_callback(FEB_TPS_LogLevel_t level, const char *msg)
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

/**
 * Initialize the TPS library and register every TPS2482 in tps_device_configs.
 *
 * One attempt per chip; on failure log + skip and continue so the board still
 * comes up if some chips are missing — `tps_handles[i]` stays nullptr for those.
 *
 * @returns true if at least one device registered, false if every chip failed.
 */
bool tps_init_devices(void)
{
  FEB_TPS_LibConfig_t lib_cfg = {
      .log_func = tps_log_callback,
      .log_level = FEB_TPS_LOG_INFO,
#if FEB_TPS_USE_FREERTOS
      .i2c_mutex = FEB_I2C_mutexHandle,
#endif
  };
  FEB_TPS_Status_t init_status = FEB_TPS_Init(&lib_cfg);
  if (init_status != FEB_TPS_OK)
  {
    LOG_E(TAG_MAIN, "TPS library init failed: %s", FEB_TPS_StatusToString(init_status));
    return false;
  }

  // Populate exported arrays for console commands
  for (uint8_t i = 0; i < NUM_TPS2482; i++)
  {
    tps2482_i2c_addresses[i] = tps_device_configs[i].i2c_addr;
    tps2482_pg_ports[i] = tps_device_configs[i].pg_port;
    tps2482_pg_pins[i] = tps_device_configs[i].pg_pin;
    if (i > 0)
    { // EN arrays don't include LV (index 0)
      tps2482_en_ports[i - 1] = tps_device_configs[i].en_port;
      tps2482_en_pins[i - 1] = tps_device_configs[i].en_pin;
    }
  }

  uint8_t ok_count = 0;
  for (uint8_t i = 0; i < NUM_TPS2482; i++)
  {
    FEB_TPS_DeviceConfig_t cfg = {
        .hi2c = &hi2c1,
        .i2c_addr = tps_device_configs[i].i2c_addr,
        .r_shunt_ohms = R_SHUNT,
        .i_max_amps = tps_device_configs[i].i_max_amps,
        .config_reg = FEB_TPS_CONFIG_DEFAULT,
        .en_gpio_port = tps_device_configs[i].en_port,
        .en_gpio_pin = tps_device_configs[i].en_pin,
        .pg_gpio_port = tps_device_configs[i].pg_port,
        .pg_gpio_pin = tps_device_configs[i].pg_pin,
        .name = tps_device_configs[i].name,
    };
    tps_handles[i] = nullptr;
    FEB_TPS_Status_t status = FEB_TPS_DeviceRegister(&cfg, &tps_handles[i]);
    if (status == FEB_TPS_OK)
    {
      ok_count++;
    }
    else
    {
      tps_handles[i] = nullptr;
      LOG_W(TAG_MAIN, "TPS %s register failed: %s, skipping", tps_device_configs[i].name,
            FEB_TPS_StatusToString(status));
    }
  }

  tps_registered_count = ok_count;
  LOG_I(TAG_MAIN, "TPS init: %u/%u chips OK", (unsigned)ok_count, (unsigned)NUM_TPS2482);
  return ok_count > 0;
}

/**
 * Verify TPS devices' power-good signals and report LV failure.
 *
 * @returns true if every device's PG read succeeded and LV (index 0) is good.
 */
bool tps_check_power_good(void)
{
  bool all_good = true;

  for (uint8_t i = 0; i < NUM_TPS2482; i++)
  {
    tps_power_good[i] = true;
    if (tps_handles[i] == nullptr)
    {
      tps_power_good[i] = false;
      continue;
    }
    bool pg_state = false;
    FEB_TPS_Status_t status = FEB_TPS_ReadPowerGood(tps_handles[i], &pg_state);
    if (status != FEB_TPS_OK)
    {
      LOG_W(TAG_MAIN, "ReadPowerGood failed for %s: %s", tps_device_configs[i].name, FEB_TPS_StatusToString(status));
      tps_power_good[i] = false;
      all_good = false;
      continue;
    }
    if (i == 0 && !pg_state)
    {
      LOG_W(TAG_MAIN, "LV power not good!");
      all_good = false;
    }
  }

  return all_good;
}

/**
 * Apply an exponential IIR low-pass filter to an array of int16 samples.
 *
 * The filter uses a fixed shift-based exponential smoothing controlled by ADC_FILTER_EXPONENT.
 * For each element i the function updates filters[i] and writes the filtered result into data_out[i].
 *
 * @param filter_initialized Per-element flags indicating whether the corresponding filter state has been
 * initialized; on first use the accumulator and output are initialized from the input.
 */
void tps_current_iir(int16_t *data_in, int16_t *data_out, int32_t *filters, uint8_t length, bool *filter_initialized)
{
  for (uint8_t i = 0; i < length; i++)
  {
    if (!filter_initialized[i])
    {
      filters[i] = data_in[i] << ADC_FILTER_EXPONENT;
      data_out[i] = data_in[i];
      filter_initialized[i] = true;
    }
    else
    {
      filters[i] += data_in[i] - (filters[i] >> ADC_FILTER_EXPONENT);
      data_out[i] = (int16_t)(filters[i] >> ADC_FILTER_EXPONENT);
    }
  }
}

/**
 * Convert raw TPS2482 sensor readings into scaled engineering units and apply current filtering.
 *
 * Converts per-device raw bus and shunt ADC readings into bus voltage (volts) and shunt voltage (millivolts)
 * using FEB_TPS_CONV_VBUS_V_PER_LSB and FEB_TPS_CONV_VSHUNT_MV_PER_LSB, converts raw current counts into
 * currents using each device's CURRENT_LSB constant, then smooths the resulting current array with an IIR filter.
 *
 * Notes:
 * - Input arrays (raw values) are sign-corrected by FEB_TPS_PollRaw.
 * - The function updates tps2482_bus_voltage, tps2482_shunt_voltage, and tps2482_current in place, and advances
 *   tps2482_current_filter state via tps_current_iir.
 */
void tps_variable_conversion(void)
{
  // Convert bus voltage and shunt voltage using library constants
  for (uint8_t i = 0; i < NUM_TPS2482; i++)
  {
    tps2482_bus_voltage[i] = FLOAT_TO_UINT16_T(tps2482_bus_voltage_raw[i] * FEB_TPS_CONV_VBUS_V_PER_LSB);
    tps2482_shunt_voltage[i] = (tps2482_shunt_voltage_raw[i] * FEB_TPS_CONV_VSHUNT_MV_PER_LSB);
  }

  // Convert current with per-device current LSB values
  tps2482_current[0] = FLOAT_TO_INT16_T(tps2482_current_raw[0] * LV_CURRENT_LSB);
  tps2482_current[1] = FLOAT_TO_INT16_T(tps2482_current_raw[1] * SH_CURRENT_LSB);
  tps2482_current[2] = FLOAT_TO_INT16_T(tps2482_current_raw[2] * LT_CURRENT_LSB);
  tps2482_current[3] = FLOAT_TO_INT16_T(tps2482_current_raw[3] * BM_L_CURRENT_LSB);
  tps2482_current[4] = FLOAT_TO_INT16_T(tps2482_current_raw[4] * SM_CURRENT_LSB);
  tps2482_current[5] = FLOAT_TO_INT16_T(tps2482_current_raw[5] * AF1_AF2_CURRENT_LSB);
  tps2482_current[6] = FLOAT_TO_INT16_T(tps2482_current_raw[6] * CP_RF_CURRENT_LSB);

  tps_current_iir(tps2482_current, tps2482_current, tps2482_current_filter, NUM_TPS2482, tps2482_current_filter_init);
}

} // namespace

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void LVPDB_TPS_Setup(void)
{
  // Initialize TPS devices. One attempt per chip; failures are logged and skipped.
  tps_init_success = tps_init_devices();
  if (tps_init_success)
  {
    LOG_I(TAG_MAIN, "TPS2482 I2C init complete");

    // Start with all rails disabled (LV has no EN pin and is skipped).
    for (uint8_t i = 0; i < NUM_TPS2482; i++)
    {
      if (tps_handles[i] != nullptr && tps_device_configs[i].en_port != nullptr)
      {
        FEB_TPS_Enable(tps_handles[i], false);
      }
    }

    // The shutdown circuit must be powered for the car to come up.
    if (tps_handles[1] != nullptr)
    {
      FEB_TPS_Enable(tps_handles[1], true); // SH
    }

    tps_check_power_good();

    LOG_I(TAG_MAIN, "TPS2482 power rails configured");
  }
  else
  {
    LOG_E(TAG_MAIN, "TPS2482 init failed - all chips unreachable, skipping power rail config");
  }
}

bool LVPDB_TPS_IsInitialized(void)
{
  return tps_init_success;
}

void LVPDB_TPS_Poll(void)
{
  if (!tps_init_success)
  {
    return;
  }

  osMutexAcquire(tpsDataMutexHandle, osWaitForever);

  uint8_t polled = 0;
  for (uint8_t i = 0; i < NUM_TPS2482; i++)
  {
    if (tps_handles[i] == nullptr)
    {
      tps2482_bus_voltage_raw[i] = 0;
      tps2482_current_raw[i] = 0;
      tps2482_shunt_voltage_raw[i] = 0;
      tps_polled_success[i] = false;
      continue;
    }
    uint16_t bv;
    int16_t cur;
    int16_t sv;
    if (FEB_TPS_PollRaw(tps_handles[i], &bv, &cur, &sv) == FEB_TPS_OK)
    {
      tps2482_bus_voltage_raw[i] = bv;
      tps2482_current_raw[i] = cur;
      tps2482_shunt_voltage_raw[i] = sv;
      tps_polled_success[i] = true;
      polled++;
    }
    else
    {
      tps2482_bus_voltage_raw[i] = 0;
      tps2482_current_raw[i] = 0;
      tps2482_shunt_voltage_raw[i] = 0;
      tps_polled_success[i] = false;
    }
  }
  if (polled < tps_registered_count)
  {
    LOG_W(TAG_MAIN, "TPS poll: %u/%u registered devices succeeded", (unsigned)polled, (unsigned)tps_registered_count);
  }

  tps_variable_conversion();

  osMutexRelease(tpsDataMutexHandle);
}

bool LVPDB_TPS_PollOk(uint8_t index)
{
  return (index < NUM_TPS2482) && tps_polled_success[index];
}

bool LVPDB_TPS_PowerGood(uint8_t index)
{
  return (index < NUM_TPS2482) && tps_power_good[index];
}
