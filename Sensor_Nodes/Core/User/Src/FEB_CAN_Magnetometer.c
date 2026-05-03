/**
 ******************************************************************************
 * @file           : FEB_CAN_Magnetometer.c
 * @brief          : CAN reporter for LIS3MDL magnetometer (mG → 0.5844 mG/LSB).
 *                   Variant-agnostic via FEB_SN_Config.h
 *                   (FRONT 0x2A, REAR 0x2B). No-op if FEB_SN_HAS_MAG=0.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_Magnetometer.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_Magnetometer.h"
#include "FEB_SN_Config.h"
#include <stdint.h>

static uint32_t can_tx_error_count = 0;

void FEB_CAN_Magnetometer_Init(void) {}

void FEB_CAN_Magnetometer_Tick(void)
{
#if FEB_SN_HAS_MAG
  struct feb_sn_mag_t s = {
      .magnetometer_x = feb_sn_mag_x_encode((double)magnetic_mG[0]),
      .magnetometer_y = feb_sn_mag_y_encode((double)magnetic_mG[1]),
      .magnetometer_z = feb_sn_mag_z_encode((double)magnetic_mG[2]),
  };
  uint8_t buf[FEB_SN_MAG_LENGTH];
  feb_sn_mag_pack(buf, &s, sizeof(buf));
  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_SN_MAG_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf)) != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
#endif
}
