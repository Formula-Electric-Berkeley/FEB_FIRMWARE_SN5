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

  uint8_t tx_data[8] = {0};

  /* --- POSITION: gps_latlong_data (lat, long) --- */

  // uint16_t tx_data[4] = {0};
  int16_t packed_lat = (int16_t)(0xBBBB);
  int16_t packed_long = (int16_t)(0xAAAA);

  memcpy(&tx_data[0], &packed_lat, sizeof(int16_t));
  memcpy(&tx_data[2], &packed_long, sizeof(int16_t));

  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_POS_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                                            FEB_CAN_GPS_POS_DATA_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }

  // /* --- MOTION: gps_motion_data (speed, course) --- */
  memset(tx_data, 0, sizeof(tx_data));
  int16_t packed_speed = (int16_t)(0xB0BA);
  int16_t packed_course = (int16_t)(0xCAFE);

  memcpy(&tx_data[0], &packed_speed, sizeof(int16_t));
  memcpy(&tx_data[2], &packed_course, sizeof(int16_t));
  // memcpy(&tx_data[4], &packed_speed, sizeof(int16_t));
  // memcpy(&tx_data[6], &packed_course, sizeof(int16_t));

  status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_MOTION_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                           4); // hetvi: hardcoded sorry

  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }

  /* --- TIME: gps_time_data (hours, minutes, seconds) --- */
  // memset(tx_data, 0, sizeof(tx_data));
  // memcpy(&tx_data[0], &gps_data.hours, sizeof(uint8_t));
  // memcpy(&tx_data[1], &gps_data.minutes, sizeof(uint8_t));
  // memcpy(&tx_data[2], &gps_data.seconds, sizeof(uint8_t));

  // status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_TIME_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
  //                           FEB_CAN_GPS_TIME_DATA_LENGTH);

  // if (status != FEB_CAN_OK)
  // {
  //   can_tx_error_count++;
  // }

  /* --- DATE: gps_date_data (day, month, year) --- */
  //   memset(tx_data, 0, sizeof(tx_data));
  //   memcpy(&tx_data[0], &gps_data.day, sizeof(uint8_t));
  //   memcpy(&tx_data[1], &gps_data.month, sizeof(uint8_t));
  //   memcpy(&tx_data[2], &gps_data.year, sizeof(uint8_t));

  //   status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_GPS_DATE_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data,
  //                             FEB_CAN_GPS_DATE_DATA_LENGTH);

  //   if (status != FEB_CAN_OK)
  //   {
  //     can_tx_error_count++;
  // }
}
