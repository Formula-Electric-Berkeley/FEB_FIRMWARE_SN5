/**
 ******************************************************************************
 * @file           : FEB_CAN_IMU.c
 * @brief          : CAN IMU Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_IMU.h"
#include "feb_can_lib.h"
#include <stddef.h>
#include <string.h>
#include "feb_can.h"
#include "FEB_IMU.h"

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_IMU_Init(void) {}

/**
 * Assemble and transmit IMU acceleration and gyroscope measurements over CAN.
 *
 * Packs raw IMU data into two 6-byte CAN frames:
 * - Acceleration frame (ID: FEB_CAN_IMU_ACCELERATION_DATA_FRAME_ID):
 *   - accel_raw[0] → bytes 0–1 (X-axis, int16_t)
 *   - accel_raw[1] → bytes 2–3 (Y-axis, int16_t)
 *   - accel_raw[2] → bytes 4–5 (Z-axis, int16_t)
 *
 * - Gyroscope frame (ID: FEB_CAN_IMU_GYRO_DATA_FRAME_ID):
 *   - gyro_raw[0] → bytes 0–1 (X-axis, int16_t)
 *   - gyro_raw[1] → bytes 2–3 (Y-axis, int16_t)
 *   - gyro_raw[2] → bytes 4–5 (Z-axis, int16_t)
 *
 * @param accel_raw Pointer to data_raw_acceleration[3] from FEB_IMU.c
 * @param gyro_raw  Pointer to data_raw_angular_rate[3] from FEB_IMU.c
 */
/* Error counter for throttled error reporting */
static uint32_t can_tx_error_count = 0;

void FEB_CAN_IMU_Tick(void)
{
  uint8_t tx_data[6] = {0};
  memcpy(&tx_data[0], &data_raw_acceleration[0], sizeof(int16_t));
  memcpy(&tx_data[2], &data_raw_acceleration[1], sizeof(int16_t));
  memcpy(&tx_data[4], &data_raw_acceleration[2], sizeof(int16_t));

  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_IMU_ACCELERATION_DATA_FRAME_ID, FEB_CAN_ID_STD,
                                            tx_data, FEB_CAN_IMU_ACCELERATION_DATA_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }

  memset(tx_data, 0, sizeof(tx_data));
  memcpy(&tx_data[0], &data_raw_angular_rate[0], sizeof(int16_t));
  memcpy(&tx_data[2], &data_raw_angular_rate[1], sizeof(int16_t));
  memcpy(&tx_data[4], &data_raw_angular_rate[2], sizeof(int16_t));

  status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_IMU_GYRO_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                           FEB_CAN_IMU_GYRO_DATA_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}
