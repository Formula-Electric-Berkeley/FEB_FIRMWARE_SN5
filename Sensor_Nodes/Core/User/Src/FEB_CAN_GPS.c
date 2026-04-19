/**
 ******************************************************************************
 * @file           : FEB_CAN_GPS.c
 * @brief          : CAN GPS Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_GPS.h"
#include "feb_can_lib.h"
#include <stddef.h>
#include <string.h>
#include "feb_can.h"
#include "FEB_GPS.h"

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_GPS_Init(void) {}

/**
 * Assemble and transmit GPS latitude and longitude measurements over CAN.
 *
 * Packs raw GPS data into an 8-byte CAN frame with the following structure (ID: FEB_CAN_GPS_LATLON_DATA_FRAME_ID):
 * - Latitude (ID: FEB_CAN_GPS_LATLON_DATA_FRAME_ID):
 *   - latitude → bytes 0–1 (int16_t)
 *   - longitude → bytes 2–3 (int16_t)
 */
/* Error counter for throttled error reporting */
static uint32_t can_tx_error_count = 0;

void FEB_CAN_GPS_Tick(void)
{
  FEB_GPS_Data_t gps_data;
  bool updated = FEB_GPS_GetLatestData(&gps_data);
  if (!updated || !gps_data.has_fix)
  {
    return;
  }

  //   LAT LON POSITIONAL DATA

  struct feb_can_gps_latlong_data_t frame = {
      .latitude = gps_data.latitude,
      .longitude = gps_data.longitude,
  };

  uint8_t tx_data[FEB_CAN_GPS_LATLON_DATA_LENGTH];
  int packed = feb_can_gps_latlong_data_pack(tx_data, &frame, sizeof(tx_data));
  if (packed < 0)
  {
    can_tx_error_count++;
    return;
  }

  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_LATLON_DATA_FRAME_ID, FEB_CAN_ID_STD,
                                            tx_data, FEB_CAN_GPS_LATLON_DATA_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }

  // MOTION DATA (SPEED AND COURSE)
  struct feb_can_gps_motion_data_t motion = {
      .speed = feb_can_gps_motion_data_speed_encode(gps_data.speed_kmh),
      .course = feb_can_gps_motion_data_course_encode(gps_data.course),
  };
  uint8_t tx_data[FEB_CAN_GPS_MOTION_DATA_LENGTH];
  int packed = feb_can_gps_motion_data_pack(tx_data, &motion, sizeof(tx_data));
  if (packed < 0)
  {
    can_tx_error_count++;
    return;
  }
  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_MOTION_DATA_FRAME_ID, FEB_CAN_ID_STD,
                                            tx_data, FEB_CAN_GPS_MOTION_DATA_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }

  //   memset(tx_data, 0, sizeof(tx_data));
  //   memcpy(&tx_data[0], &data_raw_angular_rate[0], sizeof(int16_t));
  //   memcpy(&tx_data[2], &data_raw_angular_rate[1], sizeof(int16_t));
  //   memcpy(&tx_data[4], &data_raw_angular_rate[2], sizeof(int16_t));

  //   status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_IMU_GYRO_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
  //                            FEB_CAN_IMU_GYRO_DATA_LENGTH);
  //   if (status != FEB_CAN_OK)
  //   {
  //     can_tx_error_count++;
  //   }
}
