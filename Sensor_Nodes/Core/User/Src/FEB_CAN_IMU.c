/**
 ******************************************************************************
 * @file           : FEB_CAN_IMU.c
 * @brief          : CAN reporter for LSM6DSOX accelerometer + gyroscope.
 *                   Variant-agnostic: frame IDs and pack helpers come from
 *                   FEB_SN_Config.h (FRONT 0x26/0x28, REAR 0x27/0x29).
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_IMU.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_IMU.h"
#include "FEB_SN_Config.h"
#include <stdint.h>

static uint32_t can_tx_error_count = 0;

void FEB_CAN_IMU_Init(void) {}

void FEB_CAN_IMU_Tick(void)
{
#if FEB_SN_HAS_IMU
  /* ---------------- Acceleration (FRONT 0x26 / REAR 0x27) ---------------- */
  {
    struct feb_sn_imu_accel_t s = {
        .acceleration_x = feb_sn_imu_accel_x_encode((double)acceleration_mg[0]),
        .acceleration_y = feb_sn_imu_accel_y_encode((double)acceleration_mg[1]),
        .acceleration_z = feb_sn_imu_accel_z_encode((double)acceleration_mg[2]),
    };
    uint8_t buf[FEB_SN_IMU_ACCEL_LENGTH];
    feb_sn_imu_accel_pack(buf, &s, sizeof(buf));
    if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_SN_IMU_ACCEL_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf)) != FEB_CAN_OK)
    {
      can_tx_error_count++;
    }
  }

  /* ---------------- Gyroscope (FRONT 0x28 / REAR 0x29) ---------------- */
  {
    struct feb_sn_imu_gyro_t s = {
        .gyro_x = feb_sn_imu_gyro_x_encode((double)angular_rate_mdps[0]),
        .gyro_y = feb_sn_imu_gyro_y_encode((double)angular_rate_mdps[1]),
        .gyro_z = feb_sn_imu_gyro_z_encode((double)angular_rate_mdps[2]),
    };
    uint8_t buf[FEB_SN_IMU_GYRO_LENGTH];
    feb_sn_imu_gyro_pack(buf, &s, sizeof(buf));
    if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_SN_IMU_GYRO_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf)) != FEB_CAN_OK)
    {
      can_tx_error_count++;
    }
  }
#endif
}
