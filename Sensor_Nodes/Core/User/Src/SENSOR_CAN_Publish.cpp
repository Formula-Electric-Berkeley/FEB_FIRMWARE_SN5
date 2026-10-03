/**
 ******************************************************************************
 * @file           : SENSOR_CAN_Publish.cpp
 * @brief          : Everything the Sensor Node transmits (C++ publishers)
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_SN_Config.h"           /* FEB_SN_HAS_* */
#include "FEB_IMU.h"                 /* data_raw_acceleration[], data_raw_angular_rate[], imu_temp_c */
#include "FEB_Magnetometer.h"        /* data_raw_magnetometer[], mag_temp_c */
#include "FEB_WSS.h"                 /* left_mph_x100, right_mph_x100, left_dir, right_dir */
#include "FEB_GPS.h"                 /* FEB_GPS_Data_t, FEB_GPS_GetLatestData() */
#include "FEB_Fusion.h"              /* FEB_Fusion_GetQuaternion/Euler/Linear/... */
#include "feb_can_publisher.hpp"     /* fc::Publisher<M> */
#include "FEB_LinearPotentiometer.h" /* lp_position_mm[], FEB_LP_COUNT */
#include "FEB_Thermistor.h"          /* th_temp_c[], th_status */
#include "SN_Config_Messages.hpp"
#include "FEB_StrainGauge.h"
#include "App/SN_SteeringEncoder.h"

namespace fc = feb::can;
namespace sm = feb::sn::msg;

