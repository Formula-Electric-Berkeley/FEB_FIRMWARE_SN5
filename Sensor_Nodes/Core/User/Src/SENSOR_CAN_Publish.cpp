/**
 ******************************************************************************
 * @file           : SENSOR_CAN_Publish.cpp
 * @brief          : Everything the Sensor Node transmits (C++ publishers)
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * One fc::Publisher<> per outgoing frame. The trait struct's kCycleMs drives
 * the transmit rate; kSender is checked at compile time against kThisNode so
 * a FRONT binary cannot accidentally publish a REAR-only frame.
 *
 * fill_* functions MUST be side-effect-free with respect to their message
 * struct: no I2C, no delays. Sensor reads happen in StartSensorTask.
 */

#include "FEB_SN_Config.h"           /* FEB_SN_IS_FRONT, FEB_SN_HAS_*, feb_sn_* macros */
#include "FEB_IMU.h"                 /* acceleration_mg[], angular_rate_mdps[], imu_temp_c */
#include "FEB_Magnetometer.h"        /* magnetic_mG[], mag_temp_c */
#include "FEB_WSS.h"                 /* left_rpm_x10, right_rpm_x10, left_dir, right_dir */
#include "FEB_GPS.h"                 /* FEB_GPS_Data_t, FEB_GPS_GetLatestData() */
#include "FEB_Fusion.h"              /* FEB_Fusion_GetQuaternion/Euler/Linear/... */
#include "feb_can_publisher.hpp"     /* fc::Publisher<M> */
#include "FEB_LinearPotentiometer.h" /* lp_position_mm[], FEB_LP_COUNT */

namespace fc = feb::can;
namespace fm = feb::can::msg;

/* ============================================================================
 * Per-variant trait-struct aliases.
 *
 * The trait struct differs FRONT vs REAR for split-frame sensors (IMU, Mag,
 * WSS, Linpot). GPS and Fusion currently have only FRONT trait structs on this
 * branch, so their publishers are guarded with #if FEB_SN_IS_FRONT().
 * ============================================================================ */

#if FEB_SN_IS_FRONT()
using ImuAccelMsg = fm::ImuAccelerationData;
using ImuGyroMsg = fm::ImuGyroData;
using MagMsg = fm::MagnetometerData;
using WssMsg = fm::WssFrontData;
using LinpotMsg = fm::LinearPotentiometerFront;
#else
using ImuAccelMsg = fm::ImuAccelerationDataRear;
using ImuGyroMsg = fm::ImuGyroDataRear;
using MagMsg = fm::MagnetometerDataRear;
using WssMsg = fm::WssRearData;
using LinpotMsg = fm::LinearPotentiometerRear;
#endif

/* Sensor temps message has the same trait name on both variants (only the
 * frame ID and encode function differ, which FEB_SN_SENSOR_TEMPS_* handles). */
using SensorTempsMsg = fm::SensorTempsData;

