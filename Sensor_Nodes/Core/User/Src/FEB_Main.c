#include "FEB_IMU.h"
#include "FEB_Magnetometer.h"
#include "FEB_GPS.h"
#include "FEB_SN_Commands.h"
#include "FEB_WSS.h"
#include "main.h"
#include "tim.h"

#include <string.h>
#include "FEB_Main.h"
#include "FEB_SN_Config.h"
#include "FEB_CAN_IMU.h"
#include "FEB_CAN_Magnetometer.h"
#include "FEB_CAN_WSS.h"
#include "FEB_CAN_GPS.h"
#include "FEB_CAN_Fusion.h"
#include "FEB_CAN_Sensors.h"
#include "FEB_Fusion.h"

#include "feb_uart.h"
#include "feb_log.h"
#include "feb_console.h"
#include "feb_can_lib.h"

#define TAG_MAIN "[MAIN]"

/* Tick periods. IMU tick is microsecond-based (TIM5 @ 1 MHz) so it can run at
 * 1 kHz without HAL_GetTick's 1 ms quantization. Other ticks stay millisecond. */
#define TICK_PERIOD_IMU_US 1000u    /* 1 kHz: IMU + mag sample + Fusion + fusion CAN frames */
#define TICK_PERIOD_WSS_MS 20u      /* 50  Hz: WSS computation + CAN */
#define TICK_PERIOD_RAW_IMU_MS 100u /* 10  Hz: raw IMU + mag CAN frames (debug visibility) */
#define TICK_PERIOD_GPS_MS 200u     /* 5   Hz: GPS frames (six per tick) */
#define TICK_PERIOD_TEMP_MS 1000u   /* 1   Hz: temperatures */

static bool gps_ready = false;

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

static uint8_t uart_tx_buf[1024];
static uint8_t uart_rx_buf[256];