namespace
{

/* ============================================================================
 * IMU (LSM6DSOX)
 * ============================================================================ */
#if FEB_SN_HAS_IMU

bool fill_imu_accel(sm::ImuAccel::Data &m)
{
  m.acceleration_x = data_raw_acceleration[0];
  m.acceleration_y = data_raw_acceleration[1];
  m.acceleration_z = data_raw_acceleration[2];
  m.imu_temp = (int16_t)(imu_temp_c * 100.0f);
  return true;
}
fc::Publisher<sm::ImuAccel> imu_accel_tx{fill_imu_accel};

bool fill_imu_gyro(sm::ImuGyro::Data &m)
{
  m.gyro_x = data_raw_angular_rate[0];
  m.gyro_y = data_raw_angular_rate[1];
  m.gyro_z = data_raw_angular_rate[2];
  return true;
}
fc::Publisher<sm::ImuGyro> imu_gyro_tx{fill_imu_gyro};

#endif /* FEB_SN_HAS_IMU */

/* ============================================================================
 * Magnetometer (LIS3MDL)
 * ============================================================================ */
#if FEB_SN_HAS_MAG

/* Raw LSBs match the DBC scale: 0.5844 mG (±16 G). */
bool fill_mag(sm::Mag::Data &m)
{
  m.magnetometer_x = data_raw_magnetometer[0];
  m.magnetometer_y = data_raw_magnetometer[1];
  m.magnetometer_z = data_raw_magnetometer[2];
  m.mag_temp = (int16_t)(mag_temp_c * 100.0f);
  return true;
}
fc::Publisher<sm::Mag> mag_tx{fill_mag};

#endif /* FEB_SN_HAS_MAG */

/* ============================================================================
 * Wheel speed sensors
 * ============================================================================ */
#if FEB_SN_HAS_WSS

bool fill_wss(sm::Wss::Data &m)
{
  m.wss_left = left_mph_x100;
  m.wss_right = right_mph_x100;

  uint8_t flags = 0;
  if (left_dir < 0)
    flags |= (1u << 0);
  if (right_dir < 0)
    flags |= (1u << 1);
  m.wss_dir_flags = flags;
  return true;
}

fc::Publisher<sm::Wss> wss_tx{fill_wss};

#endif /* FEB_SN_HAS_WSS */

/* ============================================================================
 * Linear potentiometer
 * ============================================================================ */
#if FEB_SN_HAS_LINEAR_POTENTIOMETER

static uint16_t mm_to_can_units(float mm)
{
  float scaled = mm * 100.0f;
  if (scaled < 0.0f)
    return 0u;
  if (scaled > 65535.0f)
    return 65535u;
  return (uint16_t)scaled;
}

bool fill_linpot(sm::Linpot::Data &m)
{
  m.linpot_left = mm_to_can_units(get_LP_Position(0));
  m.linpot_right = mm_to_can_units(get_LP_Position(1));
  return true;
}
fc::Publisher<sm::Linpot> linpot_tx{fill_linpot};

#endif /* FEB_SN_HAS_LINEAR_POTENTIOMETER */

/* ============================================================================
 * Thermistors
 * ============================================================================ */
#if FEB_SN_HAS_THERMISTOR

static int16_t temp_c_to_can_units(float temp_c)
{
  float scaled = temp_c * 100.0f;
  if (scaled < -32768.0f)
    return INT16_MIN;
  if (scaled > 32767.0f)
    return INT16_MAX;
  return (int16_t)scaled;
}

bool fill_thermistor(sm::Thermistor::Data &m)
{
  m.coolant_temp_1 = temp_c_to_can_units(th_temp_c[0]);
  m.coolant_temp_2 = temp_c_to_can_units(th_temp_c[1]);
  m.coolant_temp_3 = temp_c_to_can_units(th_temp_c[2]);
  m.therm_status = th_status;
  return true;
}
fc::Publisher<sm::Thermistor> thermistor_tx{fill_thermistor};

#endif /* FEB_SN_HAS_THERMISTOR */

/* ============================================================================
 * Strain Gauge
 * ============================================================================ */

#if FEB_SN_IS_FRONT()
// front has 3 strain gauges and rear has 4
bool fill_strain_gauge(sm::StrainGauge::Data &m)
{
  m.strain_gauge_1 = get_strain_gauge(0);
  m.strain_gauge_2 = get_strain_gauge(1);
  m.strain_gauge_3 = get_strain_gauge(2);
  return true;
}
#else
bool fill_strain_gauge(sm::StrainGauge::Data &m)
{
  m.strain_gauge_1 = get_strain_gauge(0);
  m.strain_gauge_2 = get_strain_gauge(1);
  m.strain_gauge_3 = get_strain_gauge(2);
  m.strain_gauge_4 = get_strain_gauge(3);
  return true;
}

fc::Publisher<sm::StrainGauge> strain_gauge_tx{fill_strain_gauge};

#endif

/* ============================================================================
 * Steering Encoder
 * ============================================================================ */
#if FEB_SN_IS_FRONT()

bool fill_steering_status(feb_can_steer_front_t &m)
{
  if (!steer_initialized)
    return false;

  m.angle = steer_angle;
  m.raw_angle = steer_raw_angle;
  m.agc = steer_agc;
  m.status = steer_status;
  m.magnitude = steer_magnitude;

  return true;
}

fc::Publisher<feb::can::msg::SteerFront> steer_front_tx{fill_steering_status};

#endif

/* ============================================================================
 * GPS
 * ============================================================================ */
#if FEB_SN_HAS_GPS

bool fill_gps_pos(sm::GpsPos::Data &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.latitude = (int32_t)(g.latitude * 1e7);
  m.longitude = (int32_t)(g.longitude * 1e7);
  return true;
}
fc::Publisher<sm::GpsPos> gps_pos_tx{fill_gps_pos};

bool fill_gps_altitude(sm::GpsAltitude::Data &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.altitude = (int32_t)(g.altitude * 100.0f);
  m.hdop = (uint16_t)(g.hdop * 100.0f);
  m.vdop = (uint16_t)(g.vdop * 100.0f);
  return true;
}
fc::Publisher<sm::GpsAltitude> gps_alt_tx{fill_gps_altitude};

bool fill_gps_motion(sm::GpsMotion::Data &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.speed = (uint16_t)(g.speed_kmh * 100.0f);
  m.course = (uint16_t)(g.course * 100.0f);
  return true;
}
fc::Publisher<sm::GpsMotion> gps_motion_tx{fill_gps_motion};

bool fill_gps_time(sm::GpsTime::Data &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.hours = (int8_t)g.hours;
  m.minutes = (int8_t)g.minutes;
  m.seconds = (int8_t)g.seconds;
  return true;
}
fc::Publisher<sm::GpsTime> gps_time_tx{fill_gps_time};

bool fill_gps_date(sm::GpsDate::Data &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.day = (int8_t)g.day;
  m.month = (int8_t)g.month;
  m.year = (int8_t)g.year;
  return true;
}
fc::Publisher<sm::GpsDate> gps_date_tx{fill_gps_date};

bool fill_gps_status(sm::GpsStatus::Data &m)
{
  FEB_GPS_Data_t g;
  FEB_GPS_GetLatestData(&g);
  m.fix_type = g.fix;
  m.fix_mode = g.fix_mode;
  m.sats_in_use = g.sats_in_use;
  m.sats_in_view = g.sats_in_view;
  m.valid = g.valid;
  m.has_fix = g.has_fix;
  m.pdop = (uint16_t)(g.pdop * 100.0f);
  return true;
}
fc::Publisher<sm::GpsStatus> gps_status_tx{fill_gps_status};

#endif /* FEB_SN_HAS_GPS */

/* ============================================================================
 * Fusion AHRS
 * ============================================================================ */
#if FEB_SN_HAS_FUSION

bool fill_fusion_quat(sm::FusionQuat::Data &m)
{
  float q[4];
  FEB_Fusion_GetQuaternion(q);
  m.q_w = (int16_t)(q[0] * 32767.0f);
  m.q_x = (int16_t)(q[1] * 32767.0f);
  m.q_y = (int16_t)(q[2] * 32767.0f);
  m.q_z = (int16_t)(q[3] * 32767.0f);
  return true;
}
fc::Publisher<sm::FusionQuat> fusion_quat_tx{fill_fusion_quat};

bool fill_fusion_euler(sm::FusionEuler::Data &m)
{
  float e[3];
  FEB_Fusion_GetEuler(e);
  m.roll = (int16_t)(e[0] * 100.0f);
  m.pitch = (int16_t)(e[1] * 100.0f);
  m.yaw = (int16_t)(e[2] * 100.0f);
  return true;
}
fc::Publisher<sm::FusionEuler> fusion_euler_tx{fill_fusion_euler};

bool fill_fusion_lin_accel(sm::FusionLinAccel::Data &m)
{
  float a[3];
  FEB_Fusion_GetLinearAcceleration_mg(a);
  m.lin_accel_x = (int16_t)a[0];
  m.lin_accel_y = (int16_t)a[1];
  m.lin_accel_z = (int16_t)a[2];
  return true;
}
fc::Publisher<sm::FusionLinAccel> fusion_lin_tx{fill_fusion_lin_accel};

bool fill_fusion_earth_accel(sm::FusionEarthAccel::Data &m)
{
  float a[3];
  FEB_Fusion_GetEarthAcceleration_mg(a);
  m.earth_accel_x = (int16_t)a[0];
  m.earth_accel_y = (int16_t)a[1];
  m.earth_accel_z = (int16_t)a[2];
  return true;
}
fc::Publisher<sm::FusionEarthAccel> fusion_earth_tx{fill_fusion_earth_accel};

bool fill_fusion_status(sm::FusionStatus::Data &m)
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

  m.flags = fb;
  m.accel_error = (uint8_t)(states.accelerationError * 10.0f);
  m.mag_error = (uint8_t)(states.magneticError * 10.0f);
  return true;
}
fc::Publisher<sm::FusionStatus> fusion_status_tx{fill_fusion_status};

#endif /* FEB_SN_HAS_FUSION */

} // namespace
