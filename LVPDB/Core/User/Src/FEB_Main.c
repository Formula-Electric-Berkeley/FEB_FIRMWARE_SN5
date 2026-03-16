#include "FEB_Main.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_CAN_TPS.h"
#include "FEB_LVPDB_Commands.h"
#include "feb_can_lib.h"
#include "main.h"
#include <stdint.h>
#include <stdio.h>

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

// Device configurations (will be set up in FEB_TPS_Init_Devices)
static const struct
{
  uint8_t i2c_addr;
  float i_max_amps;
  GPIO_TypeDef *en_port;
  uint16_t en_pin;
  GPIO_TypeDef *pg_port;
  uint16_t pg_pin;
  GPIO_TypeDef *alert_port;
  uint16_t alert_pin;
  const char *name;
} tps_device_configs[NUM_TPS2482] = {
    // LV - Low Voltage Source (no EN pin)
    {LV_ADDR, LV_FUSE_MAX, NULL, 0, LV_PG_GPIO_Port, LV_PG_Pin, LV_Alert_GPIO_Port, LV_Alert_Pin, "LV"},
    // SH - Shutdown Source
    {SH_ADDR, SH_FUSE_MAX, SH_EN_GPIO_Port, SH_EN_Pin, SH_PG_GPIO_Port, SH_PG_Pin, SH_Alert_GPIO_Port, SH_Alert_Pin,
     "SH"},
    // LT - Laptop Branch
    {LT_ADDR, LT_FUSE_MAX, LT_EN_GPIO_Port, LT_EN_Pin, LT_PG_GPIO_Port, LT_PG_Pin, LT_Alert_GPIO_Port, LT_Alert_Pin,
     "LT"},
    // BM_L - Braking Servo, Lidar
    {BM_L_ADDR, BM_L_FUSE_MAX, BM_L_EN_GPIO_Port, BM_L_EN_Pin, BM_L_PG_GPIO_Port, BM_L_PG_Pin, BM_L_Alert_GPIO_Port,
     BM_L_Alert_Pin, "BM_L"},
    // SM - Steering Motor
    {SM_ADDR, SM_FUSE_MAX, SM_EN_GPIO_Port, SM_EN_Pin, SM_PG_GPIO_Port, SM_PG_Pin, SM_Alert_GPIO_Port, SM_Alert_Pin,
     "SM"},
    // AF1_AF2 - Accumulator Fans
    {AF1_AF2_ADDR, AF1_AF2_FUSE_MAX, AF1_AF2_EN_GPIO_Port, AF1_AF2_EN_Pin, AF1_AF2_PG_GPIO_Port, AF1_AF2_PG_Pin,
     AF1_AF2_Alert_GPIO_Port, AF1_AF2_Alert_Pin, "AF1_AF2"},
    // CP_RF - Coolant Pump + Radiator Fans
    {CP_RF_ADDR, CP_RF_FUSE_MAX, CP_RF_EN_GPIO_Port, CP_RF_EN_Pin, CP_RF_PG_GPIO_Port, CP_RF_PG_Pin,
     CP_RF_Alert_GPIO_Port, CP_RF_Alert_Pin, "CP_RF"},
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
 * Initialize the TPS subsystem, populate exported device mappings, and register all TPS2482 devices.
 *
 * This initializes the TPS library, fills public arrays used by the console (I2C addresses,
 * enable pins, and power-good pins), and registers each device into `tps_handles`.
 *
 * @returns `true` if all devices were registered successfully, `false` if any registration failed.
 */

static bool FEB_TPS_Init_Devices(void)
{
  // Initialize TPS library
  FEB_TPS_LibConfig_t lib_cfg = {
      .log_func = tps_log_callback,
      .log_level = FEB_TPS_LOG_INFO,
  };
  FEB_TPS_Init(&lib_cfg);

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

  bool all_success = true;

  for (uint8_t i = 0; i < NUM_TPS2482; i++)
  {
    FEB_TPS_DeviceConfig_t cfg = {
        .hi2c = &hi2c1,
        .i2c_addr = tps_device_configs[i].i2c_addr,
        .r_shunt_ohms = R_SHUNT,
        .i_max_amps = tps_device_configs[i].i_max_amps,
        .config_reg = FEB_TPS_CONFIG_DEFAULT,
        .mask_reg = FEB_TPS_MASK_SOL, // Alert on shunt over-voltage
        .en_gpio_port = tps_device_configs[i].en_port,
        .en_gpio_pin = tps_device_configs[i].en_pin,
        .pg_gpio_port = tps_device_configs[i].pg_port,
        .pg_gpio_pin = tps_device_configs[i].pg_pin,
        .alert_gpio_port = tps_device_configs[i].alert_port,
        .alert_gpio_pin = tps_device_configs[i].alert_pin,
        .name = tps_device_configs[i].name,
    };

    FEB_TPS_Status_t status = FEB_TPS_DeviceRegister(&cfg, &tps_handles[i]);
    if (status != FEB_TPS_OK)
    {
      LOG_E(TAG_MAIN, "TPS init failed for %s: %s", tps_device_configs[i].name, FEB_TPS_StatusToString(status));
      all_success = false;
    }
    else
    {
      LOG_D(TAG_MAIN, "TPS %s registered at 0x%02X", tps_device_configs[i].name, tps_device_configs[i].i2c_addr);
    }
  }

  return all_success;
}

/**
 * Enable or disable all TPS2482 devices except the LV rail.
 *
 * Skips device index 0 (LV) because it does not have an enable pin; attempts to set the enable
 * state for each other registered TPS device and logs failures for devices that expose an EN pin.
 *
 * @param enable `true` to enable devices, `false` to disable them.
 * @returns `true` if every attempted enable/disable operation succeeded, `false` if any failed.
 */
static bool FEB_TPS_Enable_All(bool enable)
{
  bool all_success = true;

  // Skip LV (index 0) - it doesn't have an EN pin
  for (uint8_t i = 1; i < NUM_TPS2482; i++)
  {
    FEB_TPS_Status_t status = FEB_TPS_Enable(tps_handles[i], enable);
    if (status != FEB_TPS_OK && tps_device_configs[i].en_port != NULL)
    {
      LOG_E(TAG_MAIN, "Failed to %s %s", enable ? "enable" : "disable", tps_device_configs[i].name);
      all_success = false;
    }
  }

  return all_success;
}

/**
 * Verify TPS devices' power-good signals and report LV failure.
 *
 * Checks the power-good state for each configured TPS device and logs a warning
 * if the LV rail (device index 0) reports not power-good.
 *
 * @returns `true` if all devices' power-good states are considered healthy,
 *          `false` otherwise (returns `false` when LV (index 0) power-good is not set).
 */
static bool FEB_TPS_Check_Power_Good(void)
{
  bool all_good = true;

  for (uint8_t i = 0; i < NUM_TPS2482; i++)
  {
    bool pg_state = false;
    FEB_TPS_Status_t status = FEB_TPS_ReadPowerGood(tps_handles[i], &pg_state);

    if (status == FEB_TPS_OK)
    {
      // For LV (index 0), we expect power good
      // For others, we expect power good to match enable state (currently disabled)
      if (i == 0 && !pg_state)
      {
        LOG_W(TAG_MAIN, "LV power not good!");
        all_good = false;
      }
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
      .log_level = FEB_UART_LOG_DEBUG,
      .enable_colors = true,
      .enable_timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg);

  // Initialize console
  FEB_Console_Init();
  LVPDB_RegisterCommands();
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

  LOG_I(TAG_MAIN, "Beginning Setup");

  // I2C scan for debugging
  printf("Starting I2C Scanning: \r\n");
  for (uint8_t i = 1; i < 128; i++)
  {
    HAL_StatusTypeDef ret = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(i << 1), 3, 5);
    if (ret == HAL_OK)
    {
      printf("0x%X ", i);
    }
    else
    {
      printf("- ");
    }
  }
  printf("Done! \r\n\r\n");

  // Initialize TPS devices using the new library
  int maxiter = 0;
  bool tps_init_success = false;

  while (!tps_init_success && maxiter < 100)
  {
    tps_init_success = FEB_TPS_Init_Devices();
    if (!tps_init_success)
    {
      LOG_W(TAG_MAIN, "TPS init attempt %d failed, retrying...", maxiter);
      HAL_Delay(100); /* 100ms delay to avoid I2C bus contention */
    }
    maxiter++;
  }

  if (tps_init_success)
  {
    LOG_I(TAG_MAIN, "TPS2482 I2C init complete");
  }
  else
  {
    LOG_E(TAG_MAIN, "TPS2482 init failed after %d retries", maxiter);
  }

  // Start with all rails disabled (except LV which has no EN)
  FEB_TPS_Enable_All(false);

  // Check power good states
  FEB_TPS_Check_Power_Good();

  LOG_I(TAG_MAIN, "TPS2482 power rails configured");

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

  LOG_I(TAG_MAIN, "LVPDB Setup Complete");
  LOG_I(TAG_MAIN, "Type 'help' for available commands");

  HAL_TIM_Base_Start_IT(&htim1);
}

#define MAIN_LOOP_POLL_INTERVAL_MS 10

/**
 * Main periodic loop executed from the scheduler; performs periodic TPS sampling and UART receive processing.
 *
 * On each invocation, if at least MAIN_LOOP_POLL_INTERVAL_MS has elapsed since the last poll, triggers a batch poll of all TPS devices to update raw bus/shunt/currents and then runs variable conversion to update scaled values. Always processes pending UART RX for the primary UART instance.
 */
void FEB_Main_Loop(void)
{
  static uint32_t last_poll_tick = 0;
  uint32_t now = HAL_GetTick();

  if (now - last_poll_tick >= MAIN_LOOP_POLL_INTERVAL_MS)
  {
    last_poll_tick = now;

    // Poll all TPS devices using the new library's batch operation
    FEB_TPS_PollAllRaw(tps2482_bus_voltage_raw, tps2482_current_raw, tps2482_shunt_voltage_raw, NUM_TPS2482);

    FEB_Variable_Conversion();
  }

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
  if (tps_divider >= 100)
  {
    tps_divider = 0;
    FEB_CAN_TPS_Tick(tps2482_current_raw, tps2482_bus_voltage_raw, NUM_TPS2482);
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
 * and otherwise update the filter using a fixed-exponent IIR: filters[i] += data_in[i] - (filters[i] >> ADC_FILTER_EXPONENT),
 * then set data_out[i] to the scaled filter value (filters[i] >> ADC_FILTER_EXPONENT).
 *
 * @param data_in Pointer to the array of new input samples.
 * @param data_out Pointer to the array where filtered output samples are written.
 * @param filters Pointer to the array holding internal fixed-point filter accumulators (must be at least `length` elements).
 * @param length Number of elements to process.
 * @param filter_initialized Boolean array indicating per-index whether the corresponding filter accumulator has been initialized; on first use the accumulator and output are initialized from the input.
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
 * - Input arrays (raw values) are expected to be sign-corrected by FEB_TPS_PollAllRaw.
 * - The function updates tps2482_bus_voltage, tps2482_shunt_voltage, and tps2482_current in place, and advances
 *   tps2482_current_filter state via FEB_Current_IIR.
 */
static void FEB_Variable_Conversion(void)
{
  // Convert bus voltage and shunt voltage using library constants
  // Note: current and shunt voltage are now sign-corrected by FEB_TPS_PollAllRaw
  for (uint8_t i = 0; i < NUM_TPS2482; i++)
  {
    tps2482_bus_voltage[i] = FLOAT_TO_UINT16_T(tps2482_bus_voltage_raw[i] * FEB_TPS_CONV_VBUS_V_PER_LSB);
    tps2482_shunt_voltage[i] = (tps2482_shunt_voltage_raw[i] * FEB_TPS_CONV_VSHUNT_MV_PER_LSB);
  }

  // Convert current with per-device current LSB values
  // Note: FEB_TPS_PollAllRaw now returns sign-corrected values
  tps2482_current[0] = FLOAT_TO_INT16_T(tps2482_current_raw[0] * LV_CURRENT_LSB);
  tps2482_current[1] = FLOAT_TO_INT16_T(tps2482_current_raw[1] * SH_CURRENT_LSB);
  tps2482_current[2] = FLOAT_TO_INT16_T(tps2482_current_raw[2] * LT_CURRENT_LSB);
  tps2482_current[3] = FLOAT_TO_INT16_T(tps2482_current_raw[3] * BM_L_CURRENT_LSB);
  tps2482_current[4] = FLOAT_TO_INT16_T(tps2482_current_raw[4] * SM_CURRENT_LSB);
  tps2482_current[5] = FLOAT_TO_INT16_T(tps2482_current_raw[5] * AF1_AF2_CURRENT_LSB);
  tps2482_current[6] = FLOAT_TO_INT16_T(tps2482_current_raw[6] * CP_RF_CURRENT_LSB);

  FEB_Current_IIR(tps2482_current, tps2482_current, tps2482_current_filter, NUM_TPS2482, tps2482_current_filter_init);
}
