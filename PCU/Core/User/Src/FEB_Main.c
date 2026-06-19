#include "FEB_Main.h"
#include "FEB_ADC.h"
#include "FEB_PCU_APPS_Commands.h"
#include "main.h"
#include <stdint.h>
#include <stdio.h>
#include "can.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"

static uint8_t uart_tx_buf[4096];
static uint8_t uart_rx_buf[256];

/* TPS2482 I2C Configuration
 * - Address pins: A0=GND, A1=GND → 0x40
 * - R_shunt = 0.012Ω (12 milliohm)
 * - I_max = 4A
 */
static uint8_t tps_i2c_address;

/* CAN initialization status - gates CAN-dependent subsystems */
static bool can_init_success = false;

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

  // === CHECKPOINT 1: UART/Log ready ===
  LOG_I(TAG_MAIN, "[1/8] UART and Log initialized");
  HAL_Delay(50);

  // Initialize console (registers built-in commands: help, version, uptime, reboot, log)
  FEB_Console_Init(true);

  // Register PCU-specific commands
  PCU_RegisterCommands();

  // Connect UART RX to console processor
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

  // === CHECKPOINT 2: Console ready ===
  LOG_I(TAG_MAIN, "[2/8] Console initialized");
  HAL_Delay(50);

  // CAN initialization
  FEB_CAN_Config_t can_cfg = {
      .hcan1 = &hcan1,
      .hcan2 = &hcan2,
      .get_tick_ms = HAL_GetTick,
  };
  if (FEB_CAN_Init(&can_cfg) != FEB_CAN_OK)
  {
    LOG_E(TAG_MAIN, "CAN initialization failed! Running in degraded mode.");
    can_init_success = false;
    HAL_Delay(50);
  }
  else
  {
    can_init_success = true;
    // === CHECKPOINT 3: CAN ready ===
    LOG_I(TAG_MAIN, "[3/8] CAN initialized");
    HAL_Delay(50);
  }

  // ADC initialization
  FEB_ADC_Init();
  FEB_ADC_Start(ADC_MODE_DMA);

  // Start the ADC sampling trigger. All three ADCs were armed by FEB_ADC_Start
  // in external-trigger mode and convert exactly one coherent scan per TIM2 TRGO
  // (update) event @ ~10 kHz; without TIM2 running they never sample.
  HAL_TIM_Base_Start(&htim2);

  // === CHECKPOINT 4: ADC ready ===
  LOG_I(TAG_MAIN, "[4/8] ADC initialized");
  HAL_Delay(50);

  // RMS and BMS initialization (requires CAN)
  if (can_init_success)
  {
    FEB_CAN_RMS_Init();

    // === CHECKPOINT 5: RMS ready ===
    LOG_I(TAG_MAIN, "[5/8] RMS initialized");
    HAL_Delay(50);

    // NOTE: FEB_CAN_RMS_Init() already commanded the inverter DISABLED (lockout-
    // safe). The inverter stays disabled until FEB_RMS_Torque() enables it on a
    // clean 0->1 edge once BMS drive state is reached.

    FEB_CAN_BMS_Init();

    // === CHECKPOINT 6: BMS ready ===
    LOG_I(TAG_MAIN, "[6/8] BMS initialized");
    HAL_Delay(50);

    // IVT current/voltage sensor RX (pack voltage + current for RMS limiting)
    FEB_CAN_IVT_Init();
    LOG_I(TAG_MAIN, "[6/8] IVT initialized");
    HAL_Delay(50);
  }
  else
  {
    LOG_W(TAG_MAIN, "[5/8] RMS skipped (CAN unavailable)");
    LOG_W(TAG_MAIN, "[6/8] BMS skipped (CAN unavailable)");
    HAL_Delay(50);
  }

  // TPS2482 power monitoring initialization
  tps_i2c_address = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND); // 0x40
  FEB_CAN_TPS_Init();

  // === CHECKPOINT 7: TPS ready ===
  LOG_I(TAG_MAIN, "[7/8] TPS initialized (0x%02X)", tps_i2c_address);
  HAL_Delay(50);

  // === CHECKPOINT 8: Starting timer ===
  LOG_I(TAG_MAIN, "[8/8] Starting 1ms timer...");
  HAL_Delay(50);

  HAL_TIM_Base_Start_IT(&htim1);

  if (can_init_success)
  {
    LOG_I(TAG_MAIN, "=== PCU Setup Complete ===");
  }
  else
  {
    LOG_W(TAG_MAIN, "=== PCU Setup Complete (DEGRADED - CAN FAILED) ===");
  }
  HAL_Delay(50);
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
  PCU_APPS_StreamProcess();

  // BMS heartbeat: flag is set by CAN RX ISR; transmit here in task context to avoid
  // combining with ISR diagnostic TX and saturating the 3 hardware CAN mailboxes.
  if (can_init_success)
  {
    FEB_CAN_BMS_ProcessHeartbeat();
  }

  // TPS power monitoring (rate limited to 4 Hz to keep CAN traffic low)
  static uint32_t last_tps_tick = 0;
  uint32_t now = HAL_GetTick();
  if (now - last_tps_tick >= 250)
  {
    last_tps_tick = now;
    FEB_CAN_TPS_Update(&hi2c1, &tps_i2c_address, 1);
    if (can_init_success)
    {
      FEB_CAN_TPS_Transmit();
    }
  }

  // CAN TX processing (skip if CAN failed)
  if (can_init_success)
  {
    FEB_CAN_TX_Process();
    FEB_CAN_TX_ProcessPeriodic();
  }

  // NOTE: FEB_RMS_Torque() and diagnostics run from FEB_1ms_Callback every 10-20ms
}

