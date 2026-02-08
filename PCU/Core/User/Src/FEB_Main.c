#include "FEB_Main.h"
#include "TPS2482.h"
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

/* ===== TPS2482 I2C CONFIGURATION =====
 *
 * Hardware Setup:
 *   - Number of devices: 1
 *   - I2C Address pins: A0=GND, A1=GND
 *   - Resulting 7-bit address: 0x40 (calculated by TPS2482_I2C_ADDR macro)
 *   - Note: STM32 HAL I2C functions expect 7-bit addresses (they handle the R/W bit internally)
 *
 * Address Calculation:
 *   - TPS2482_I2C_ADDR(A1, A0) macro from TPS2482.h
 *   - Base address: 0b1000000 (0x40)
 *   - A1 and A0 pins can be GND (0x00), VCC (0x01), SDA (0x02), or SCL (0x03)
 *   - Current config: Both pins = GND → 0x40
 */
#define NUM_TPS_DEVICES 1
static uint8_t tps_i2c_address = TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_GND); /* 7-bit address: 0x40 */

/* TPS2482 Configuration
 * CAL calculation:
 *   - R_shunt = 0.012Ω (12 milliohm)
 *   - I_max = 4A
 *   - Current_LSB = I_max / 2^15 = 4 / 32768 = 0.000122 A/LSB
 *   - CAL = 0.00512 / (Current_LSB × R_shunt) = 0.00512 / (0.000122 × 0.012) = 3495
 */
static TPS2482_Configuration tps_config = {
    .config = TPS2482_CONFIG_DEFAULT, /* 0x4127 - Continuous shunt+bus voltage, 128 samples avg, 1.1ms conversion */
    .cal = 3495,                      /* Calibration value for 4A max, 12mΩ shunt */
    .mask = 0x0000,                   /* No alerts configured */
    .alert_lim = 0x0000               /* No alert limit */
};

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
      .log_level = FEB_UART_LOG_INFO,
      .enable_colors = true,
      .enable_timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg);

  // Verify UART DMA RX is running (diagnostic for RX issues)
  if (huart2.RxState != HAL_UART_STATE_BUSY_RX)
  {
    printf("[DIAG] UART DMA RX not started! RxState=%d (expected %d)\r\n", huart2.RxState, HAL_UART_STATE_BUSY_RX);
  }

  // Initialize console (registers built-in commands: help, version, uptime, reboot, log)
  FEB_Console_Init();

  // Register PCU-specific commands
  PCU_RegisterCommands();

  // Connect UART RX to console processor
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

  LOG_I(TAG_MAIN, "=== FEB PCU Starting ===");

  // Initialize CAN library (replaces FEB_CAN_TX_Init/FEB_CAN_RX_Init)
  FEB_CAN_Config_t can_cfg = {
      .hcan1 = &hcan1,
      .hcan2 = &hcan2,
      .get_tick_ms = HAL_GetTick,
  };
  if (FEB_CAN_Init(&can_cfg) != FEB_CAN_OK)
  {
    LOG_E(TAG_MAIN, "CAN initialization failed!");
  }
  else
  {
    LOG_I(TAG_MAIN, "CAN initialized");
  }

  // Start ADCs
  FEB_ADC_Init();
  FEB_ADC_Start(ADC_MODE_DMA);
  LOG_I(TAG_MAIN, "ADC initialized");

  // Diagnostic: Print APPS calibration values
  HAL_Delay(100); // Wait for ADC to stabilize
  LOG_I(TAG_MAIN, "=== APPS Calibration Diagnostics ===");
  LOG_I(TAG_MAIN, "APPS1 Cal: %d - %d mV (range: %d mV)", APPS1_DEFAULT_MIN_VOLTAGE_MV, APPS1_DEFAULT_MAX_VOLTAGE_MV,
        APPS1_DEFAULT_MAX_VOLTAGE_MV - APPS1_DEFAULT_MIN_VOLTAGE_MV);
  LOG_I(TAG_MAIN, "APPS2 Cal: %d - %d mV (range: %d mV)", APPS2_DEFAULT_MIN_VOLTAGE_MV, APPS2_DEFAULT_MAX_VOLTAGE_MV,
        APPS2_DEFAULT_MAX_VOLTAGE_MV - APPS2_DEFAULT_MIN_VOLTAGE_MV);
  LOG_I(TAG_MAIN, "Initial APPS1 read: %d ADC (%.2fV)", FEB_ADC_GetAccelPedal1Raw(), FEB_ADC_GetAccelPedal1Voltage());
  LOG_I(TAG_MAIN, "Initial APPS2 read: %d ADC (%.2fV)", FEB_ADC_GetAccelPedal2Raw(), FEB_ADC_GetAccelPedal2Voltage());
  printf("\r\n");

  // RMS Setup (registers RX callbacks for RMS messages)
  FEB_CAN_RMS_Init();
  LOG_I(TAG_MAIN, "RMS initialized");

  // BMS Setup (registers RX callbacks for BMS messages)
  FEB_CAN_BMS_Init();
  LOG_I(TAG_MAIN, "BMS initialized");

  // TPS2482 Setup
  uint16_t tps_device_id = 0;
  bool tps_init_success = false;

  /* Initialize TPS2482 hardware (configure CAL, CONFIG, MASK, ALERT_LIM registers) */
  TPS2482_Init(&hi2c1, &tps_i2c_address, &tps_config, &tps_device_id, &tps_init_success, NUM_TPS_DEVICES);

  /* Initialize TPS CAN message structure */
  FEB_CAN_TPS_Init();

  if (tps_init_success)
  {
    LOG_I(TAG_MAIN, "TPS2482 initialized successfully");
    LOG_I(TAG_MAIN, "  Device ID: 0x%04X", tps_device_id);
    LOG_I(TAG_MAIN, "  CAL value: %d (0x%04X) for 4A max, 12mOhm shunt", tps_config.cal, tps_config.cal);
    LOG_I(TAG_MAIN, "  Config: 0x%04X (continuous measurement mode)", tps_config.config);
  }
  else
  {
    LOG_E(TAG_MAIN, "TPS2482 initialization FAILED");
    LOG_E(TAG_MAIN, "  Check: I2C1 pins, pull-ups, TPS2482 power, address (0x%02X)", tps_i2c_address);
  }

  LOG_I(TAG_MAIN, "=== Setup Complete ===");
  LOG_I(TAG_MAIN, "Type 'help' for available commands");

  // Start 1ms timer for periodic callbacks
  HAL_TIM_Base_Start_IT(&htim1);
}

