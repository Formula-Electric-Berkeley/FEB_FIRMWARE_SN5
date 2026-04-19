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

  uint8_t tx_buf[8];
  int packed;

  /* --- POSITION: gps_latlong_data (lat, long) --- */
  struct feb_can_gps_latlong_data_t latlong = {
      .latitude = feb_can_gps_latlong_data_latitude_encode(gps_data.latitude),
      .longitude = feb_can_gps_latlong_data_longitude_encode(gps_data.longitude),
  };
  packed = feb_can_gps_latlong_data_pack(tx_buf, &latlong, sizeof(tx_buf));
  if (packed > 0)
    send_frame(FEB_CAN_GPS_LATLONG_DATA_FRAME_ID, tx_buf, FEB_CAN_GPS_LATLONG_DATA_LENGTH);
  else
    can_tx_error_count++;

  /* --- MOTION: gps_motion_data (speed, course) --- */
  struct feb_can_gps_motion_data_t motion = {
      .speed = feb_can_gps_motion_data_speed_encode(gps_data.speed_kmh),
      .course = feb_can_gps_motion_data_course_encode(gps_data.course),
  };
  packed = feb_can_gps_motion_data_pack(tx_buf, &motion, sizeof(tx_buf));
  if (packed > 0)
    send_frame(FEB_CAN_GPS_MOTION_DATA_FRAME_ID, tx_buf, FEB_CAN_GPS_MOTION_DATA_LENGTH);
  else
    can_tx_error_count++;

  /* --- TIME: gps_time_data (hours, minutes, seconds) --- */
  struct feb_can_gps_time_data_t time_data = {
      .hours = feb_can_gps_time_data_hours_encode((double)gps_data.hours),
      .minutes = feb_can_gps_time_data_minutes_encode((double)gps_data.minutes),
      .seconds = feb_can_gps_time_data_seconds_encode((double)gps_data.seconds),
  };
  packed = feb_can_gps_time_data_pack(tx_buf, &time_data, sizeof(tx_buf));
  if (packed > 0)
    send_frame(FEB_CAN_GPS_TIME_DATA_FRAME_ID, tx_buf, FEB_CAN_GPS_TIME_DATA_LENGTH);
  else
    can_tx_error_count++;

  /* --- DATE: gps_date_data (day, month, year) --- */
  struct feb_can_gps_date_data_t date_data = {
      .day = feb_can_gps_date_data_day_encode((double)gps_data.day),
      .month = feb_can_gps_date_data_month_encode((double)gps_data.month),
      .year = feb_can_gps_date_data_year_encode((double)gps_data.year),
  };
  packed = feb_can_gps_date_data_pack(tx_buf, &date_data, sizeof(tx_buf));
  if (packed > 0)
    send_frame(FEB_CAN_GPS_DATE_DATA_FRAME_ID, tx_buf, FEB_CAN_GPS_DATE_DATA_LENGTH);
  else
    can_tx_error_count++;
}
