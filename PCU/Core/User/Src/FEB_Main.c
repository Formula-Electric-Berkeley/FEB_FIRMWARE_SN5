#include "FEB_Main.h"
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

/* TPS2482 I2C Configuration
 * - Address pins: A0=GND, A1=GND → 0x40
 * - R_shunt = 0.012Ω (12 milliohm)
 * - I_max = 4A
 */
static uint8_t tps_i2c_address;

/**
 * Initialize and configure primary hardware subsystems and start the system timer.
 *
 * Performs board-level initialization required at startup: configures UART (including RX/TX buffers,
 * DMA and console callback), initializes the console and registers PCU commands, initializes CAN
 * controllers, starts ADC in DMA mode, initializes RMS and BMS subsystems (including an initial
 * RMS process call to clear lockout), initializes TPS power-monitoring state, and starts the
 * base timer (TIM1) with interrupt.
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
      .level = FEB_LOG_INFO,
      .colors = true,
      .timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_Log_Init(&log_cfg);

  // Initialize console (registers built-in commands: help, version, uptime, reboot, log)
  FEB_Console_Init(true);

  // Register PCU-specific commands
  PCU_RegisterCommands();

  // Connect UART RX to console processor
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

  // CAN initialization
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

  // ADC initialization
  FEB_ADC_Init();
  FEB_ADC_Start(ADC_MODE_DMA);
  LOG_I(TAG_MAIN, "ADC initialized");

  // RMS and BMS initialization
  FEB_CAN_RMS_Init();
  LOG_I(TAG_MAIN, "RMS initialized");

  // Clear RMS lockout (2-second blocking sequence - runs once at startup)
  FEB_RMS_Process();

  FEB_CAN_BMS_Init();
  LOG_I(TAG_MAIN, "BMS initialized");

  // TPS2482 power monitoring initialization
  tps_i2c_address = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND); // 0x40
  FEB_CAN_TPS_Init();
  LOG_I(TAG_MAIN, "TPS initialized (0x%02X)", tps_i2c_address);

  LOG_I(TAG_MAIN, "=== Setup Complete ===");

  HAL_TIM_Base_Start_IT(&htim1);
}

/**
 * Perform the application's main-loop tasks.
 *
 * Processes UART RX data, polls the TPS power monitor at approximately 10 Hz and transmits TPS updates,
 * and services CAN transmit queues (normal and periodic).
 */
void FEB_Main_Loop(void)
{
  FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);

  // TPS power monitoring (rate limited to 10Hz to prevent CAN queue overflow)
  static uint32_t last_tps_tick = 0;
  uint32_t now = HAL_GetTick();
  if (now - last_tps_tick >= 100)
  {
    last_tps_tick = now;
    FEB_CAN_TPS_Update(&hi2c1, &tps_i2c_address, 1);
    FEB_CAN_TPS_Transmit();
  }

  // CAN TX processing
  FEB_CAN_TX_Process();
  FEB_CAN_TX_ProcessPeriodic();

  // NOTE: FEB_RMS_Torque() and diagnostics run from FEB_1ms_Callback every 10-20ms
}

/**
 * Handle periodic tasks driven by the 1 ms system tick.
 *
 * Processes the BMS heartbeat on every invocation, triggers the RMS torque
 * update at a 10 ms cadence, and transmits brake and APPS diagnostics at a
 * 20 ms cadence.
 */
void FEB_1ms_Callback(void)
{
  static uint16_t torque_divider = 0;
  static uint16_t diagnostics_divider = 0;

  FEB_CAN_BMS_ProcessHeartbeat();

  torque_divider++;
  if (torque_divider >= 10)
  {
    torque_divider = 0;
    FEB_RMS_Torque();
  }

  diagnostics_divider++;
  if (diagnostics_divider >= 20)
  {
    diagnostics_divider = 0;
    FEB_CAN_Diagnostics_TransmitBrakeData();
    FEB_CAN_Diagnostics_TransmitAPPSData();
  }
}