void FEB_Main_Loop(void)
{
  // Process any received UART commands (console input)
  FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);

  // Process CAN TX queue (required for FreeRTOS mode, no-op in bare-metal)
  FEB_CAN_TX_Process();

  // Process periodic CAN messages
  FEB_CAN_TX_ProcessPeriodic();
}

void FEB_1ms_Callback(void)
{
  static uint16_t torque_divider = 0;
  static uint16_t tps_divider = 0;
  static uint16_t diagnostics_divider = 0;
  static uint16_t debug_divider = 0;

  // Process deferred heartbeat TX (set by BMS RX callback)
  FEB_CAN_BMS_ProcessHeartbeat();

  // Update torque command at 100Hz (every 10ms)
  torque_divider++;
  if (torque_divider >= 10)
  {
    torque_divider = 0;
    FEB_RMS_Torque();
  }

  // Diagnostics transmission at 50Hz (every 20ms)
  diagnostics_divider++;
  if (diagnostics_divider >= 20)
  {
    diagnostics_divider = 0;
    FEB_CAN_Diagnostics_TransmitBrakeData();
    FEB_CAN_Diagnostics_TransmitAPPSData();
  }

  // TPS2482 power monitoring at 10Hz (every 100ms)
  tps_divider++;
  if (tps_divider >= 100)
  {
    tps_divider = 0;
    FEB_CAN_TPS_Update(&hi2c1, &tps_i2c_address, NUM_TPS_DEVICES);
    FEB_CAN_TPS_Transmit();
  }

  // Debug output at 1Hz (every 1000ms)
  debug_divider++;
  if (debug_divider >= 1000)
  {
    debug_divider = 0;

    APPS_DataTypeDef apps_data;
    Brake_DataTypeDef brake_data;

    FEB_ADC_GetAPPSData(&apps_data);
    FEB_ADC_GetBrakeData(&brake_data);

    // Enhanced debug output with raw ADC values
    LOG_D(TAG_MAIN, "APPS1: %4d ADC (%.2fV / %.1f%%) | APPS2: %4d ADC (%.2fV / %.1f%%) | Avg: %.1f%% | %s",
          FEB_ADC_GetAccelPedal1Raw(), FEB_ADC_GetAccelPedal1Voltage(), apps_data.position1,
          FEB_ADC_GetAccelPedal2Raw(), FEB_ADC_GetAccelPedal2Voltage(), apps_data.position2, apps_data.acceleration,
          apps_data.plausible ? "PLAUS" : "IMPLAUS");

    LOG_D(TAG_MAIN, "Brake1: %4d ADC (%.2fV / %.1f%%) | Brake2: %4d ADC (%.2fV / %.1f%%) | Brake Input: %.1f%% | %s",
          FEB_ADC_GetBrakePressure1Raw(), FEB_ADC_GetBrakePressure1Voltage(), brake_data.pressure1_percent,
          FEB_ADC_GetBrakePressure2Raw(), FEB_ADC_GetBrakePressure2Voltage(), brake_data.pressure2_percent,
          brake_data.brake_position, brake_data.brake_pressed ? "PRESSED" : "RELEASED");
  }
}
