/**
 ******************************************************************************
 * @file           : FEB_SN_Config.h
 * @brief          : Sensor Node FRONT/REAR variant selector + CAN ID alias table.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * One firmware tree, two binaries: FRONT and REAR. Both variants run on
 * identical hardware; they differ only in which CAN frame IDs they publish.
 *
 * The build system supplies FEB_SENSOR_NODE_VARIANT (=FEB_SN_VARIANT_FRONT or
 * =FEB_SN_VARIANT_REAR). Reporters #include this header and use the FEB_SN_*
 * aliases below; they never reference the underlying feb_can_*_front/_rear
 * symbols directly. The single exception is FEB_CAN_WSS.c, which keeps one
 * #if FEB_SN_IS_FRONT() because the FRONT and REAR DBC layouts genuinely
 * differ (FRONT carries dir flags, REAR does not).
 *
 ******************************************************************************
 */

#ifndef FEB_SN_CONFIG_H
#define FEB_SN_CONFIG_H

#include "feb_can.h"

#define FEB_SN_VARIANT_FRONT 1
#define FEB_SN_VARIANT_REAR 2

#ifndef FEB_SENSOR_NODE_VARIANT
#error "FEB_SENSOR_NODE_VARIANT must be defined by the build system (FRONT or REAR)"
#endif
#if (FEB_SENSOR_NODE_VARIANT != FEB_SN_VARIANT_FRONT) && (FEB_SENSOR_NODE_VARIANT != FEB_SN_VARIANT_REAR)
#error "FEB_SENSOR_NODE_VARIANT must be FEB_SN_VARIANT_FRONT or FEB_SN_VARIANT_REAR"
#endif

#define FEB_SN_IS_FRONT() (FEB_SENSOR_NODE_VARIANT == FEB_SN_VARIANT_FRONT)
#define FEB_SN_IS_REAR() (FEB_SENSOR_NODE_VARIANT == FEB_SN_VARIANT_REAR)

extern const char FEB_SN_VARIANT_NAME[];

/* ============================================================================
 * Per-variant sensor population.
 *
 * FRONT and REAR boards may not be populated identically. Set each
 * FEB_SN_HAS_<sensor> to 1 if the corresponding device is present on the
 * physical board for that variant, 0 if it is absent. These flags gate:
 *   - sensor driver init calls in FEB_Main.c (no I2C/UART traffic to a
 *     missing chip — avoids spurious init-failure logs and bus stalls)
 *   - CAN reporter Tick bodies (no frames published for absent sensors)
 *
 * Defaults below assume all sensors are populated on both variants. Adjust
 * per the actual BOM. Feature gates are evaluated at compile time, so
 * disabled sensors carry zero flash/RAM cost.
 *
 * Fusion AHRS depends on IMU + magnetometer; if either is absent on a
 * variant, FEB_SN_HAS_FUSION must be 0. Sensor die temps come from the IMU
 * and magnetometer drivers, so FEB_SN_HAS_SENSOR_TEMPS requires both.
 * ============================================================================ */

#if FEB_SN_IS_FRONT()
#define FEB_SN_HAS_IMU 1
#define FEB_SN_HAS_MAG 1
#define FEB_SN_HAS_GPS 1
#define FEB_SN_HAS_WSS 1
#define FEB_SN_HAS_FUSION 1
#define FEB_SN_HAS_SENSOR_TEMPS 1
#else /* REAR */
#define FEB_SN_HAS_IMU 1
#define FEB_SN_HAS_MAG 1
#define FEB_SN_HAS_GPS 1
#define FEB_SN_HAS_WSS 1
#define FEB_SN_HAS_FUSION 1
#define FEB_SN_HAS_SENSOR_TEMPS 1
#endif

