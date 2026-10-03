/**
 ******************************************************************************
 * @file           : SN_Task_Sensors.cpp
 * @brief          : Sensor task: sensor bring-up and sampling loop
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "SN_Task_Sensors.hpp"
#include "SN_Config_Messages.hpp"

#include "FEB_CAN_IRTSSensorConfig.h"
#include "FEB_Fusion.h"
#include "FEB_GPS.h"
#include "FEB_IMU.h"
#include "FEB_LinearPotentiometer.h"
#include "FEB_Magnetometer.h"
#include "FEB_SN_Config.h"
#include "FEB_WSS.h"
#include "feb_console.h"
#include "feb_log.h"
#include "tim.h"
#include "../SN_SteeringEncoder.h"

#include <algorithm>

#define TAG_SENSOR "[SENSOR]"

namespace
{
constexpr uint32_t kImuSamplePeriodMs =
    std::min(feb::sn::msg::ImuAccel::kCycleMs, feb::sn::msg::FusionLinAccel::kCycleMs);

} // namespace

#if FEB_SN_HAS_GPS
namespace
{
bool gps_ready = false;

} // namespace
#endif

namespace feb::sn
{

void SensorsInit()
{
  HAL_TIM_Base_Start(&htim5);

#if FEB_SN_HAS_IMU
  if (lsm6dsox_init() != 0)
  {
    LOG_E(TAG_SENSOR, "IMU init failed");
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
  FEB_Fusion_Init(kImuSamplePeriodMs);
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

#if FEB_SN_HAS_LINEAR_POTENTIOMETER
  FEB_LinearPotentiometer_Init();
  FEB_Console_Printf("Linear potentiometers initialized\r\n");
#else
  FEB_Console_Printf("Linear potentiometers absent on this variant\r\n");
#endif

#if FEB_SN_IS_FRONT()
  if (!FEB_Steering_Init())
    LOG_E(TAG_SENSOR, "Steering encoder init failed");
  else
    FEB_Console_Printf("Steering encoder initialized\r\n");
#else
  FEB_Console_Printf("Linear potentiometers absent on this variant\r\n");
#endif

#if FEB_SN_HAS_GPS
  const int gps_result = FEB_GPS_Init();
  if (gps_result != 0)
  {
    LOG_E(TAG_SENSOR, "GPS init failed: %d", gps_result);
  }
  else
  {
    int cfg_result = FEB_GPS_ConfigureOutput(1, 10, 0, 0);
    if (cfg_result >= 0)
    {
      cfg_result = FEB_GPS_SetUpdateRate(10);
    }
    if (cfg_result < 0)
    {
      LOG_W(TAG_SENSOR, "GPS config output failed: %d", cfg_result);
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
#endif

  FEB_CAN_IRTSSensorConfig_Init();
  FEB_Console_Printf("IRTS sensor config ready (irts send)\r\n");

  FEB_Console_Printf("Sensor Node (%s) Setup Complete\r\n", FEB_SN_VARIANT_NAME);
}

void SensorsTick()
{
  static uint32_t t_imu_ms = 0;
  static uint32_t t_wss_ms = 0;
  static uint32_t t_lp_ms = 0;
  static uint32_t prev_fusion_us = 0;
  static bool fusion_dt_primed = false;

  const uint32_t now_ms = HAL_GetTick();

#if FEB_SN_HAS_GPS
  if (gps_ready)
  {
    FEB_GPS_Process();
  }
#endif

  if ((uint32_t)(now_ms - t_imu_ms) >= kImuSamplePeriodMs)
  {
    const uint32_t now_us = __HAL_TIM_GET_COUNTER(&htim5);
    float dt = (float)kImuSamplePeriodMs / 1000.0f;
    if (fusion_dt_primed)
    {
      dt = (float)((uint32_t)(now_us - prev_fusion_us)) / 1.0e6f;
    }
    prev_fusion_us = now_us;
    fusion_dt_primed = true;

#if FEB_SN_HAS_IMU
    read_Acceleration();
    read_Angular_Rate();
    read_IMU_Temperature();
#endif
#if FEB_SN_HAS_MAG
    read_Magnetic_Field_Data();
    read_Mag_Temperature();
#endif
#if FEB_SN_HAS_FUSION
    FEB_Fusion_Update(dt);
#else
    (void)dt;
#endif

    t_imu_ms = now_ms;
  }

  if ((uint32_t)(now_ms - t_wss_ms) >= msg::Wss::kCycleMs)
  {
#if FEB_SN_HAS_WSS
    WSS_Main();
#endif
    t_wss_ms = now_ms;
  }

  if ((uint32_t)(now_ms - t_lp_ms) >= msg::Linpot::kCycleMs)
  {
#if FEB_SN_HAS_LINEAR_POTENTIOMETER
    read_LinearPotentiometer();
#endif

#if FEB_SN_IS_FRONT()
    read_SteeringPosition();
#endif

    t_lp_ms = now_ms;
  }

  FEB_CAN_IRTSSensorConfig_Tick();
}

} // namespace feb::sn
