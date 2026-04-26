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
#include "feb_console.h"
/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_GPS_Init(void) {}

/**
 * Assemble and transmit GPS latitude and longitude measurements over CAN.
 *
 * Packs raw GPS data into an 8-byte CAN frame with the following structure (ID: FEB_CAN_GPS_LATLON_DATA_FRAME_ID):
 * - Latitude (ID: FEB_CAN_GPS_LATLON_DATA_FRAME_ID):
 *   - latitude → bytes 0–1 (double)
 *   - longitude → bytes 2–3 (double)
 * - Speed and Course (ID: FEB_CAN_GPS_MOTION_DATA_FRAME_ID):
 *   - speed → bytes 0–1 (double)
 *   - course → bytes 2–3 (double)
 * - Time (ID: FEB_CAN_GPS_TIME_DATA_FRAME_ID):
 *   - hours → byte 0 (uint8_t)
 *   - minutes → byte 1 (uint8_t)
 *   - seconds → byte 2 (uint8_t)
 * - Date (ID: FEB_CAN_GPS_DATE_DATA_FRAME_ID):
 *   - day → byte 0 (uint8_t)
 *   - month → byte 1 (uint8_t)
 *   - year → byte 2 (uint8_t)
 * @param gps_data Pointer to latest GPS data from FEB_GPS.c
 */
/* Error counter for throttled error reporting */
static uint32_t can_tx_error_count = 0;

void FEB_CAN_GPS_Tick(void)
{
  FEB_GPS_Data_t gps_data;
  bool updated = FEB_GPS_GetLatestData(&gps_data);
  // if (!updated || !gps_data.has_fix)
  // {
  //   return;
  // }
  // FEB_Console_Printf("\r\nupdated: %d, has_fix: %d", updated, gps_data.has_fix);

  uint8_t tx_data[8];
  int packed;

  /* --- POSITION: gps_latlong_data (lat, long) --- */

  // uint16_t tx_data[4] = {0};
  uint16_t packed_lat = (int16_t)(gps_data.latitude);
  uint16_t packed_long = (int16_t)(gps_data.longitude);

  memcpy(&tx_data[0], &packed_lat, sizeof(uint16_t));
  memcpy(&tx_data[2], &packed_long, sizeof(uint16_t));

  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_POS_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                                            FEB_CAN_GPS_POS_DATA_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }

  // struct feb_can_gps_latlong_data_t latlong = {
  //     .latitude = feb_can_gps_latlong_data_latitude_encode(gps_data.latitude),
  //     .longitude = feb_can_gps_latlong_data_longitude_encode(gps_data.longitude),
  // };
  // packed = feb_can_gps_latlong_data_pack(tx_data, &latlong, sizeof(tx_data));
  // if (packed >= 0)
  //   FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_LATLONG_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
  //                   FEB_CAN_GPS_LATLONG_DATA_LENGTH);
  //     FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, 0x123, FEB_CAN_ID_STD, tx_data,
  //                   4);
  // else
  //   can_tx_error_count++;

  // // FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_LATLONG_DATA_FRAME_ID,
  // //    FEB_CAN_ID_STD, tx_data, FEB_CAN_GPS_LATLONG_DATA_LENGTH);
  // // FEB_Console_Printf("\r\nGPS LatLong CAN Tx: lat=%.6f, long=%.6f", latlong.latitude, latlong.longitude);
  // FEB_Console_Printf("\r\n packed latlon: %d", packed);

  // // /* --- MOTION: gps_motion_data (speed, course) --- */
  // struct feb_can_gps_motion_data_t motion = {
  //     .speed = feb_can_gps_motion_data_speed_encode(gps_data.speed_kmh),
  //     .course = feb_can_gps_motion_data_course_encode(gps_data.course),
  // };
  // packed = feb_can_gps_motion_data_pack(tx_data, &motion, sizeof(tx_data));
  // if (packed >= 0)
  //   FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_MOTION_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
  //                   FEB_CAN_GPS_MOTION_DATA_LENGTH);
  // else
  //   can_tx_error_count++;
  // FEB_Console_Printf("\r\n packed motion: %d", packed);
  // FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_MOTION_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
  //                 FEB_CAN_GPS_MOTION_DATA_LENGTH);

  // /* --- TIME: gps_time_data (hours, minutes, seconds) --- */
  // struct feb_can_gps_time_data_t time_data = {
  //     .hours = feb_can_gps_time_data_hours_encode((double)gps_data.hours),
  //     .minutes = feb_can_gps_time_data_minutes_encode((double)gps_data.minutes),
  //     .seconds = feb_can_gps_time_data_seconds_encode((double)gps_data.seconds),
  // };
  // packed = feb_can_gps_time_data_pack(tx_data, &time_data, sizeof(tx_data));
  // if (packed > 0)
  //   FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_TIME_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
  //   FEB_CAN_GPS_TIME_DATA_LENGTH);
  // else
  //   can_tx_error_count++;

  // /* --- DATE: gps_date_data (day, month, year) --- */
  // struct feb_can_gps_date_data_t date_data = {
  //     .day = feb_can_gps_date_data_day_encode((double)gps_data.day),
  //     .month = feb_can_gps_date_data_month_encode((double)gps_data.month),
  //     .year = feb_can_gps_date_data_year_encode((double)gps_data.year),
  // };
  // packed = feb_can_gps_date_data_pack(tx_data, &date_data, sizeof(tx_data));
  // if (packed > 0)
  //   FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_DATE_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
  //   FEB_CAN_GPS_DATE_DATA_LENGTH);
  // else
  //   can_tx_error_count++;
}