/* Consistency checks: composite features require their primitives. */
#if FEB_SN_HAS_FUSION && (!FEB_SN_HAS_IMU || !FEB_SN_HAS_MAG)
#error "FEB_SN_HAS_FUSION requires both FEB_SN_HAS_IMU and FEB_SN_HAS_MAG"
#endif
#if FEB_SN_HAS_SENSOR_TEMPS && (!FEB_SN_HAS_IMU || !FEB_SN_HAS_MAG)
#error "FEB_SN_HAS_SENSOR_TEMPS requires both FEB_SN_HAS_IMU and FEB_SN_HAS_MAG"
#endif

/* ============================================================================
 * Per-variant CAN frame ID + struct + pack-fn aliases.
 *
 * Reporters use only the FEB_SN_* names below. To add a new sensor: extend
 * sensor_nodes_messages.py with FRONT/REAR functions, register both IDs in
 * generate.py, then add a matching alias block here.
 * ============================================================================ */

#if FEB_SN_IS_FRONT()

/* ---------------- IMU acceleration (0x26 FRONT / 0x27 REAR) ---------------- */
#define FEB_SN_IMU_ACCEL_FRAME_ID FEB_CAN_IMU_ACCELERATION_DATA_FRAME_ID
#define FEB_SN_IMU_ACCEL_LENGTH FEB_CAN_IMU_ACCELERATION_DATA_LENGTH
#define feb_sn_imu_accel_t feb_can_imu_acceleration_data_t
#define feb_sn_imu_accel_pack feb_can_imu_acceleration_data_pack
#define feb_sn_imu_accel_x_encode feb_can_imu_acceleration_data_acceleration_x_encode
#define feb_sn_imu_accel_y_encode feb_can_imu_acceleration_data_acceleration_y_encode
#define feb_sn_imu_accel_z_encode feb_can_imu_acceleration_data_acceleration_z_encode

/* ---------------- IMU gyro (0x28 FRONT / 0x29 REAR) ---------------- */
#define FEB_SN_IMU_GYRO_FRAME_ID FEB_CAN_IMU_GYRO_DATA_FRAME_ID
#define FEB_SN_IMU_GYRO_LENGTH FEB_CAN_IMU_GYRO_DATA_LENGTH
#define feb_sn_imu_gyro_t feb_can_imu_gyro_data_t
#define feb_sn_imu_gyro_pack feb_can_imu_gyro_data_pack
#define feb_sn_imu_gyro_x_encode feb_can_imu_gyro_data_gyro_x_encode
#define feb_sn_imu_gyro_y_encode feb_can_imu_gyro_data_gyro_y_encode
#define feb_sn_imu_gyro_z_encode feb_can_imu_gyro_data_gyro_z_encode

/* ---------------- Magnetometer (0x2A FRONT / 0x2B REAR) ---------------- */
#define FEB_SN_MAG_FRAME_ID FEB_CAN_MAGNETOMETER_DATA_FRAME_ID
#define FEB_SN_MAG_LENGTH FEB_CAN_MAGNETOMETER_DATA_LENGTH
#define feb_sn_mag_t feb_can_magnetometer_data_t
#define feb_sn_mag_pack feb_can_magnetometer_data_pack
#define feb_sn_mag_x_encode feb_can_magnetometer_data_magnetometer_x_encode
#define feb_sn_mag_y_encode feb_can_magnetometer_data_magnetometer_y_encode
#define feb_sn_mag_z_encode feb_can_magnetometer_data_magnetometer_z_encode

/* ---------------- WSS (0x24 FRONT / 0x25 REAR) - layouts differ ---------------- */
#define FEB_SN_WSS_FRAME_ID FEB_CAN_WSS_FRONT_DATA_FRAME_ID
#define FEB_SN_WSS_LENGTH FEB_CAN_WSS_FRONT_DATA_LENGTH

/* ---------------- GPS pos (0x40 FRONT / 0x50 REAR) ---------------- */
#define FEB_SN_GPS_POS_FRAME_ID FEB_CAN_GPS_POS_DATA_FRAME_ID
#define FEB_SN_GPS_POS_LENGTH FEB_CAN_GPS_POS_DATA_LENGTH
#define feb_sn_gps_pos_t feb_can_gps_pos_data_t
#define feb_sn_gps_pos_pack feb_can_gps_pos_data_pack
#define feb_sn_gps_pos_latitude_encode feb_can_gps_pos_data_latitude_encode
#define feb_sn_gps_pos_longitude_encode feb_can_gps_pos_data_longitude_encode

