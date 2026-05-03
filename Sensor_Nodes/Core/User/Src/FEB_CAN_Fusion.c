/**
 ******************************************************************************
 * @file           : FEB_CAN_Fusion.c
 * @brief          : CAN reporter for Fusion AHRS outputs.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_Fusion.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_Fusion.h"
#include "Fusion.h"
#include <stdint.h>

static uint32_t can_tx_error_count = 0;

static inline void tx_or_count(uint32_t frame_id, uint8_t *data, uint32_t len)
{
  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_id, FEB_CAN_ID_STD, data, len) != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}

void FEB_CAN_Fusion_Tick(void)
{
  /* ---------------- Quaternion (0x47) ---------------- */
  {
    float q[4];
    FEB_Fusion_GetQuaternion(q);
    struct feb_can_fusion_quaternion_data_t s = {
        .q_w = feb_can_fusion_quaternion_data_q_w_encode((double)q[0]),
        .q_x = feb_can_fusion_quaternion_data_q_x_encode((double)q[1]),
        .q_y = feb_can_fusion_quaternion_data_q_y_encode((double)q[2]),
        .q_z = feb_can_fusion_quaternion_data_q_z_encode((double)q[3]),
    };
    uint8_t buf[FEB_CAN_FUSION_QUATERNION_DATA_LENGTH];
    feb_can_fusion_quaternion_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_FUSION_QUATERNION_DATA_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Euler (0x48) ---------------- */
  {
    float e[3];
    FEB_Fusion_GetEuler(e);
    struct feb_can_fusion_euler_data_t s = {
        .roll = feb_can_fusion_euler_data_roll_encode((double)e[0]),
        .pitch = feb_can_fusion_euler_data_pitch_encode((double)e[1]),
        .yaw = feb_can_fusion_euler_data_yaw_encode((double)e[2]),
    };
    uint8_t buf[FEB_CAN_FUSION_EULER_DATA_LENGTH];
    feb_can_fusion_euler_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_FUSION_EULER_DATA_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Linear acceleration body frame (0x49) ---------------- */
  {
    float a[3];
    FEB_Fusion_GetLinearAcceleration_mg(a);
    struct feb_can_fusion_linear_accel_data_t s = {
        .lin_accel_x = feb_can_fusion_linear_accel_data_lin_accel_x_encode((double)a[0]),
        .lin_accel_y = feb_can_fusion_linear_accel_data_lin_accel_y_encode((double)a[1]),
        .lin_accel_z = feb_can_fusion_linear_accel_data_lin_accel_z_encode((double)a[2]),
    };
    uint8_t buf[FEB_CAN_FUSION_LINEAR_ACCEL_DATA_LENGTH];
    feb_can_fusion_linear_accel_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_FUSION_LINEAR_ACCEL_DATA_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Earth-frame acceleration (0x4A) ---------------- */
  {
    float a[3];
    FEB_Fusion_GetEarthAcceleration_mg(a);
    struct feb_can_fusion_earth_accel_data_t s = {
        .earth_accel_x = feb_can_fusion_earth_accel_data_earth_accel_x_encode((double)a[0]),
        .earth_accel_y = feb_can_fusion_earth_accel_data_earth_accel_y_encode((double)a[1]),
        .earth_accel_z = feb_can_fusion_earth_accel_data_earth_accel_z_encode((double)a[2]),
    };
    uint8_t buf[FEB_CAN_FUSION_EARTH_ACCEL_DATA_LENGTH];
    feb_can_fusion_earth_accel_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_FUSION_EARTH_ACCEL_DATA_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Status flags + rejection errors (0x4B) ---------------- */
  {
    FusionAhrsFlags flags;
    FusionAhrsInternalStates states;
    FEB_Fusion_GetFlags(&flags);
    FEB_Fusion_GetInternalStates(&states);

    uint8_t flag_byte = 0;
    if (flags.startup)
      flag_byte |= (1u << 0);
    if (flags.angularRateRecovery)
      flag_byte |= (1u << 1);
    if (flags.accelerationRecovery)
      flag_byte |= (1u << 2);
    if (flags.magneticRecovery)
      flag_byte |= (1u << 3);
    if (states.accelerometerIgnored)
      flag_byte |= (1u << 4);
    if (states.magnetometerIgnored)
      flag_byte |= (1u << 5);

    struct feb_can_fusion_status_data_t s = {
        .flags = feb_can_fusion_status_data_flags_encode((double)flag_byte),
        .accel_error = feb_can_fusion_status_data_accel_error_encode((double)states.accelerationError),
        .mag_error = feb_can_fusion_status_data_mag_error_encode((double)states.magneticError),
    };
    uint8_t buf[FEB_CAN_FUSION_STATUS_DATA_LENGTH];
    feb_can_fusion_status_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_FUSION_STATUS_DATA_FRAME_ID, buf, sizeof(buf));
  }
}
