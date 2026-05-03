/**
 ******************************************************************************
 * @file           : FEB_CAN_GPS.c
 * @brief          : CAN GPS Reporter Module Implementation.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Frames published per tick (CAN1):
 *   0x40 gps_pos_data       lat_e7 (i32, 1e-7 deg/LSB), lon_e7 (i32, 1e-7 deg/LSB)
 *   0x41 gps_altitude_data  altitude (i32, 0.01 m/LSB), HDOP (u16 x0.01), VDOP (u16 x0.01)
 *   0x42 gps_motion_data    speed (u16, 0.01 km/h/LSB), course (u16, 0.01 deg/LSB)
 *   0x43 gps_time_data      hours, minutes, seconds (u8, UTC)
 *   0x44 gps_date_data      day, month, year (u8, UTC)
 *   0x45 gps_status_data    fix_type, fix_mode, sats_in_use, sats_in_view,
 *                           valid, has_fix (u8 each), pdop (u16 x0.01)
 ******************************************************************************
 */

#include "FEB_CAN_GPS.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_GPS.h"
#include <stdint.h>

static uint32_t can_tx_error_count = 0;

void FEB_CAN_GPS_Init(void) {}

static inline void tx_or_count(uint32_t frame_id, uint8_t *data, uint32_t len)
{
  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_id, FEB_CAN_ID_STD, data, len) != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}

void FEB_CAN_GPS_Tick(void)
{
  FEB_GPS_Data_t gps;
  (void)FEB_GPS_GetLatestData(&gps);

  /* ---------------- Position (0x40) ---------------- */
  {
    struct feb_can_gps_pos_data_t s = {
        .latitude = feb_can_gps_pos_data_latitude_encode(gps.latitude),
        .longitude = feb_can_gps_pos_data_longitude_encode(gps.longitude),
    };
    uint8_t buf[FEB_CAN_GPS_POS_DATA_LENGTH];
    feb_can_gps_pos_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_GPS_POS_DATA_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Altitude + HDOP/VDOP (0x41) ---------------- */
  {
    struct feb_can_gps_altitude_data_t s = {
        .altitude = feb_can_gps_altitude_data_altitude_encode(gps.altitude),
        .hdop = feb_can_gps_altitude_data_hdop_encode(gps.hdop),
        .vdop = feb_can_gps_altitude_data_vdop_encode(gps.vdop),
    };
    uint8_t buf[FEB_CAN_GPS_ALTITUDE_DATA_LENGTH];
    feb_can_gps_altitude_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_GPS_ALTITUDE_DATA_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Motion (speed + course) (0x42) ---------------- */
  {
    struct feb_can_gps_motion_data_t s = {
        .speed = feb_can_gps_motion_data_speed_encode(gps.speed_kmh),
        .course = feb_can_gps_motion_data_course_encode(gps.course),
    };
    uint8_t buf[FEB_CAN_GPS_MOTION_DATA_LENGTH];
    feb_can_gps_motion_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_GPS_MOTION_DATA_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Time UTC (0x43) ---------------- */
  {
    struct feb_can_gps_time_data_t s = {
        .hours = feb_can_gps_time_data_hours_encode((double)gps.hours),
        .minutes = feb_can_gps_time_data_minutes_encode((double)gps.minutes),
        .seconds = feb_can_gps_time_data_seconds_encode((double)gps.seconds),
    };
    uint8_t buf[FEB_CAN_GPS_TIME_DATA_LENGTH];
    feb_can_gps_time_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_GPS_TIME_DATA_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Date UTC (0x44) ---------------- */
  {
    struct feb_can_gps_date_data_t s = {
        .day = feb_can_gps_date_data_day_encode((double)gps.day),
        .month = feb_can_gps_date_data_month_encode((double)gps.month),
        .year = feb_can_gps_date_data_year_encode((double)gps.year),
    };
    uint8_t buf[FEB_CAN_GPS_DATE_DATA_LENGTH];
    feb_can_gps_date_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_GPS_DATE_DATA_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Status (0x45) ---------------- */
  {
    struct feb_can_gps_status_data_t s = {
        .fix_type = feb_can_gps_status_data_fix_type_encode((double)gps.fix),
        .fix_mode = feb_can_gps_status_data_fix_mode_encode((double)gps.fix_mode),
        .sats_in_use = feb_can_gps_status_data_sats_in_use_encode((double)gps.sats_in_use),
        .sats_in_view = feb_can_gps_status_data_sats_in_view_encode((double)gps.sats_in_view),
        .valid = feb_can_gps_status_data_valid_encode((double)(gps.valid ? 1 : 0)),
        .has_fix = feb_can_gps_status_data_has_fix_encode((double)(gps.has_fix ? 1 : 0)),
        .pdop = feb_can_gps_status_data_pdop_encode(gps.pdop),
    };
    uint8_t buf[FEB_CAN_GPS_STATUS_DATA_LENGTH];
    feb_can_gps_status_data_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_CAN_GPS_STATUS_DATA_FRAME_ID, buf, sizeof(buf));
  }
}