/* ---------------- GPS altitude (0x41 FRONT / 0x51 REAR) ---------------- */
#define FEB_SN_GPS_ALTITUDE_FRAME_ID FEB_CAN_GPS_ALTITUDE_DATA_FRAME_ID
#define FEB_SN_GPS_ALTITUDE_LENGTH FEB_CAN_GPS_ALTITUDE_DATA_LENGTH
#define feb_sn_gps_altitude_t feb_can_gps_altitude_data_t
#define feb_sn_gps_altitude_pack feb_can_gps_altitude_data_pack
#define feb_sn_gps_altitude_altitude_encode feb_can_gps_altitude_data_altitude_encode
#define feb_sn_gps_altitude_hdop_encode feb_can_gps_altitude_data_hdop_encode
#define feb_sn_gps_altitude_vdop_encode feb_can_gps_altitude_data_vdop_encode

/* ---------------- GPS motion (0x42 FRONT / 0x52 REAR) ---------------- */
#define FEB_SN_GPS_MOTION_FRAME_ID FEB_CAN_GPS_MOTION_DATA_FRAME_ID
#define FEB_SN_GPS_MOTION_LENGTH FEB_CAN_GPS_MOTION_DATA_LENGTH
#define feb_sn_gps_motion_t feb_can_gps_motion_data_t
#define feb_sn_gps_motion_pack feb_can_gps_motion_data_pack
#define feb_sn_gps_motion_speed_encode feb_can_gps_motion_data_speed_encode
#define feb_sn_gps_motion_course_encode feb_can_gps_motion_data_course_encode

/* ---------------- GPS time (0x43 FRONT / 0x53 REAR) ---------------- */
#define FEB_SN_GPS_TIME_FRAME_ID FEB_CAN_GPS_TIME_DATA_FRAME_ID
#define FEB_SN_GPS_TIME_LENGTH FEB_CAN_GPS_TIME_DATA_LENGTH
#define feb_sn_gps_time_t feb_can_gps_time_data_t
#define feb_sn_gps_time_pack feb_can_gps_time_data_pack
#define feb_sn_gps_time_hours_encode feb_can_gps_time_data_hours_encode
#define feb_sn_gps_time_minutes_encode feb_can_gps_time_data_minutes_encode
#define feb_sn_gps_time_seconds_encode feb_can_gps_time_data_seconds_encode

/* ---------------- GPS date (0x44 FRONT / 0x54 REAR) ---------------- */
#define FEB_SN_GPS_DATE_FRAME_ID FEB_CAN_GPS_DATE_DATA_FRAME_ID
#define FEB_SN_GPS_DATE_LENGTH FEB_CAN_GPS_DATE_DATA_LENGTH
#define feb_sn_gps_date_t feb_can_gps_date_data_t
#define feb_sn_gps_date_pack feb_can_gps_date_data_pack
#define feb_sn_gps_date_day_encode feb_can_gps_date_data_day_encode
#define feb_sn_gps_date_month_encode feb_can_gps_date_data_month_encode
#define feb_sn_gps_date_year_encode feb_can_gps_date_data_year_encode

/* ---------------- GPS status (0x45 FRONT / 0x55 REAR) ---------------- */
#define FEB_SN_GPS_STATUS_FRAME_ID FEB_CAN_GPS_STATUS_DATA_FRAME_ID
#define FEB_SN_GPS_STATUS_LENGTH FEB_CAN_GPS_STATUS_DATA_LENGTH
#define feb_sn_gps_status_t feb_can_gps_status_data_t
#define feb_sn_gps_status_pack feb_can_gps_status_data_pack
#define feb_sn_gps_status_fix_type_encode feb_can_gps_status_data_fix_type_encode
#define feb_sn_gps_status_fix_mode_encode feb_can_gps_status_data_fix_mode_encode
#define feb_sn_gps_status_sats_in_use_encode feb_can_gps_status_data_sats_in_use_encode
#define feb_sn_gps_status_sats_in_view_encode feb_can_gps_status_data_sats_in_view_encode
#define feb_sn_gps_status_valid_encode feb_can_gps_status_data_valid_encode
#define feb_sn_gps_status_has_fix_encode feb_can_gps_status_data_has_fix_encode
#define feb_sn_gps_status_pdop_encode feb_can_gps_status_data_pdop_encode

