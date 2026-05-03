/**
 ******************************************************************************
 * @file           : FEB_CAN_IMU.c
 * @brief          : CAN reporter for LSM6DSOX accelerometer + gyroscope.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Wire encoding mirrors driver output:
 *   acceleration_mg[] (mg) -> imu_acceleration_data (0.061 mg/LSB int16)
 *   angular_rate_mdps[] (mdps) -> imu_gyro_data (70 mdps/LSB int16)
 ******************************************************************************
 */

#include "FEB_CAN_IMU.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_IMU.h"
#include <stdint.h>

static uint32_t can_tx_error_count = 0;

void FEB_CAN_IMU_Init(void) {}

void FEB_CAN_IMU_Tick(void)
{
  /* ---------------- Acceleration (0x26) ---------------- */
  {
    struct feb_can_imu_acceleration_data_t s = {
        .acceleration_x = feb_can_imu_acceleration_data_acceleration_x_encode((double)acceleration_mg[0]),
        .acceleration_y = feb_can_imu_acceleration_data_acceleration_y_encode((double)acceleration_mg[1]),
        .acceleration_z = feb_can_imu_acceleration_data_acceleration_z_encode((double)acceleration_mg[2]),
    };
    uint8_t buf[FEB_CAN_IMU_ACCELERATION_DATA_LENGTH];
    feb_can_imu_acceleration_data_pack(buf, &s, sizeof(buf));
    if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_IMU_ACCELERATION_DATA_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf)) !=
        FEB_CAN_OK)
    {
      can_tx_error_count++;
    }
  }

  /* ---------------- Gyroscope (0x28) ---------------- */
  {
    struct feb_can_imu_gyro_data_t s = {
        .gyro_x = feb_can_imu_gyro_data_gyro_x_encode((double)angular_rate_mdps[0]),
        .gyro_y = feb_can_imu_gyro_data_gyro_y_encode((double)angular_rate_mdps[1]),
        .gyro_z = feb_can_imu_gyro_data_gyro_z_encode((double)angular_rate_mdps[2]),
    };
    uint8_t buf[FEB_CAN_IMU_GYRO_DATA_LENGTH];
    feb_can_imu_gyro_data_pack(buf, &s, sizeof(buf));
    if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_IMU_GYRO_DATA_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf)) !=
        FEB_CAN_OK)
    {
      can_tx_error_count++;
    }
  }
}
