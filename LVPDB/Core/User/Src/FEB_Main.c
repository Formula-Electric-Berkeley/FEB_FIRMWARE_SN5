#include "FEB_Main.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_CAN_TPS.h"
#include "FEB_LVPDB_Commands.h"
#include "feb_can.h"
#include "feb_can_lib.h"
#include "feb_console.h"
#include "feb_log.h"
#include "feb_tps.h"
#include "main.h"
#include <stdint.h>
#include <stdio.h>
#include "FEB_CAN_DASH.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim1;
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;

static uint8_t uart_tx_buf[4096];
static uint8_t uart_rx_buf[256];

static void FEB_Variable_Conversion(void);

/* ============================================================================
 * TPS Device Handles and Data
 * ============================================================================ */

// Device handles (in order: LV, SH, LT, BM_L, SM, AF1_AF2, CP_RF)
FEB_TPS_Handle_t tps_handles[NUM_TPS2482];
static struct feb_can_lvpdb_heartbeat_t lvpdb_heartbeat_msg;

// Device configurations consumed by FEB_TPS_Init_Devices
static const struct
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
    {LV_ADDR, LV_FUSE_MAX, NULL, 0, LV_PG_GPIO_Port, LV_PG_Pin, "LV"},
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

// Raw measurement data (for backward compatibility with CAN transmission)
// Note: current and shunt voltage are now sign-corrected by the library
int16_t tps2482_current_raw[NUM_TPS2482];
uint16_t tps2482_bus_voltage_raw[NUM_TPS2482];
int16_t tps2482_shunt_voltage_raw[NUM_TPS2482];

// Filtered current values
int32_t tps2482_current_filter[NUM_TPS2482];
bool tps2482_current_filter_init[NUM_TPS2482];

// Converted values
int16_t tps2482_current[NUM_TPS2482];
uint16_t tps2482_bus_voltage[NUM_TPS2482];
double tps2482_shunt_voltage[NUM_TPS2482];

// Exported arrays for console commands (populated from tps_device_configs)
uint8_t tps2482_i2c_addresses[NUM_TPS2482];
GPIO_TypeDef *tps2482_en_ports[NUM_TPS2482 - 1]; // No EN for LV
uint16_t tps2482_en_pins[NUM_TPS2482 - 1];
GPIO_TypeDef *tps2482_pg_ports[NUM_TPS2482];
uint16_t tps2482_pg_pins[NUM_TPS2482];

FEB_LVPDB_CAN_Data can_data;
bool bus_voltage_healthy = true;
static bool tps_init_success = false;
static uint8_t tps_registered_count = 0;

/**
 * Route TPS library log messages into the platform logging system with level mapping.
 *
 * @param level Log level provided by the TPS library.
 * @param msg   Null-terminated log message to forward.
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

/**
 * Initialize the TPS library and register every TPS2482 in tps_device_configs.
 *
 * One attempt per chip; on failure log + skip and continue so the board still
 * comes up if some chips are missing — `tps_handles[i]` stays NULL for those.
 *
 * @returns true if at least one device registered, false if every chip failed.
 */
static bool FEB_TPS_Init_Devices(void)
{
  FEB_TPS_LibConfig_t lib_cfg = {
      .log_func = tps_log_callback,
      .log_level = FEB_TPS_LOG_INFO,
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
    tps_handles[i] = NULL;
    FEB_TPS_Status_t status = FEB_TPS_DeviceRegister(&cfg, &tps_handles[i]);
    if (status == FEB_TPS_OK)
    {
      ok_count++;
    }
    else
    {
      tps_handles[i] = NULL;
      LOG_W(TAG_MAIN, "TPS %s register failed: %s, skipping", tps_device_configs[i].name,
            FEB_TPS_StatusToString(status));
    }
  }

  tps_registered_count = ok_count;
  LOG_I(TAG_MAIN, "TPS init: %u/%u chips OK", (unsigned)ok_count, (unsigned)NUM_TPS2482);
  return ok_count > 0;
}

static bool tps_power_good[NUM_TPS2482];
/**
 * Verify TPS devices' power-good signals and report LV failure.
 *
 * @returns true if every device's PG read succeeded and LV (index 0) is good.
 */