/* ---------------- Fusion quaternion (0x47 FRONT / 0x57 REAR) ---------------- */
#define FEB_SN_FUSION_QUAT_FRAME_ID FEB_CAN_FUSION_QUATERNION_DATA_FRAME_ID
#define FEB_SN_FUSION_QUAT_LENGTH FEB_CAN_FUSION_QUATERNION_DATA_LENGTH
#define feb_sn_fusion_quat_t feb_can_fusion_quaternion_data_t
#define feb_sn_fusion_quat_pack feb_can_fusion_quaternion_data_pack
#define feb_sn_fusion_quat_w_encode feb_can_fusion_quaternion_data_q_w_encode
#define feb_sn_fusion_quat_x_encode feb_can_fusion_quaternion_data_q_x_encode
#define feb_sn_fusion_quat_y_encode feb_can_fusion_quaternion_data_q_y_encode
#define feb_sn_fusion_quat_z_encode feb_can_fusion_quaternion_data_q_z_encode

/* ---------------- Fusion Euler (0x48 FRONT / 0x58 REAR) ---------------- */
#define FEB_SN_FUSION_EULER_FRAME_ID FEB_CAN_FUSION_EULER_DATA_FRAME_ID
#define FEB_SN_FUSION_EULER_LENGTH FEB_CAN_FUSION_EULER_DATA_LENGTH
#define feb_sn_fusion_euler_t feb_can_fusion_euler_data_t
#define feb_sn_fusion_euler_pack feb_can_fusion_euler_data_pack
#define feb_sn_fusion_euler_roll_encode feb_can_fusion_euler_data_roll_encode
#define feb_sn_fusion_euler_pitch_encode feb_can_fusion_euler_data_pitch_encode
#define feb_sn_fusion_euler_yaw_encode feb_can_fusion_euler_data_yaw_encode

/* ---------------- Fusion linear accel (0x49 FRONT / 0x59 REAR) ---------------- */
#define FEB_SN_FUSION_LIN_ACCEL_FRAME_ID FEB_CAN_FUSION_LINEAR_ACCEL_DATA_FRAME_ID
#define FEB_SN_FUSION_LIN_ACCEL_LENGTH FEB_CAN_FUSION_LINEAR_ACCEL_DATA_LENGTH
#define feb_sn_fusion_lin_accel_t feb_can_fusion_linear_accel_data_t
#define feb_sn_fusion_lin_accel_pack feb_can_fusion_linear_accel_data_pack
#define feb_sn_fusion_lin_accel_x_encode feb_can_fusion_linear_accel_data_lin_accel_x_encode
#define feb_sn_fusion_lin_accel_y_encode feb_can_fusion_linear_accel_data_lin_accel_y_encode
#define feb_sn_fusion_lin_accel_z_encode feb_can_fusion_linear_accel_data_lin_accel_z_encode

/* ---------------- Fusion earth accel (0x4A FRONT / 0x5A REAR) ---------------- */
#define FEB_SN_FUSION_EARTH_ACCEL_FRAME_ID FEB_CAN_FUSION_EARTH_ACCEL_DATA_FRAME_ID
#define FEB_SN_FUSION_EARTH_ACCEL_LENGTH FEB_CAN_FUSION_EARTH_ACCEL_DATA_LENGTH
#define feb_sn_fusion_earth_accel_t feb_can_fusion_earth_accel_data_t
#define feb_sn_fusion_earth_accel_pack feb_can_fusion_earth_accel_data_pack
#define feb_sn_fusion_earth_accel_x_encode feb_can_fusion_earth_accel_data_earth_accel_x_encode
#define feb_sn_fusion_earth_accel_y_encode feb_can_fusion_earth_accel_data_earth_accel_y_encode
#define feb_sn_fusion_earth_accel_z_encode feb_can_fusion_earth_accel_data_earth_accel_z_encode

