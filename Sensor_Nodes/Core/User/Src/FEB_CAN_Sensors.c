/**
 ******************************************************************************
 * @file           : FEB_CAN_Sensors.c
 * @brief          : CAN reporter for misc sensor health (die temps).
 *                   Variant-agnostic via FEB_SN_Config.h
 *                   (FRONT 0x4C / REAR 0x4D). No-op if FEB_SN_HAS_SENSOR_TEMPS=0.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_Sensors.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_IMU.h"
#include "FEB_Magnetometer.h"
#include "FEB_SN_Config.h"

static uint32_t can_tx_error_count = 0;

void FEB_CAN_Temps_Tick(void)
{
#if FEB_SN_HAS_SENSOR_TEMPS
  struct feb_sn_sensor_temps_t s = {
      .imu_temp = feb_sn_sensor_temps_imu_encode((double)imu_temp_c),
      .mag_temp = feb_sn_sensor_temps_mag_encode((double)mag_temp_c),
  };
  uint8_t buf[FEB_SN_SENSOR_TEMPS_LENGTH];
  feb_sn_sensor_temps_pack(buf, &s, sizeof(buf));
  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_SN_SENSOR_TEMPS_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf)) != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
#endif
}