namespace
{

/* ============================================================================
 * IMU (LSM6DSOX)
 * ============================================================================ */
#if FEB_SN_HAS_IMU

bool fill_imu_accel(feb_sn_imu_accel_t &m)
{
  m.acceleration_x = feb_sn_imu_accel_x_encode((double)acceleration_mg[0]);
  m.acceleration_y = feb_sn_imu_accel_y_encode((double)acceleration_mg[1]);
  m.acceleration_z = feb_sn_imu_accel_z_encode((double)acceleration_mg[2]);
  return true;
}
fc::Publisher<ImuAccelMsg> imu_accel_tx{fill_imu_accel};

bool fill_imu_gyro(feb_sn_imu_gyro_t &m)
{
  m.gyro_x = feb_sn_imu_gyro_x_encode((double)angular_rate_mdps[0]);
  m.gyro_y = feb_sn_imu_gyro_y_encode((double)angular_rate_mdps[1]);
  m.gyro_z = feb_sn_imu_gyro_z_encode((double)angular_rate_mdps[2]);
  return true;
}
fc::Publisher<ImuGyroMsg> imu_gyro_tx{fill_imu_gyro};

#endif /* FEB_SN_HAS_IMU */

/* ============================================================================
 * Magnetometer (LIS3MDL)
 * ============================================================================ */
#if FEB_SN_HAS_MAG

bool fill_mag(feb_sn_mag_t &m)
{
  m.magnetometer_x = feb_sn_mag_x_encode((double)magnetic_mG[0]);
  m.magnetometer_y = feb_sn_mag_y_encode((double)magnetic_mG[1]);
  m.magnetometer_z = feb_sn_mag_z_encode((double)magnetic_mG[2]);
  return true;
}
fc::Publisher<MagMsg> mag_tx{fill_mag};

#endif /* FEB_SN_HAS_MAG */

/* ============================================================================
 * Wheel speed sensors
 * ============================================================================ */
#if FEB_SN_HAS_WSS

bool fill_wss(feb_sn_wss_t &m)
{
  /* Globals are mph × 100 (see FEB_WSS.h comment). The DBC comment in
   * FEB_SN_Config.h says the frame carries mph at 0.01 resolution; encode
   * expects physical mph. Divide by 100. */
  m.feb_sn_wss_left = feb_sn_wss_left_encode((double)left_mph_x100 / 100.0);
  m.feb_sn_wss_right = feb_sn_wss_right_encode((double)right_mph_x100 / 100.0);

  uint8_t flags = 0;
  if (left_dir < 0)
    flags |= (1u << 0);
  if (right_dir < 0)
    flags |= (1u << 1);
  m.feb_sn_wss_dir_flags = feb_sn_wss_dir_flags_encode((double)flags);
  return true;
}

fc::Publisher<WssMsg> wss_tx{fill_wss};

#endif /* FEB_SN_HAS_WSS */

/* ============================================================================
 * Linear potentiometer
 * ============================================================================ */
#if FEB_SN_HAS_LINEAR_POTENTIOMETER

/* Same conversion the old C reporter used: mm → 0.01 mm/LSB, clamped to uint16.
 * [0] = left, [1] = right, per FEB_CAN_LinearPotentiometer.c comment. */
static uint16_t mm_to_can_units(float mm)
{
  float scaled = mm * 100.0f;
  if (scaled < 0.0f)
    return 0u;
  if (scaled > 65535.0f)
    return 65535u;
  return (uint16_t)scaled;
}

bool fill_linpot(feb_sn_linpot_t &m)
{
  m.feb_sn_linpot_left = mm_to_can_units(lp_position_mm[0]);
  m.feb_sn_linpot_right = mm_to_can_units(lp_position_mm[1]);
  return true;
}
fc::Publisher<LinpotMsg> linpot_tx{fill_linpot};

#endif /* FEB_SN_HAS_LINEAR_POTENTIOMETER */

/* ============================================================================
 * Sensor die temperatures (IMU + magnetometer)
 * ============================================================================ */
#if FEB_SN_HAS_SENSOR_TEMPS && FEB_SN_IS_FRONT()

bool fill_sensor_temps(feb_sn_sensor_temps_t &m)
{
  m.imu_temp = feb_sn_sensor_temps_imu_encode((double)imu_temp_c);
  m.mag_temp = feb_sn_sensor_temps_mag_encode((double)mag_temp_c);
  return true;
}
fc::Publisher<SensorTempsMsg> sensor_temps_tx{fill_sensor_temps};

#endif /* FEB_SN_HAS_SENSOR_TEMPS && FEB_SN_IS_FRONT */

/* ============================================================================
 * GPS — FRONT-only trait structs on this branch.
 *
 * If you need REAR to publish GPS, the DBC codegen script
 * (sensor_nodes_messages.py) needs to emit GpsPosDataRear / GpsAltitudeDataRear
 * / etc. trait structs, matching the feb_can_gps_*_rear_t C structs that
 * already exist. Until then, only FRONT emits these.
 * ============================================================================ */
#if FEB_SN_HAS_GPS && FEB_SN_IS_FRONT()

bool fill_gps_pos(feb_sn_gps_pos_t &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.latitude = feb_sn_gps_pos_latitude_encode(g.latitude);
  m.longitude = feb_sn_gps_pos_longitude_encode(g.longitude);
  return true;
}
fc::Publisher<fm::GpsPosData> gps_pos_tx{fill_gps_pos};

bool fill_gps_altitude(feb_sn_gps_altitude_t &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.altitude = feb_sn_gps_altitude_altitude_encode(g.altitude);
  m.hdop = feb_sn_gps_altitude_hdop_encode(g.hdop);
  m.vdop = feb_sn_gps_altitude_vdop_encode(g.vdop);
  return true;
}
fc::Publisher<fm::GpsAltitudeData> gps_alt_tx{fill_gps_altitude};

bool fill_gps_motion(feb_sn_gps_motion_t &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.speed = feb_sn_gps_motion_speed_encode(g.speed_kmh);
  m.course = feb_sn_gps_motion_course_encode(g.course);
  return true;
}
fc::Publisher<fm::GpsMotionData> gps_motion_tx{fill_gps_motion};

bool fill_gps_time(feb_sn_gps_time_t &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.hours = feb_sn_gps_time_hours_encode((double)g.hours);
  m.minutes = feb_sn_gps_time_minutes_encode((double)g.minutes);
  m.seconds = feb_sn_gps_time_seconds_encode((double)g.seconds);
  return true;
}
fc::Publisher<fm::GpsTimeData> gps_time_tx{fill_gps_time};

bool fill_gps_date(feb_sn_gps_date_t &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.day = feb_sn_gps_date_day_encode((double)g.day);
  m.month = feb_sn_gps_date_month_encode((double)g.month);
  m.year = feb_sn_gps_date_year_encode((double)g.year);
  return true;
}
fc::Publisher<fm::GpsDateData> gps_date_tx{fill_gps_date};

bool fill_gps_status(feb_sn_gps_status_t &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.fix_type = feb_sn_gps_status_fix_type_encode((double)g.fix);
  m.fix_mode = feb_sn_gps_status_fix_mode_encode((double)g.fix_mode);
  m.sats_in_use = feb_sn_gps_status_sats_in_use_encode((double)g.sats_in_use);
  m.sats_in_view = feb_sn_gps_status_sats_in_view_encode((double)g.sats_in_view);
  m.valid = feb_sn_gps_status_valid_encode((double)g.valid);
  m.has_fix = feb_sn_gps_status_has_fix_encode((double)g.has_fix);
  m.pdop = feb_sn_gps_status_pdop_encode(g.pdop);
  return true;
}
fc::Publisher<fm::GpsStatusData> gps_status_tx{fill_gps_status};

#endif /* FEB_SN_HAS_GPS && FEB_SN_IS_FRONT */

/* ============================================================================
 * Fusion AHRS — FRONT-only trait structs on this branch (same reason as GPS).
 * ============================================================================ */
#if FEB_SN_HAS_FUSION && FEB_SN_IS_FRONT()

bool fill_fusion_quat(feb_sn_fusion_quat_t &m)
{
  float q[4];
  FEB_Fusion_GetQuaternion(q);
  m.q_w = feb_sn_fusion_quat_w_encode((double)q[0]);
  m.q_x = feb_sn_fusion_quat_x_encode((double)q[1]);
  m.q_y = feb_sn_fusion_quat_y_encode((double)q[2]);
  m.q_z = feb_sn_fusion_quat_z_encode((double)q[3]);
  return true;
}
fc::Publisher<fm::FusionQuaternionData> fusion_quat_tx{fill_fusion_quat};

bool fill_fusion_euler(feb_sn_fusion_euler_t &m)
{
  float e[3];
  FEB_Fusion_GetEuler(e);
  m.roll = feb_sn_fusion_euler_roll_encode((double)e[0]);
  m.pitch = feb_sn_fusion_euler_pitch_encode((double)e[1]);
  m.yaw = feb_sn_fusion_euler_yaw_encode((double)e[2]);
  return true;
}
fc::Publisher<fm::FusionEulerData> fusion_euler_tx{fill_fusion_euler};

bool fill_fusion_lin_accel(feb_sn_fusion_lin_accel_t &m)
{
  float a[3];
  FEB_Fusion_GetLinearAcceleration_mg(a);
  m.lin_accel_x = feb_sn_fusion_lin_accel_x_encode((double)a[0]);
  m.lin_accel_y = feb_sn_fusion_lin_accel_y_encode((double)a[1]);
  m.lin_accel_z = feb_sn_fusion_lin_accel_z_encode((double)a[2]);
  return true;
}
fc::Publisher<fm::FusionLinearAccelData> fusion_lin_tx{fill_fusion_lin_accel};

bool fill_fusion_earth_accel(feb_sn_fusion_earth_accel_t &m)
{
  float a[3];
  FEB_Fusion_GetEarthAcceleration_mg(a);
  m.earth_accel_x = feb_sn_fusion_earth_accel_x_encode((double)a[0]);
  m.earth_accel_y = feb_sn_fusion_earth_accel_y_encode((double)a[1]);
  m.earth_accel_z = feb_sn_fusion_earth_accel_z_encode((double)a[2]);
  return true;
}
fc::Publisher<fm::FusionEarthAccelData> fusion_earth_tx{fill_fusion_earth_accel};

bool fill_fusion_status(feb_sn_fusion_status_t &m)
{
  FusionAhrsFlags flags;
  FusionAhrsInternalStates states;
  FEB_Fusion_GetFlags(&flags);
  FEB_Fusion_GetInternalStates(&states);

  uint8_t fb = 0;
  if (flags.startup)
    fb |= (1u << 0);
  if (flags.angularRateRecovery)
    fb |= (1u << 1);
  if (flags.accelerationRecovery)
    fb |= (1u << 2);
  if (flags.magneticRecovery)
    fb |= (1u << 3);
  if (states.accelerometerIgnored)
    fb |= (1u << 4);
  if (states.magnetometerIgnored)
    fb |= (1u << 5);

  m.flags = feb_sn_fusion_status_flags_encode((double)fb);
  m.accel_error = feb_sn_fusion_status_accel_err_encode(states.accelerationError);
  m.mag_error = feb_sn_fusion_status_mag_err_encode(states.magneticError);
  return true;
}
fc::Publisher<fm::FusionStatusData> fusion_status_tx{fill_fusion_status};

#endif /* FEB_SN_HAS_FUSION && FEB_SN_IS_FRONT */

} // namespace
