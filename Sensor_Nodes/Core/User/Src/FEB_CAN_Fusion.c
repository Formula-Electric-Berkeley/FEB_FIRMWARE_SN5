/**
 ******************************************************************************
 * @file           : FEB_CAN_Fusion.c
 * @brief          : CAN reporter for Fusion AHRS outputs. Variant-agnostic
 *                   via FEB_SN_Config.h. No-op if FEB_SN_HAS_FUSION=0.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_Fusion.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_Fusion.h"
#include "FEB_SN_Config.h"
#include "Fusion.h"
#include <stdint.h>

static uint32_t can_tx_error_count = 0;

#if FEB_SN_HAS_FUSION
static inline void tx_or_count(uint32_t frame_id, uint8_t *data, uint32_t len)
{
  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_id, FEB_CAN_ID_STD, data, len) != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}
#endif

void FEB_CAN_Fusion_Tick(void)
{
#if FEB_SN_HAS_FUSION
  /* ---------------- Quaternion (FRONT 0x47 / REAR 0x57) ---------------- */
  {
    float q[4];
    FEB_Fusion_GetQuaternion(q);
    struct feb_sn_fusion_quat_t s = {
        .q_w = feb_sn_fusion_quat_w_encode((double)q[0]),
        .q_x = feb_sn_fusion_quat_x_encode((double)q[1]),
        .q_y = feb_sn_fusion_quat_y_encode((double)q[2]),
        .q_z = feb_sn_fusion_quat_z_encode((double)q[3]),
    };
    uint8_t buf[FEB_SN_FUSION_QUAT_LENGTH];
    feb_sn_fusion_quat_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_FUSION_QUAT_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Euler (FRONT 0x48 / REAR 0x58) ---------------- */
  {
    float e[3];
    FEB_Fusion_GetEuler(e);
    struct feb_sn_fusion_euler_t s = {
        .roll = feb_sn_fusion_euler_roll_encode((double)e[0]),
        .pitch = feb_sn_fusion_euler_pitch_encode((double)e[1]),
        .yaw = feb_sn_fusion_euler_yaw_encode((double)e[2]),
    };
    uint8_t buf[FEB_SN_FUSION_EULER_LENGTH];
    feb_sn_fusion_euler_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_FUSION_EULER_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Linear acceleration body frame (FRONT 0x49 / REAR 0x59) ---------------- */
  {
    float a[3];
    FEB_Fusion_GetLinearAcceleration_mg(a);
    struct feb_sn_fusion_lin_accel_t s = {
        .lin_accel_x = feb_sn_fusion_lin_accel_x_encode((double)a[0]),
        .lin_accel_y = feb_sn_fusion_lin_accel_y_encode((double)a[1]),
        .lin_accel_z = feb_sn_fusion_lin_accel_z_encode((double)a[2]),
    };
    uint8_t buf[FEB_SN_FUSION_LIN_ACCEL_LENGTH];
    feb_sn_fusion_lin_accel_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_FUSION_LIN_ACCEL_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Earth-frame acceleration (FRONT 0x4A / REAR 0x5A) ---------------- */
  {
    float a[3];
    FEB_Fusion_GetEarthAcceleration_mg(a);
    struct feb_sn_fusion_earth_accel_t s = {
        .earth_accel_x = feb_sn_fusion_earth_accel_x_encode((double)a[0]),
        .earth_accel_y = feb_sn_fusion_earth_accel_y_encode((double)a[1]),
        .earth_accel_z = feb_sn_fusion_earth_accel_z_encode((double)a[2]),
    };
    uint8_t buf[FEB_SN_FUSION_EARTH_ACCEL_LENGTH];
    feb_sn_fusion_earth_accel_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_FUSION_EARTH_ACCEL_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Status flags + rejection errors (FRONT 0x4B / REAR 0x5B) ---------------- */
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

    struct feb_sn_fusion_status_t s = {
        .flags = feb_sn_fusion_status_flags_encode((double)flag_byte),
        .accel_error = feb_sn_fusion_status_accel_err_encode((double)states.accelerationError),
        .mag_error = feb_sn_fusion_status_mag_err_encode((double)states.magneticError),
    };
    uint8_t buf[FEB_SN_FUSION_STATUS_LENGTH];
    feb_sn_fusion_status_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_FUSION_STATUS_FRAME_ID, buf, sizeof(buf));
  }
#endif
}