void FEB_Init(void)
{
  /* UART first (logging depends on it). */
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
  if (FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg) != FEB_UART_OK)
  {
    Error_Handler();
  }

  FEB_Log_Config_t log_cfg = {
      .uart_instance = FEB_UART_INSTANCE_1,
      .level = FEB_LOG_TRACE,
      .colors = true,
      .timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_Log_Init(&log_cfg);

  FEB_Console_Init(true);
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

  SN_RegisterCommands();

  FEB_Console_Printf("Sensor Node (%s) Starting\r\n", FEB_SN_VARIANT_NAME);

  /* Free-running 1 MHz µs counter for Fusion dt and WSS edge timestamps. */
  HAL_TIM_Base_Start(&htim5);

#if FEB_SN_HAS_IMU
  if (lsm6dsox_init() != 0)
  {
    LOG_E(TAG_MAIN, "IMU init failed");
  }
  else
  {
    FEB_Console_Printf("IMU initialized\r\n");
  }
#else
  FEB_Console_Printf("IMU absent on this variant\r\n");
#endif

#if FEB_SN_HAS_MAG
  lis3mdl_init();
  FEB_Console_Printf("Magnetometer initialized\r\n");
#else
  FEB_Console_Printf("Magnetometer absent on this variant\r\n");
#endif

#if FEB_SN_HAS_FUSION
  FEB_Fusion_Init();
  FEB_Console_Printf("Fusion orientation filter initialized\r\n");
#if FEB_SN_HAS_IMU
  FEB_Console_Printf("Auto-calibrating gyro (1 s, keep car still)...\r\n");
  FEB_Fusion_AutoCalibrate_Gyro();
  FEB_Console_Printf("Gyro auto-cal done. Mag will refine online during driving.\r\n");
#endif
#endif

#if FEB_SN_HAS_WSS
  FEB_WSS_Init();
  FEB_Console_Printf("WSS initialized\r\n");
#else
  FEB_Console_Printf("WSS absent on this variant\r\n");
#endif

#if FEB_SN_HAS_GPS
  int gps_result = FEB_GPS_Init();
  if (gps_result != 0)
  {
    LOG_E(TAG_MAIN, "GPS init failed: %d", gps_result);
    gps_ready = false;
  }
  else
  {
    int cfg_result = FEB_GPS_ConfigureOutput(true, true, false, true); /* GGA, GSA, RMC */
    if (cfg_result < 0)
    {
      LOG_W(TAG_MAIN, "GPS config output failed: %d", cfg_result);
      FEB_Console_Printf("GPS initialized (degraded)\r\n");
    }
    else
    {
      FEB_Console_Printf("GPS initialized\r\n");
    }
    gps_ready = true;
  }
#else
  FEB_Console_Printf("GPS absent on this variant\r\n");
  gps_ready = false;
#endif

  FEB_CAN_Config_t can_cfg = {
      .hcan1 = &hcan1,
      .hcan2 = &hcan2,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_CAN_Init(&can_cfg);
  FEB_Console_Printf("CAN initialized\r\n");

  FEB_Console_Printf("Sensor Node (%s) Setup Complete\r\n", FEB_SN_VARIANT_NAME);
}

void FEB_Main_Loop(void)
{
  static uint32_t t_imu_us = 0;
  static uint32_t t_wss_ms = 0;
  static uint32_t t_raw_imu_ms = 0;
  static uint32_t t_gps_ms = 0;
  static uint32_t t_temp_ms = 0;
  static uint32_t prev_fusion_us = 0;
  static bool fusion_dt_primed = false;

  const uint32_t now_ms = HAL_GetTick();
  const uint32_t now_us = __HAL_TIM_GET_COUNTER(&htim5);

  /* Drain UART RX every iteration so console + GPS NMEA never starve. */
  FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);
#if FEB_SN_HAS_GPS
  if (gps_ready)
  {
    FEB_GPS_Process();
  }
#endif

  /* 1 kHz: sample IMU+mag, run Fusion with µs-accurate dt, publish related CAN frames.
   * Gated by TIM5 (1 MHz free-running) so we don't get HAL_GetTick's 1 ms quantization.
   * Each sample/Tick is independently compile-gated so the loop stays uniform across variants. */
  if ((uint32_t)(now_us - t_imu_us) >= TICK_PERIOD_IMU_US)
  {
    float dt = 0.001f;
    if (fusion_dt_primed)
    {
      dt = (float)((uint32_t)(now_us - prev_fusion_us)) / 1.0e6f;
    }
    prev_fusion_us = now_us;
    fusion_dt_primed = true;

#if FEB_SN_HAS_IMU
    read_Acceleration();
    read_Angular_Rate();
#endif
#if FEB_SN_HAS_MAG
    read_Magnetic_Field_Data();
#endif
#if FEB_SN_HAS_FUSION
    FEB_Fusion_Update(dt);
#else
    (void)dt;
#endif

    FEB_CAN_Fusion_Tick();

    t_imu_us = now_us;
  }

  /* 50 Hz: recompute wheel RPM from per-edge timestamp ring, transmit. */
  if ((uint32_t)(now_ms - t_wss_ms) >= TICK_PERIOD_WSS_MS)
  {
#if FEB_SN_HAS_WSS
    WSS_Main();
#endif
    FEB_CAN_WSS_Tick();
    t_wss_ms = now_ms;
  }

  /* 5 Hz: GPS frames (six per tick: pos, altitude, motion, time, date, status). */
  if ((uint32_t)(now_ms - t_gps_ms) >= TICK_PERIOD_GPS_MS)
  {
    FEB_CAN_GPS_Tick();
    t_gps_ms = now_ms;
  }

  /* 1 Hz: sensor die temperatures. */
  if ((uint32_t)(now_ms - t_temp_ms) >= TICK_PERIOD_TEMP_MS)
  {
#if FEB_SN_HAS_IMU
    read_IMU_Temperature();
#endif
#if FEB_SN_HAS_MAG
    read_Mag_Temperature();
#endif
    FEB_CAN_Temps_Tick();
    t_temp_ms = now_ms;
  }

  /* 10 Hz: raw IMU + mag CAN frames (downsampled — fusion ships at 1 kHz). */
  if ((uint32_t)(now_ms - t_raw_imu_ms) >= TICK_PERIOD_RAW_IMU_MS)
  {
    FEB_CAN_IMU_Tick();
    FEB_CAN_Magnetometer_Tick();
    t_raw_imu_ms = now_ms;
  }
}