/**
 * Handle periodic tasks driven by the 1 ms system tick.
 *
 * Processes the BMS heartbeat on every invocation, triggers the RMS torque
 * update at a 10 ms cadence, and transmits brake, APPS, and raw pedal-voltage
 * diagnostics at a 10 Hz cadence.
 */
void FEB_1ms_Callback(void)
{
  static uint16_t torque_divider = 0;
  static uint16_t brake_divider = 0;
  // Diagnostics are throttled hard (brake/APPS/pedal-mV @ 10 Hz) to keep CAN
  // traffic low; the dividers are offset (brake @ 0 ms, pedal-mV @ 25 ms, APPS @
  // 50 ms) so the three never fire in the same tick. The software TX FIFO in
  // feb_can absorbs any residual bursting.
  static uint16_t apps_divider = 50;
  static uint16_t pedal_mv_divider = 75; // first fire at tick 25, then every 100

  // Latch ONE time-coherent ADC snapshot for this control cycle. MUST run first
  // so every consumer below (APPS plausibility, brake faults, RMS torque, CAN
  // diagnostics, CLI) reads the same sampling instant — APPS1/APPS2, brake1/
  // brake2 and APPS-vs-brake are then mutually coherent and a staggered-read can
  // never inflate a plausibility deviation.
  FEB_ADC_TickSample();

  // Refresh the APPS cache every 1 ms so the implausibility timer
  // accumulates correctly across all consumers (FEB_RMS_Torque,
  // FEB_CAN_Diagnostics_TransmitAPPSData, the CLI snapshot view).
  FEB_ADC_TickAPPS();

  // Brake fault detection at 1 ms: BSE sensor open/short (T.4.3.4/.5, 100 ms
  // latch) and the EV.4.7 brake+throttle check (short debounce, ~immediate).
  // Runs even if CAN init failed — it is local, ADC-only safety logic. Must
  // follow FEB_ADC_TickAPPS() so apps_cache.acceleration is fresh for EV.4.7.
  FEB_ADC_TickBrakeFaults();

  // Skip CAN-dependent operations if CAN init failed
  if (!can_init_success)
  {
    return;
  }

  // BMS heartbeat is processed in FEB_Main_Loop, not here, to avoid combining
  // its TX with RMS + diagnostics and saturating all 3 hardware CAN mailboxes.

  // Torque command to the RMS — control signal, kept relatively fast (50 Hz).
  // Well within the inverter command timeout; drop to >= 10 (100 Hz) if needed.
  torque_divider++;
  if (torque_divider >= 20)
  {
    torque_divider = 0;
    FEB_RMS_Torque();
  }

  // Brake + APPS diagnostics — telemetry only, 10 Hz is plenty.
  brake_divider++;
  if (brake_divider >= 100)
  {
    brake_divider = 0;
    FEB_CAN_Diagnostics_TransmitBrakeData();
  }

  apps_divider++;
  if (apps_divider >= 50)
  {
    apps_divider = 0;
    FEB_CAN_Diagnostics_TransmitAPPSData();
  }

  // Raw pedal sensor voltages (mV) — telemetry only, 10 Hz.
  pedal_mv_divider++;
  if (pedal_mv_divider >= 100)
  {
    pedal_mv_divider = 0;
    FEB_CAN_Diagnostics_TransmitPedalVoltages();
  }
}
