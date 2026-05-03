/**
 ******************************************************************************
 * @file           : FEB_CAN_GPS.c
 * @brief          : CAN GPS Reporter Module Implementation. Variant-agnostic
 *                   via FEB_SN_Config.h. No-op if FEB_SN_HAS_GPS=0.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Frames published per tick (CAN1) — IDs differ FRONT vs REAR; signals match:
 *   gps_pos_data       lat_e7 (i32, 1e-7 deg/LSB), lon_e7 (i32, 1e-7 deg/LSB)
 *   gps_altitude_data  altitude (i32, 0.01 m/LSB), HDOP (u16 x0.01), VDOP (u16 x0.01)
 *   gps_motion_data    speed (u16, 0.01 km/h/LSB), course (u16, 0.01 deg/LSB)
 *   gps_time_data      hours, minutes, seconds (u8, UTC)
 *   gps_date_data      day, month, year (u8, UTC)
 *   gps_status_data    fix_type, fix_mode, sats_in_use, sats_in_view,
 *                      valid, has_fix (u8 each), pdop (u16 x0.01)
 ******************************************************************************
 */

#include "FEB_CAN_GPS.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_GPS.h"
#include "FEB_SN_Config.h"
#include <stdint.h>

static uint32_t can_tx_error_count = 0;

void FEB_CAN_GPS_Init(void) {}

#if FEB_SN_HAS_GPS
static inline void tx_or_count(uint32_t frame_id, uint8_t *data, uint32_t len)
{
  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_id, FEB_CAN_ID_STD, data, len) != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}
#endif

void FEB_CAN_GPS_Tick(void)
{
#if FEB_SN_HAS_GPS
  FEB_GPS_Data_t gps;
  (void)FEB_GPS_GetLatestData(&gps);

  /* ---------------- Position (FRONT 0x40 / REAR 0x50) ---------------- */
  {
    struct feb_sn_gps_pos_t s = {
        .latitude = feb_sn_gps_pos_latitude_encode(gps.latitude),
        .longitude = feb_sn_gps_pos_longitude_encode(gps.longitude),
    };
    uint8_t buf[FEB_SN_GPS_POS_LENGTH];
    feb_sn_gps_pos_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_GPS_POS_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Altitude + HDOP/VDOP (FRONT 0x41 / REAR 0x51) ---------------- */
  {
    struct feb_sn_gps_altitude_t s = {
        .altitude = feb_sn_gps_altitude_altitude_encode(gps.altitude),
        .hdop = feb_sn_gps_altitude_hdop_encode(gps.hdop),
        .vdop = feb_sn_gps_altitude_vdop_encode(gps.vdop),
    };
    uint8_t buf[FEB_SN_GPS_ALTITUDE_LENGTH];
    feb_sn_gps_altitude_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_GPS_ALTITUDE_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Motion (speed + course) (FRONT 0x42 / REAR 0x52) ---------------- */
  {
    struct feb_sn_gps_motion_t s = {
        .speed = feb_sn_gps_motion_speed_encode(gps.speed_kmh),
        .course = feb_sn_gps_motion_course_encode(gps.course),
    };
    uint8_t buf[FEB_SN_GPS_MOTION_LENGTH];
    feb_sn_gps_motion_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_GPS_MOTION_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Time UTC (FRONT 0x43 / REAR 0x53) ---------------- */
  {
    struct feb_sn_gps_time_t s = {
        .hours = feb_sn_gps_time_hours_encode((double)gps.hours),
        .minutes = feb_sn_gps_time_minutes_encode((double)gps.minutes),
        .seconds = feb_sn_gps_time_seconds_encode((double)gps.seconds),
    };
    uint8_t buf[FEB_SN_GPS_TIME_LENGTH];
    feb_sn_gps_time_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_GPS_TIME_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Date UTC (FRONT 0x44 / REAR 0x54) ---------------- */
  {
    struct feb_sn_gps_date_t s = {
        .day = feb_sn_gps_date_day_encode((double)gps.day),
        .month = feb_sn_gps_date_month_encode((double)gps.month),
        .year = feb_sn_gps_date_year_encode((double)gps.year),
    };
    uint8_t buf[FEB_SN_GPS_DATE_LENGTH];
    feb_sn_gps_date_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_GPS_DATE_FRAME_ID, buf, sizeof(buf));
  }

  /* ---------------- Status (FRONT 0x45 / REAR 0x55) ---------------- */
  {
    struct feb_sn_gps_status_t s = {
        .fix_type = feb_sn_gps_status_fix_type_encode((double)gps.fix),
        .fix_mode = feb_sn_gps_status_fix_mode_encode((double)gps.fix_mode),
        .sats_in_use = feb_sn_gps_status_sats_in_use_encode((double)gps.sats_in_use),
        .sats_in_view = feb_sn_gps_status_sats_in_view_encode((double)gps.sats_in_view),
        .valid = feb_sn_gps_status_valid_encode((double)(gps.valid ? 1 : 0)),
        .has_fix = feb_sn_gps_status_has_fix_encode((double)(gps.has_fix ? 1 : 0)),
        .pdop = feb_sn_gps_status_pdop_encode(gps.pdop),
    };
    uint8_t buf[FEB_SN_GPS_STATUS_LENGTH];
    feb_sn_gps_status_pack(buf, &s, sizeof(buf));
    tx_or_count(FEB_SN_GPS_STATUS_FRAME_ID, buf, sizeof(buf));
  }
#endif
}
