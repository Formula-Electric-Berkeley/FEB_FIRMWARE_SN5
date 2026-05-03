/**
 ******************************************************************************
 * @file           : FEB_CAN_Sensors.c
 * @brief          : CAN reporter for misc sensor health.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_Sensors.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_IMU.h"
#include "FEB_Magnetometer.h"

static uint32_t can_tx_error_count = 0;

void FEB_CAN_Temps_Tick(void)
{
  struct feb_can_sensor_temps_data_t s = {
      .imu_temp = feb_can_sensor_temps_data_imu_temp_encode((double)imu_temp_c),
      .mag_temp = feb_can_sensor_temps_data_mag_temp_encode((double)mag_temp_c),
  };
  uint8_t buf[FEB_CAN_SENSOR_TEMPS_DATA_LENGTH];
  feb_can_sensor_temps_data_pack(buf, &s, sizeof(buf));
  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_SENSOR_TEMPS_DATA_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf)) !=
      FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}