/* ---------------- Fusion status (0x4B FRONT / 0x5B REAR) ---------------- */
#define FEB_SN_FUSION_STATUS_FRAME_ID FEB_CAN_FUSION_STATUS_DATA_FRAME_ID
#define FEB_SN_FUSION_STATUS_LENGTH FEB_CAN_FUSION_STATUS_DATA_LENGTH
#define feb_sn_fusion_status_t feb_can_fusion_status_data_t
#define feb_sn_fusion_status_pack feb_can_fusion_status_data_pack
#define feb_sn_fusion_status_flags_encode feb_can_fusion_status_data_flags_encode
#define feb_sn_fusion_status_accel_err_encode feb_can_fusion_status_data_accel_error_encode
#define feb_sn_fusion_status_mag_err_encode feb_can_fusion_status_data_mag_error_encode

/* ---------------- Sensor temps (0x4C FRONT / 0x4D REAR) ---------------- */
#define FEB_SN_SENSOR_TEMPS_FRAME_ID FEB_CAN_SENSOR_TEMPS_DATA_FRAME_ID
#define FEB_SN_SENSOR_TEMPS_LENGTH FEB_CAN_SENSOR_TEMPS_DATA_LENGTH
#define feb_sn_sensor_temps_t feb_can_sensor_temps_data_t
#define feb_sn_sensor_temps_pack feb_can_sensor_temps_data_pack
#define feb_sn_sensor_temps_imu_encode feb_can_sensor_temps_data_imu_temp_encode
#define feb_sn_sensor_temps_mag_encode feb_can_sensor_temps_data_mag_temp_encode

#else /* REAR */

/* ---------------- IMU acceleration (REAR = 0x27) ---------------- */
#define FEB_SN_IMU_ACCEL_FRAME_ID FEB_CAN_IMU_ACCELERATION_DATA_REAR_FRAME_ID
#define FEB_SN_IMU_ACCEL_LENGTH FEB_CAN_IMU_ACCELERATION_DATA_REAR_LENGTH
#define feb_sn_imu_accel_t feb_can_imu_acceleration_data_rear_t
#define feb_sn_imu_accel_pack feb_can_imu_acceleration_data_rear_pack
#define feb_sn_imu_accel_x_encode feb_can_imu_acceleration_data_rear_acceleration_x_encode
#define feb_sn_imu_accel_y_encode feb_can_imu_acceleration_data_rear_acceleration_y_encode
#define feb_sn_imu_accel_z_encode feb_can_imu_acceleration_data_rear_acceleration_z_encode

/* ---------------- IMU gyro (REAR = 0x29) ---------------- */
#define FEB_SN_IMU_GYRO_FRAME_ID FEB_CAN_IMU_GYRO_DATA_REAR_FRAME_ID
#define FEB_SN_IMU_GYRO_LENGTH FEB_CAN_IMU_GYRO_DATA_REAR_LENGTH
#define feb_sn_imu_gyro_t feb_can_imu_gyro_data_rear_t
#define feb_sn_imu_gyro_pack feb_can_imu_gyro_data_rear_pack
#define feb_sn_imu_gyro_x_encode feb_can_imu_gyro_data_rear_gyro_x_encode
#define feb_sn_imu_gyro_y_encode feb_can_imu_gyro_data_rear_gyro_y_encode
#define feb_sn_imu_gyro_z_encode feb_can_imu_gyro_data_rear_gyro_z_encode