static bool FEB_TPS_Check_Power_Good(void)
{
  bool all_good = true;

  for (uint8_t i = 0; i < NUM_TPS2482; i++)
  {
    tps_power_good[i] = true;
    if (tps_handles[i] == NULL)
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
 * Perform board startup sequence and configure main subsystems.
 *
 * Initializes UART and console, scans the I2C bus for attached devices,
 * initializes and configures TPS2482 power-management devices (including
 * disabling all non-LV rails and verifying power-good signals), initializes
 * the CAN subsystem and its ping/pong module, configures the brake-light GPIO
 * to off, and starts the 1 kHz timer interrupt for regular system ticks.
 */

void FEB_Main_Setup(void)
{
  // Initialize UART library first (before any LOG calls)
  FEB_UART_Config_t uart_cfg = {
      .huart = &huart2,
      .hdma_tx = &hdma_usart2_tx,
      .hdma_rx = &hdma_usart2_rx,
      .tx_buffer = uart_tx_buf,
      .tx_buffer_size = sizeof(uart_tx_buf),
      .rx_buffer = uart_rx_buf,
      .rx_buffer_size = sizeof(uart_rx_buf),
      .get_tick_ms = HAL_GetTick,
  };
  FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg);

  // Initialize logging system
  FEB_Log_Config_t log_cfg = {
      .uart_instance = FEB_UART_INSTANCE_1,
      .level = FEB_LOG_DEBUG,
      .colors = true,
      .timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_Log_Init(&log_cfg);

  // Initialize console
  FEB_Console_Init(true);
  LVPDB_RegisterCommands();
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

  LOG_I(TAG_MAIN, "Beginning Setup");

  // I2C scan for debugging
  FEB_Console_Printf("Starting I2C Scanning: \r\n");
  for (uint8_t i = 1; i < 128; i++)
  {
    HAL_StatusTypeDef ret = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(i << 1), 3, 5);
    if (ret == HAL_OK)
    {
      FEB_Console_Printf("0x%X ", i);
    }
    else
    {
      FEB_Console_Printf("- ");
    }
  }
  FEB_Console_Printf("Done! \r\n\r\n");

  // Initialize TPS devices. One attempt per chip; failures are logged and skipped.
  tps_init_success = FEB_TPS_Init_Devices();
  if (tps_init_success)
  {
    LOG_I(TAG_MAIN, "TPS2482 I2C init complete");

    // Start with all rails disabled (LV has no EN pin and is skipped).
    for (uint8_t i = 0; i < NUM_TPS2482; i++)
    {
      if (tps_handles[i] != NULL && tps_device_configs[i].en_port != NULL)
      {
        FEB_TPS_Enable(tps_handles[i], false);
      }
    }
    FEB_TPS_Check_Power_Good();

    LOG_I(TAG_MAIN, "TPS2482 power rails configured");
  }
  else
  {
    LOG_E(TAG_MAIN, "TPS2482 init failed - all chips unreachable, skipping power rail config");
  }

  // Initialize brake light to be off
  HAL_GPIO_WritePin(BL_Switch_GPIO_Port, BL_Switch_Pin, GPIO_PIN_RESET);

  // Initialize CAN library
  FEB_CAN_Config_t can_cfg = {
      .hcan1 = &hcan1,
      .hcan2 = NULL,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_CAN_Init(&can_cfg);

  // Initialize ping/pong module
  FEB_CAN_PingPong_Init();
  FEB_CAN_DASH_Init();

  LOG_I(TAG_MAIN, "LVPDB Setup Complete");
  LOG_I(TAG_MAIN, "Type 'help' for available commands");

  HAL_TIM_Base_Start_IT(&htim1);
}

#define MAIN_LOOP_POLL_INTERVAL_MS 50
static bool tps_polled_success[NUM_TPS2482];

/**
 * Main periodic loop. Polls each TPS via its own handle (position-aligned with
 * tps_device_configs[] so per-rail LSBs in FEB_Variable_Conversion stay correct),
 * runs the conversion, and processes any UART input.
 */
void FEB_Main_Loop(void)
{
  static uint32_t last_poll_tick = 0;
  uint32_t now = HAL_GetTick();

  if (tps_init_success && (uint32_t)(now - last_poll_tick) >= MAIN_LOOP_POLL_INTERVAL_MS)
  {
    last_poll_tick = now;

    uint8_t polled = 0;
    for (uint8_t i = 0; i < NUM_TPS2482; i++)
    {
      if (tps_handles[i] == NULL)
      {
        tps2482_bus_voltage_raw[i] = 0;
        tps2482_current_raw[i] = 0;
        tps2482_shunt_voltage_raw[i] = 0;
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

    FEB_Variable_Conversion();
  }

  DASH_State_t dash_state = FEB_CAN_DASH_GetLastState();

  // Device handles (in order: LV, SH, LT, BM_L, SM, AF1_AF2, CP_RF)
  FEB_TPS_Enable(tps_handles[5], dash_state.switch1); // AF1_AF2
  FEB_TPS_Enable(tps_handles[6], dash_state.switch2); // CP_RF

  // FEB_TPS_Enable(tps_handles[3], true); // BM_L

  FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);
}

/**
 * Handle periodic work driven by the 1 kHz system tick.
 *
 * Called from the 1 kHz timer interrupt; maintains internal 1 ms counters and, every 100 ms,
 * invokes the CAN ping/pong maintenance tick and the CAN TPS polling tick using the
 * tps2482_current_raw and tps2482_bus_voltage_raw arrays for all devices (NUM_TPS2482).
 */
void FEB_1ms_Callback(void)
{
  // Process CAN ping/pong every 100ms
  static uint16_t ping_divider = 0;
  ping_divider++;
  if (ping_divider >= 100)
  {
    ping_divider = 0;
    FEB_CAN_PingPong_Tick();
  }

  // Process CAN TPS reading every 100ms
  static uint16_t tps_divider = 0;
  tps_divider++;
  if (tps_divider >= 99)
  {
    tps_divider = 0;
    if (tps_init_success)
    {
      FEB_Variable_Conversion();
      FEB_CAN_TPS_Tick(tps2482_current, tps2482_bus_voltage, NUM_TPS2482);
    }
  }

  static uint16_t heartbeat_divider = 0;
  heartbeat_divider++;
  if (heartbeat_divider >=
      67) // not 100ms to offset message from other two statuses (ran into issue where mailbox got full)
  {
    heartbeat_divider = 0;

    lvpdb_heartbeat_msg.tps_init_failed = !tps_init_success;

    lvpdb_heartbeat_msg.tps_lv_poll_failed = !tps_polled_success[0];
    lvpdb_heartbeat_msg.tps_sh_poll_failed = !tps_polled_success[1];
    lvpdb_heartbeat_msg.tps_lt_poll_failed = !tps_polled_success[2];
    lvpdb_heartbeat_msg.tps_bm_l_poll_failed = !tps_polled_success[3];
    lvpdb_heartbeat_msg.tps_sm_poll_failed = !tps_polled_success[4];
    lvpdb_heartbeat_msg.tps_af1_af2_poll_failed = !tps_polled_success[5];
    lvpdb_heartbeat_msg.tps_cp_rf_poll_failed = !tps_polled_success[6];

    lvpdb_heartbeat_msg.tps_lv_power_not_good = !tps_power_good[0];
    lvpdb_heartbeat_msg.tps_sh_power_not_good = !tps_power_good[1];
    lvpdb_heartbeat_msg.tps_lt_power_not_good = !tps_power_good[2];
    lvpdb_heartbeat_msg.tps_bm_l_power_not_good = !tps_power_good[3];
    lvpdb_heartbeat_msg.tps_sm_power_not_good = !tps_power_good[4];
    lvpdb_heartbeat_msg.tps_af1_af2_power_not_good = !tps_power_good[5];
    lvpdb_heartbeat_msg.tps_cp_rf_power_not_good = !tps_power_good[6];

    lvpdb_heartbeat_msg.dash_state_stale = !FEB_CAN_DASH_IsDataFresh(250);

    uint8_t tx_data[FEB_CAN_LVPDB_HEARTBEAT_LENGTH];
    memset(tx_data, 0x00, sizeof(tx_data));
    feb_can_lvpdb_heartbeat_pack(tx_data, &lvpdb_heartbeat_msg, sizeof(tx_data));

    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_LVPDB_HEARTBEAT_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                    FEB_CAN_LVPDB_HEARTBEAT_LENGTH);
  }
}

/* ============================================================================
 * Data Conversion and Filtering
 * ============================================================================ */

#define ADC_FILTER_EXPONENT 2

/**
 * Apply a per-element IIR low-pass filter to an array of input samples.
 *
 * For each index up to `length`, initialize the internal filter state on first use
 * and otherwise update the filter using a fixed-exponent IIR: filters[i] += data_in[i] - (filters[i] >>
 * ADC_FILTER_EXPONENT), then set data_out[i] to the scaled filter value (filters[i] >> ADC_FILTER_EXPONENT).
 *
 * @param data_in Pointer to the array of new input samples.
 * @param data_out Pointer to the array where filtered output samples are written.
 * @param filters Pointer to the array holding internal fixed-point filter accumulators (must be at least `length`
 * elements).
 * @param length Number of elements to process.
 * @param filter_initialized Boolean array indicating per-index whether the corresponding filter accumulator has been
 * initialized; on first use the accumulator and output are initialized from the input.
 */
static void FEB_Current_IIR(int16_t *data_in, int16_t *data_out, int32_t *filters, uint8_t length,
                            bool *filter_initialized)
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
      data_out[i] = filters[i] >> ADC_FILTER_EXPONENT;
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
 *   tps2482_current_filter state via FEB_Current_IIR.
 */
static void FEB_Variable_Conversion(void)
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

  FEB_Current_IIR(tps2482_current, tps2482_current, tps2482_current_filter, NUM_TPS2482, tps2482_current_filter_init);
}
