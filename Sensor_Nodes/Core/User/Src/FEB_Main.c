#include "FEB_IMU.h"
#include "FEB_Magnetometer.h"
#include "FEB_GPS.h"
#include "FEB_SN_Commands.h"
#include "FEB_WSS.h"
#include "main.h"
#include <string.h>
#include "FEB_Main.h"
#include "FEB_CAN_IMU.h"
// Common libraries
#include "feb_uart.h"
#include "feb_log.h"
#include "feb_console.h"
#include "feb_can_lib.h"

#define TAG_MAIN "[MAIN]"

static bool gps_ready = false;

extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

static uint8_t uart_tx_buf[1024];
static uint8_t uart_rx_buf[256];

void FEB_Update()
{
  read_Acceleration();
  read_Angular_Rate();
  read_Magnetic_Field_Data();
  WSS_Main();
  FEB_CAN_IMU_Tick();
}

void FEB_Init(void)
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
  int uart_result = FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg);
  if (uart_result != FEB_UART_OK)
  {
    Error_Handler();
  }

  // Initialize logging system
  FEB_Log_Config_t log_cfg = {
      .uart_instance = FEB_UART_INSTANCE_1,
      .level = FEB_LOG_TRACE,
      .colors = true,
      .timestamps = true,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_Log_Init(&log_cfg);

  // Initialize console with default commands
  FEB_Console_Init(true);
  FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

  // Register Sensor Node specific commands
  SN_RegisterCommands();

  FEB_Console_Printf("Sensor Node Starting\r\n");

  // Initialize sensors
  int imu_result = lsm6dsox_init();
  if (imu_result != 0)
  {
    LOG_E(TAG_MAIN, "IMU init failed: %d", imu_result);
  }
  else
  {
    FEB_Console_Printf("IMU initialized\r\n");
  }

  lis3mdl_init();
  FEB_Console_Printf("Magnetometer initialized\r\n");

  // Initialize GPS
  int gps_result = FEB_GPS_Init();
  if (gps_result != 0)
  {
    LOG_E(TAG_MAIN, "GPS init failed: %d", gps_result);
    gps_ready = false;
  }
  else
  {
    int cfg_result = FEB_GPS_ConfigureOutput(true, true, false, true); // GGA, GSA, RMC (no GSV)
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

  // Initialize CAN library
  FEB_CAN_Config_t can_cfg = {
      .hcan1 = &hcan1,
      .hcan2 = &hcan2,
      .get_tick_ms = HAL_GetTick,
  };
  FEB_CAN_Init(&can_cfg);
  FEB_Console_Printf("CAN initialized\r\n");

  FEB_Console_Printf("Sensor Node Setup Complete\r\n");
}

void FEB_Main_Loop(void)
{
  FEB_Update();

  FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);
  if (gps_ready)
  {
    FEB_GPS_Process();
  }
  HAL_Delay(100);
}