/* ---------------- Magnetometer (REAR = 0x2B) ---------------- */
#define FEB_SN_MAG_FRAME_ID FEB_CAN_MAGNETOMETER_DATA_REAR_FRAME_ID
#define FEB_SN_MAG_LENGTH FEB_CAN_MAGNETOMETER_DATA_REAR_LENGTH
#define feb_sn_mag_t feb_can_magnetometer_data_rear_t
#define feb_sn_mag_pack feb_can_magnetometer_data_rear_pack
#define feb_sn_mag_x_encode feb_can_magnetometer_data_rear_magnetometer_x_encode
#define feb_sn_mag_y_encode feb_can_magnetometer_data_rear_magnetometer_y_encode
#define feb_sn_mag_z_encode feb_can_magnetometer_data_rear_magnetometer_z_encode

/* ---------------- WSS (REAR = 0x25) - layouts differ; FEB_CAN_WSS.c branches ---------------- */
#define FEB_SN_WSS_FRAME_ID FEB_CAN_WSS_REAR_DATA_FRAME_ID
#define FEB_SN_WSS_LENGTH FEB_CAN_WSS_REAR_DATA_LENGTH

/* ---------------- GPS pos (REAR = 0x50) ---------------- */
#define FEB_SN_GPS_POS_FRAME_ID FEB_CAN_GPS_POS_DATA_REAR_FRAME_ID
#define FEB_SN_GPS_POS_LENGTH FEB_CAN_GPS_POS_DATA_REAR_LENGTH
#define feb_sn_gps_pos_t feb_can_gps_pos_data_rear_t
#define feb_sn_gps_pos_pack feb_can_gps_pos_data_rear_pack
#define feb_sn_gps_pos_latitude_encode feb_can_gps_pos_data_rear_latitude_encode
#define feb_sn_gps_pos_longitude_encode feb_can_gps_pos_data_rear_longitude_encode

/* ---------------- GPS altitude (REAR = 0x51) ---------------- */
#define FEB_SN_GPS_ALTITUDE_FRAME_ID FEB_CAN_GPS_ALTITUDE_DATA_REAR_FRAME_ID
#define FEB_SN_GPS_ALTITUDE_LENGTH FEB_CAN_GPS_ALTITUDE_DATA_REAR_LENGTH
#define feb_sn_gps_altitude_t feb_can_gps_altitude_data_rear_t
#define feb_sn_gps_altitude_pack feb_can_gps_altitude_data_rear_pack
#define feb_sn_gps_altitude_altitude_encode feb_can_gps_altitude_data_rear_altitude_encode
#define feb_sn_gps_altitude_hdop_encode feb_can_gps_altitude_data_rear_hdop_encode
#define feb_sn_gps_altitude_vdop_encode feb_can_gps_altitude_data_rear_vdop_encode

/* ---------------- GPS motion (REAR = 0x52) ---------------- */
#define FEB_SN_GPS_MOTION_FRAME_ID FEB_CAN_GPS_MOTION_DATA_REAR_FRAME_ID
#define FEB_SN_GPS_MOTION_LENGTH FEB_CAN_GPS_MOTION_DATA_REAR_LENGTH
#define feb_sn_gps_motion_t feb_can_gps_motion_data_rear_t
#define feb_sn_gps_motion_pack feb_can_gps_motion_data_rear_pack
#define feb_sn_gps_motion_speed_encode feb_can_gps_motion_data_rear_speed_encode
#define feb_sn_gps_motion_course_encode feb_can_gps_motion_data_rear_course_encode

/* ---------------- GPS time (REAR = 0x53) ---------------- */
#define FEB_SN_GPS_TIME_FRAME_ID FEB_CAN_GPS_TIME_DATA_REAR_FRAME_ID
#define FEB_SN_GPS_TIME_LENGTH FEB_CAN_GPS_TIME_DATA_REAR_LENGTH
#define feb_sn_gps_time_t feb_can_gps_time_data_rear_t
#define feb_sn_gps_time_pack feb_can_gps_time_data_rear_pack
#define feb_sn_gps_time_hours_encode feb_can_gps_time_data_rear_hours_encode
#define feb_sn_gps_time_minutes_encode feb_can_gps_time_data_rear_minutes_encode
#define feb_sn_gps_time_seconds_encode feb_can_gps_time_data_rear_seconds_encode

