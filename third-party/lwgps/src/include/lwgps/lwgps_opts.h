/**
 * @file    lwgps_opts.h
 * @brief   LwGPS configuration for FEB Sensor Nodes (MTK3339 GPS)
 */

#ifndef LWGPS_OPTS_H
#define LWGPS_OPTS_H

/* Use double precision for latitude/longitude (STM32F446 has FPU) */
#define LWGPS_CFG_DOUBLE              1

/* Enable NMEA statement parsing */
#define LWGPS_CFG_STATEMENT_GPGGA     1   /* Lat, Lon, Alt, Time, Fix, Sats */
#define LWGPS_CFG_STATEMENT_GPGSA     1   /* DOP values, Fix mode */
#define LWGPS_CFG_STATEMENT_GPGSV     1   /* Satellites in view */
#define LWGPS_CFG_STATEMENT_GPRMC     1   /* Speed, Course, Date, Validity */

/* Disable detailed satellite info to save memory */
#define LWGPS_CFG_STATEMENT_GPGSV_SAT_DET  0

/* Enable CRC validation for NMEA sentences */
#define LWGPS_CFG_CRC                 1

/* Disable uBlox-specific extensions (MTK3339 doesn't support them) */
#define LWGPS_CFG_STATEMENT_PUBX      0
#define LWGPS_CFG_STATEMENT_PUBX_TIME 0

#endif /* LWGPS_OPTS_H */