/* ---------------- GPS date (REAR = 0x54) ---------------- */
#define FEB_SN_GPS_DATE_FRAME_ID FEB_CAN_GPS_DATE_DATA_REAR_FRAME_ID
#define FEB_SN_GPS_DATE_LENGTH FEB_CAN_GPS_DATE_DATA_REAR_LENGTH
#define feb_sn_gps_date_t feb_can_gps_date_data_rear_t
#define feb_sn_gps_date_pack feb_can_gps_date_data_rear_pack
#define feb_sn_gps_date_day_encode feb_can_gps_date_data_rear_day_encode
#define feb_sn_gps_date_month_encode feb_can_gps_date_data_rear_month_encode
#define feb_sn_gps_date_year_encode feb_can_gps_date_data_rear_year_encode

/* ---------------- GPS status (REAR = 0x55) ---------------- */
#define FEB_SN_GPS_STATUS_FRAME_ID FEB_CAN_GPS_STATUS_DATA_REAR_FRAME_ID
#define FEB_SN_GPS_STATUS_LENGTH FEB_CAN_GPS_STATUS_DATA_REAR_LENGTH
#define feb_sn_gps_status_t feb_can_gps_status_data_rear_t
#define feb_sn_gps_status_pack feb_can_gps_status_data_rear_pack
#define feb_sn_gps_status_fix_type_encode feb_can_gps_status_data_rear_fix_type_encode
#define feb_sn_gps_status_fix_mode_encode feb_can_gps_status_data_rear_fix_mode_encode
#define feb_sn_gps_status_sats_in_use_encode feb_can_gps_status_data_rear_sats_in_use_encode
#define feb_sn_gps_status_sats_in_view_encode feb_can_gps_status_data_rear_sats_in_view_encode
#define feb_sn_gps_status_valid_encode feb_can_gps_status_data_rear_valid_encode
#define feb_sn_gps_status_has_fix_encode feb_can_gps_status_data_rear_has_fix_encode
#define feb_sn_gps_status_pdop_encode feb_can_gps_status_data_rear_pdop_encode

/* ---------------- Fusion quaternion (REAR = 0x57) ---------------- */
#define FEB_SN_FUSION_QUAT_FRAME_ID FEB_CAN_FUSION_QUATERNION_DATA_REAR_FRAME_ID
#define FEB_SN_FUSION_QUAT_LENGTH FEB_CAN_FUSION_QUATERNION_DATA_REAR_LENGTH
#define feb_sn_fusion_quat_t feb_can_fusion_quaternion_data_rear_t
#define feb_sn_fusion_quat_pack feb_can_fusion_quaternion_data_rear_pack
#define feb_sn_fusion_quat_w_encode feb_can_fusion_quaternion_data_rear_q_w_encode
#define feb_sn_fusion_quat_x_encode feb_can_fusion_quaternion_data_rear_q_x_encode
#define feb_sn_fusion_quat_y_encode feb_can_fusion_quaternion_data_rear_q_y_encode
#define feb_sn_fusion_quat_z_encode feb_can_fusion_quaternion_data_rear_q_z_encode

/* ---------------- Fusion Euler (REAR = 0x58) ---------------- */
#define FEB_SN_FUSION_EULER_FRAME_ID FEB_CAN_FUSION_EULER_DATA_REAR_FRAME_ID
#define FEB_SN_FUSION_EULER_LENGTH FEB_CAN_FUSION_EULER_DATA_REAR_LENGTH
#define feb_sn_fusion_euler_t feb_can_fusion_euler_data_rear_t
#define feb_sn_fusion_euler_pack feb_can_fusion_euler_data_rear_pack
#define feb_sn_fusion_euler_roll_encode feb_can_fusion_euler_data_rear_roll_encode
#define feb_sn_fusion_euler_pitch_encode feb_can_fusion_euler_data_rear_pitch_encode
#define feb_sn_fusion_euler_yaw_encode feb_can_fusion_euler_data_rear_yaw_encode

/* ---------------- Fusion linear accel (REAR = 0x59) ---------------- */
#define FEB_SN_FUSION_LIN_ACCEL_FRAME_ID FEB_CAN_FUSION_LINEAR_ACCEL_DATA_REAR_FRAME_ID
#define FEB_SN_FUSION_LIN_ACCEL_LENGTH FEB_CAN_FUSION_LINEAR_ACCEL_DATA_REAR_LENGTH
#define feb_sn_fusion_lin_accel_t feb_can_fusion_linear_accel_data_rear_t
#define feb_sn_fusion_lin_accel_pack feb_can_fusion_linear_accel_data_rear_pack
#define feb_sn_fusion_lin_accel_x_encode feb_can_fusion_linear_accel_data_rear_lin_accel_x_encode
#define feb_sn_fusion_lin_accel_y_encode feb_can_fusion_linear_accel_data_rear_lin_accel_y_encode
#define feb_sn_fusion_lin_accel_z_encode feb_can_fusion_linear_accel_data_rear_lin_accel_z_encode

/* ---------------- Fusion earth accel (REAR = 0x5A) ---------------- */
#define FEB_SN_FUSION_EARTH_ACCEL_FRAME_ID FEB_CAN_FUSION_EARTH_ACCEL_DATA_REAR_FRAME_ID
#define FEB_SN_FUSION_EARTH_ACCEL_LENGTH FEB_CAN_FUSION_EARTH_ACCEL_DATA_REAR_LENGTH
#define feb_sn_fusion_earth_accel_t feb_can_fusion_earth_accel_data_rear_t
#define feb_sn_fusion_earth_accel_pack feb_can_fusion_earth_accel_data_rear_pack
#define feb_sn_fusion_earth_accel_x_encode feb_can_fusion_earth_accel_data_rear_earth_accel_x_encode
#define feb_sn_fusion_earth_accel_y_encode feb_can_fusion_earth_accel_data_rear_earth_accel_y_encode
#define feb_sn_fusion_earth_accel_z_encode feb_can_fusion_earth_accel_data_rear_earth_accel_z_encode

/* ---------------- Fusion status (REAR = 0x5B) ---------------- */
#define FEB_SN_FUSION_STATUS_FRAME_ID FEB_CAN_FUSION_STATUS_DATA_REAR_FRAME_ID
#define FEB_SN_FUSION_STATUS_LENGTH FEB_CAN_FUSION_STATUS_DATA_REAR_LENGTH
#define feb_sn_fusion_status_t feb_can_fusion_status_data_rear_t
#define feb_sn_fusion_status_pack feb_can_fusion_status_data_rear_pack
#define feb_sn_fusion_status_flags_encode feb_can_fusion_status_data_rear_flags_encode
#define feb_sn_fusion_status_accel_err_encode feb_can_fusion_status_data_rear_accel_error_encode
#define feb_sn_fusion_status_mag_err_encode feb_can_fusion_status_data_rear_mag_error_encode

/* ---------------- Sensor temps (REAR = 0x4D) ---------------- */
#define FEB_SN_SENSOR_TEMPS_FRAME_ID FEB_CAN_SENSOR_TEMPS_DATA_REAR_FRAME_ID
#define FEB_SN_SENSOR_TEMPS_LENGTH FEB_CAN_SENSOR_TEMPS_DATA_REAR_LENGTH
#define feb_sn_sensor_temps_t feb_can_sensor_temps_data_rear_t
#define feb_sn_sensor_temps_pack feb_can_sensor_temps_data_rear_pack
#define feb_sn_sensor_temps_imu_encode feb_can_sensor_temps_data_rear_imu_temp_encode
#define feb_sn_sensor_temps_mag_encode feb_can_sensor_temps_data_rear_mag_temp_encode

#endif

#endif /* FEB_SN_CONFIG_H */
