/**
 ******************************************************************************
 * @file           : FEB_SN_Commands.c
 * @brief          : Sensor Node Console Commands (IMU, MAG)
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_SN_Commands.h"
#include "FEB_IMU.h"
#include "FEB_Magnetometer.h"
#include "FEB_GPS.h"
#include "feb_console.h"
#include "feb_string_utils.h"
#include "lsm6dsox_reg.h"
#include "lis3mdl_reg.h"
#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------- */
/*                              IMU Commands                                  */
/* -------------------------------------------------------------------------- */

static void print_imu_help(void)
{
  FEB_Console_Printf("IMU Commands:\r\n");
  FEB_Console_Printf("  IMU|status  - Show IMU init status and device ID\r\n");
  FEB_Console_Printf("  IMU|accel   - Read acceleration X, Y, Z [mg]\r\n");
  FEB_Console_Printf("  IMU|gyro    - Read angular rate X, Y, Z [mdps]\r\n");
  FEB_Console_Printf("  IMU|temp    - Read IMU temperature [C]\r\n");
  FEB_Console_Printf("  IMU|all     - Read all IMU data\r\n");
}

static void cmd_imu_status(void)
{
  uint8_t whoamI = 0;
  int32_t ret = lsm6dsox_device_id_get(&lsm6dsox_ctx, &whoamI);

  FEB_Console_Printf("=== IMU Status ===\r\n");
  FEB_Console_Printf("Device: LSM6DSOX\r\n");
  FEB_Console_Printf("WHO_AM_I: 0x%02X (expected 0x%02X)\r\n", whoamI, LSM6DSOX_ID);
  FEB_Console_Printf("Status: %s\r\n", (ret == 0 && whoamI == LSM6DSOX_ID) ? "OK" : "ERROR");
}

static void cmd_imu_accel(void)
{
  int16_t raw[3];
  memset(raw, 0, sizeof(raw));

  if (lsm6dsox_acceleration_raw_get(&lsm6dsox_ctx, raw) != 0)
  {
    FEB_Console_Printf("Error reading acceleration\r\n");
    return;
  }

  float_t mg[3];
  mg[0] = lsm6dsox_from_fs2_to_mg(raw[0]);
  mg[1] = lsm6dsox_from_fs2_to_mg(raw[1]);
  mg[2] = lsm6dsox_from_fs2_to_mg(raw[2]);

  FEB_Console_Printf("=== Acceleration [mg] ===\r\n");
  FEB_Console_Printf("X: %.2f\r\n", mg[0]);
  FEB_Console_Printf("Y: %.2f\r\n", mg[1]);
  FEB_Console_Printf("Z: %.2f\r\n", mg[2]);
}

static void cmd_imu_gyro(void)
{
  int16_t raw[3];
  memset(raw, 0, sizeof(raw));

  if (lsm6dsox_angular_rate_raw_get(&lsm6dsox_ctx, raw) != 0)
  {
    FEB_Console_Printf("Error reading angular rate\r\n");
    return;
  }

  float_t mdps[3];
  mdps[0] = lsm6dsox_from_fs2000_to_mdps(raw[0]);
  mdps[1] = lsm6dsox_from_fs2000_to_mdps(raw[1]);
  mdps[2] = lsm6dsox_from_fs2000_to_mdps(raw[2]);

  FEB_Console_Printf("=== Angular Rate [mdps] ===\r\n");
  FEB_Console_Printf("X: %.2f\r\n", mdps[0]);
  FEB_Console_Printf("Y: %.2f\r\n", mdps[1]);
  FEB_Console_Printf("Z: %.2f\r\n", mdps[2]);
}

static void cmd_imu_temp(void)
{
  int16_t raw_temp;

  if (lsm6dsox_temperature_raw_get(&lsm6dsox_ctx, &raw_temp) != 0)
  {
    FEB_Console_Printf("Error reading temperature\r\n");
    return;
  }

  float_t temp_c = lsm6dsox_from_lsb_to_celsius(raw_temp);

  FEB_Console_Printf("=== IMU Temperature ===\r\n");
  FEB_Console_Printf("Temperature: %.2f C\r\n", temp_c);
}

static void cmd_imu_all(void)
{
  cmd_imu_accel();
  FEB_Console_Printf("\r\n");
  cmd_imu_gyro();
  FEB_Console_Printf("\r\n");
  cmd_imu_temp();
}

static void cmd_imu(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_imu_help();
    return;
  }

  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    cmd_imu_status();
  }
  else if (FEB_strcasecmp(subcmd, "accel") == 0)
  {
    cmd_imu_accel();
  }
  else if (FEB_strcasecmp(subcmd, "gyro") == 0)
  {
    cmd_imu_gyro();
  }
  else if (FEB_strcasecmp(subcmd, "temp") == 0)
  {
    cmd_imu_temp();
  }
  else if (FEB_strcasecmp(subcmd, "all") == 0)
  {
    cmd_imu_all();
  }
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_imu_help();
  }
}

static void csv_imu_status(void)
{
  uint8_t whoamI = 0;
  int32_t ret = lsm6dsox_device_id_get(&lsm6dsox_ctx, &whoamI);
  int init_ok = (ret == 0 && whoamI == LSM6DSOX_ID) ? 1 : 0;
  FEB_Console_CsvPrintf("imuStat", "0x%02X,0x%02X,%d\r\n", whoamI, LSM6DSOX_ID, init_ok);
}

static void csv_imu_accel(void)
{
  int16_t raw[3] = {0};
  if (lsm6dsox_acceleration_raw_get(&lsm6dsox_ctx, raw) != 0)
  {
    FEB_Console_CsvPrintf("csv_err", "imu_accel_read\r\n");
    return;
  }
  FEB_Console_CsvPrintf("imuAccel", "%.2f,%.2f,%.2f\r\n", lsm6dsox_from_fs2_to_mg(raw[0]),
                        lsm6dsox_from_fs2_to_mg(raw[1]), lsm6dsox_from_fs2_to_mg(raw[2]));
}

static void csv_imu_gyro(void)
{
  int16_t raw[3] = {0};
  if (lsm6dsox_angular_rate_raw_get(&lsm6dsox_ctx, raw) != 0)
  {
    FEB_Console_CsvPrintf("csv_err", "imu_gyro_read\r\n");
    return;
  }
  FEB_Console_CsvPrintf("imuGyro", "%.2f,%.2f,%.2f\r\n", lsm6dsox_from_fs2000_to_mdps(raw[0]),
                        lsm6dsox_from_fs2000_to_mdps(raw[1]), lsm6dsox_from_fs2000_to_mdps(raw[2]));
}

static void csv_imu_temp(void)
{
  int16_t raw_temp;
  if (lsm6dsox_temperature_raw_get(&lsm6dsox_ctx, &raw_temp) != 0)
  {
    FEB_Console_CsvPrintf("csv_err", "imu_temp_read\r\n");
    return;
  }
  FEB_Console_CsvPrintf("imuTemp", "%.2f\r\n", lsm6dsox_from_lsb_to_celsius(raw_temp));
}

static void cmd_imu_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvPrintf("csv_err", "imu_usage,status|accel|gyro|temp|all\r\n");
    return;
  }
  const char *subcmd = argv[1];
  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    csv_imu_status();
  }
  else if (FEB_strcasecmp(subcmd, "accel") == 0)
  {
    csv_imu_accel();
  }
  else if (FEB_strcasecmp(subcmd, "gyro") == 0)
  {
    csv_imu_gyro();
  }
  else if (FEB_strcasecmp(subcmd, "temp") == 0)
  {
    csv_imu_temp();
  }
  else if (FEB_strcasecmp(subcmd, "all") == 0)
  {
    csv_imu_accel();
    csv_imu_gyro();
    csv_imu_temp();
  }
  else
  {
    FEB_Console_CsvPrintf("csv_err", "imu_mode,%s\r\n", subcmd);
  }
}

/* -------------------------------------------------------------------------- */
/*                          Magnetometer Commands                             */
/* -------------------------------------------------------------------------- */

static void print_mag_help(void)
{
  FEB_Console_Printf("MAG Commands:\r\n");
  FEB_Console_Printf("  MAG|status  - Show magnetometer init status and device ID\r\n");
  FEB_Console_Printf("  MAG|field   - Read magnetic field X, Y, Z [mG]\r\n");
  FEB_Console_Printf("  MAG|temp    - Read magnetometer temperature [C]\r\n");
  FEB_Console_Printf("  MAG|all     - Read all magnetometer data\r\n");
}

static void cmd_mag_status(void)
{
  uint8_t whoamI = 0;
  int32_t ret = lis3mdl_device_id_get(&lis3mdl_ctx, &whoamI);

  FEB_Console_Printf("=== Magnetometer Status ===\r\n");
  FEB_Console_Printf("Device: LIS3MDL\r\n");
  FEB_Console_Printf("WHO_AM_I: 0x%02X (expected 0x%02X)\r\n", whoamI, LIS3MDL_ID);
  FEB_Console_Printf("Status: %s\r\n", (ret == 0 && whoamI == LIS3MDL_ID) ? "OK" : "ERROR");
}

static void cmd_mag_field(void)
{
  int16_t raw[3];
  memset(raw, 0, sizeof(raw));

  if (lis3mdl_magnetic_raw_get(&lis3mdl_ctx, raw) != 0)
  {
    FEB_Console_Printf("Error reading magnetic field\r\n");
    return;
  }

  float_t mG[3];
  mG[0] = 1000.0f * lis3mdl_from_fs16_to_gauss(raw[0]);
  mG[1] = 1000.0f * lis3mdl_from_fs16_to_gauss(raw[1]);
  mG[2] = 1000.0f * lis3mdl_from_fs16_to_gauss(raw[2]);

  FEB_Console_Printf("=== Magnetic Field [mG] ===\r\n");
  FEB_Console_Printf("X: %.2f\r\n", mG[0]);
  FEB_Console_Printf("Y: %.2f\r\n", mG[1]);
  FEB_Console_Printf("Z: %.2f\r\n", mG[2]);
}

static void cmd_mag_temp(void)
{
  int16_t raw_temp;

  if (lis3mdl_temperature_raw_get(&lis3mdl_ctx, &raw_temp) != 0)
  {
    FEB_Console_Printf("Error reading temperature\r\n");
    return;
  }

  float_t temp_c = lis3mdl_from_lsb_to_celsius(raw_temp);

  FEB_Console_Printf("=== Magnetometer Temperature ===\r\n");
  FEB_Console_Printf("Temperature: %.2f C\r\n", temp_c);
}

static void cmd_mag_all(void)
{
  cmd_mag_field();
  FEB_Console_Printf("\r\n");
  cmd_mag_temp();
}

static void cmd_mag(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_mag_help();
    return;
  }

  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    cmd_mag_status();
  }
  else if (FEB_strcasecmp(subcmd, "field") == 0)
  {
    cmd_mag_field();
  }
  else if (FEB_strcasecmp(subcmd, "temp") == 0)
  {
    cmd_mag_temp();
  }
  else if (FEB_strcasecmp(subcmd, "all") == 0)
  {
    cmd_mag_all();
  }
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_mag_help();
  }
}

static void csv_mag_status(void)
{
  uint8_t whoamI = 0;
  int32_t ret = lis3mdl_device_id_get(&lis3mdl_ctx, &whoamI);
  int init_ok = (ret == 0 && whoamI == LIS3MDL_ID) ? 1 : 0;
  FEB_Console_CsvPrintf("magStat", "0x%02X,0x%02X,%d\r\n", whoamI, LIS3MDL_ID, init_ok);
}

static void csv_mag_field(void)
{
  int16_t raw[3] = {0};
  if (lis3mdl_magnetic_raw_get(&lis3mdl_ctx, raw) != 0)
  {
    FEB_Console_CsvPrintf("csv_err", "mag_field_read\r\n");
    return;
  }
  FEB_Console_CsvPrintf("magField", "%.2f,%.2f,%.2f\r\n", 1000.0f * lis3mdl_from_fs16_to_gauss(raw[0]),
                        1000.0f * lis3mdl_from_fs16_to_gauss(raw[1]), 1000.0f * lis3mdl_from_fs16_to_gauss(raw[2]));
}

static void csv_mag_temp(void)
{
  int16_t raw_temp;
  if (lis3mdl_temperature_raw_get(&lis3mdl_ctx, &raw_temp) != 0)
  {
    FEB_Console_CsvPrintf("csv_err", "mag_temp_read\r\n");
    return;
  }
  FEB_Console_CsvPrintf("magTemp", "%.2f\r\n", lis3mdl_from_lsb_to_celsius(raw_temp));
}

static void cmd_mag_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvPrintf("csv_err", "mag_usage,status|field|temp|all\r\n");
    return;
  }
  const char *subcmd = argv[1];
  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    csv_mag_status();
  }
  else if (FEB_strcasecmp(subcmd, "field") == 0)
  {
    csv_mag_field();
  }
  else if (FEB_strcasecmp(subcmd, "temp") == 0)
  {
    csv_mag_temp();
  }
  else if (FEB_strcasecmp(subcmd, "all") == 0)
  {
    csv_mag_field();
    csv_mag_temp();
  }
  else
  {
    FEB_Console_CsvPrintf("csv_err", "mag_mode,%s\r\n", subcmd);
  }
}

/* -------------------------------------------------------------------------- */
/*                              GPS Commands                                   */
/* -------------------------------------------------------------------------- */

static void print_gps_help(void)
{
  FEB_Console_Printf("GPS Commands:\r\n");
  FEB_Console_Printf("  GPS|status   - Show GPS status and fix info\r\n");
  FEB_Console_Printf("  GPS|pos      - Show position (lat, lon, alt)\r\n");
  FEB_Console_Printf("  GPS|time     - Show UTC time and date\r\n");
  FEB_Console_Printf("  GPS|speed    - Show speed and course\r\n");
  FEB_Console_Printf("  GPS|sats     - Show satellite and DOP info\r\n");
  FEB_Console_Printf("  GPS|all      - Show all GPS data\r\n");
  FEB_Console_Printf("  GPS|enable   - Enable GPS module\r\n");
  FEB_Console_Printf("  GPS|disable  - Disable GPS module\r\n");
  FEB_Console_Printf("  GPS|rate <hz>  - Set update rate (1, 5, 10)\r\n");
  FEB_Console_Printf("  GPS|pmtk <cmd> - Send raw PMTK command\r\n");
}

static void cmd_gps_status(void)
{
  FEB_GPS_Data_t data;
  FEB_GPS_GetLatestData(&data);

  const char *fix_str;
  switch (data.fix)
  {
  case 0:
    fix_str = "Invalid";
    break;
  case 1:
    fix_str = "GPS";
    break;
  case 2:
    fix_str = "DGPS";
    break;
  case 3:
    fix_str = "PPS";
    break;
  default:
    fix_str = "Unknown";
    break;
  }

  const char *mode_str;
  switch (data.fix_mode)
  {
  case 1:
    mode_str = "No Fix";
    break;
  case 2:
    mode_str = "2D";
    break;
  case 3:
    mode_str = "3D";
    break;
  default:
    mode_str = "Unknown";
    break;
  }

  FEB_Console_Printf("=== GPS Status ===\r\n");
  FEB_Console_Printf("Module: %s\r\n", FEB_GPS_IsEnabled() ? "Enabled" : "Disabled");
  FEB_Console_Printf("Valid: %s\r\n", data.valid ? "Yes" : "No");
  FEB_Console_Printf("Fix Type: %s\r\n", fix_str);
  FEB_Console_Printf("Fix Mode: %s\r\n", mode_str);
  FEB_Console_Printf("Satellites: %u in use, %u in view\r\n", data.sats_in_use, data.sats_in_view);
  FEB_Console_Printf("Last Update: %u ms ago\r\n", (unsigned int)(HAL_GetTick() - data.last_update_ms));
}

static void cmd_gps_pos(void)
{
  FEB_GPS_Data_t data;
  FEB_GPS_GetLatestData(&data);

  FEB_Console_Printf("=== GPS Position ===\r\n");
  if (!data.has_fix)
  {
    FEB_Console_Printf("No fix available\r\n");
    return;
  }
  FEB_Console_Printf("Latitude:  %.6f deg\r\n", data.latitude);
  FEB_Console_Printf("Longitude: %.6f deg\r\n", data.longitude);
  FEB_Console_Printf("Altitude:  %.1f m\r\n", data.altitude);
}

static void cmd_gps_time(void)
{
  FEB_GPS_Data_t data;
  FEB_GPS_GetLatestData(&data);

  FEB_Console_Printf("=== GPS Time (UTC) ===\r\n");
  FEB_Console_Printf("Time: %02u:%02u:%02u\r\n", data.hours, data.minutes, data.seconds);
  FEB_Console_Printf("Date: 20%02u-%02u-%02u\r\n", data.year, data.month, data.day);
}

static void cmd_gps_speed(void)
{
  FEB_GPS_Data_t data;
  FEB_GPS_GetLatestData(&data);

  FEB_Console_Printf("=== GPS Speed ===\r\n");
  if (!data.has_fix)
  {
    FEB_Console_Printf("No fix available\r\n");
    return;
  }
  FEB_Console_Printf("Speed:  %.2f km/h\r\n", data.speed_kmh);
  FEB_Console_Printf("Course: %.1f deg\r\n", data.course);
}

static void cmd_gps_sats(void)
{
  FEB_GPS_Data_t data;
  FEB_GPS_GetLatestData(&data);

  FEB_Console_Printf("=== GPS Satellites ===\r\n");
  FEB_Console_Printf("In Use: %u\r\n", data.sats_in_use);
  FEB_Console_Printf("In View: %u\r\n", data.sats_in_view);
  FEB_Console_Printf("HDOP: %.2f\r\n", data.hdop);
  FEB_Console_Printf("VDOP: %.2f\r\n", data.vdop);
  FEB_Console_Printf("PDOP: %.2f\r\n", data.pdop);
}

static void cmd_gps_all(void)
{
  FEB_GPS_Data_t data;
  FEB_GPS_GetLatestData(&data);

  FEB_Console_Printf("=== GPS Data ===\r\n");
  FEB_Console_Printf("Module: %s | Valid: %s | Fix: %s\r\n", FEB_GPS_IsEnabled() ? "ON" : "OFF",
                     data.valid ? "Yes" : "No", data.has_fix ? "Yes" : "No");
  FEB_Console_Printf("Position: %.6f%c, %.6f%c | Alt: %.1f m\r\n", data.latitude >= 0 ? data.latitude : -data.latitude,
                     data.latitude >= 0 ? 'N' : 'S', data.longitude >= 0 ? data.longitude : -data.longitude,
                     data.longitude >= 0 ? 'E' : 'W', data.altitude);
  FEB_Console_Printf("Speed: %.2f km/h | Course: %.1f deg\r\n", data.speed_kmh, data.course);
  FEB_Console_Printf("Time: %02u:%02u:%02u | Date: 20%02u-%02u-%02u\r\n", data.hours, data.minutes, data.seconds,
                     data.year, data.month, data.day);
  FEB_Console_Printf("Sats: %u/%u | HDOP: %.2f\r\n", data.sats_in_use, data.sats_in_view, data.hdop);
}

static void cmd_gps_enable(void)
{
  FEB_GPS_SetEnabled(true);
  FEB_Console_Printf("GPS module enabled\r\n");
}

static void cmd_gps_disable(void)
{
  FEB_GPS_SetEnabled(false);
  FEB_Console_Printf("GPS module disabled\r\n");
}

static void cmd_gps_rate(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: GPS|rate <1|5|10>\r\n");
    return;
  }

  int hz = atoi(argv[2]);
  if (hz != 1 && hz != 5 && hz != 10)
  {
    FEB_Console_Printf("Invalid rate. Use 1, 5, or 10 Hz\r\n");
    return;
  }

  if (FEB_GPS_SetUpdateRate((uint8_t)hz) >= 0)
  {
    FEB_Console_Printf("GPS update rate set to %d Hz\r\n", hz);
  }
  else
  {
    FEB_Console_Printf("Failed to set update rate\r\n");
  }
}

static void cmd_gps_pmtk(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: GPS|pmtk <command>\r\n");
    FEB_Console_Printf("Example: GPS|pmtk PMTK101\r\n");
    return;
  }

  if (FEB_GPS_SendPMTKCommand(argv[2]) >= 0)
  {
    FEB_Console_Printf("PMTK command sent: %s\r\n", argv[2]);
  }
  else
  {
    FEB_Console_Printf("Failed to send PMTK command\r\n");
  }
}

static void cmd_gps(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_gps_help();
    return;
  }

  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    cmd_gps_status();
  }
  else if (FEB_strcasecmp(subcmd, "pos") == 0)
  {
    cmd_gps_pos();
  }
  else if (FEB_strcasecmp(subcmd, "time") == 0)
  {
    cmd_gps_time();
  }
  else if (FEB_strcasecmp(subcmd, "speed") == 0)
  {
    cmd_gps_speed();
  }
  else if (FEB_strcasecmp(subcmd, "sats") == 0)
  {
    cmd_gps_sats();
  }
  else if (FEB_strcasecmp(subcmd, "all") == 0)
  {
    cmd_gps_all();
  }
  else if (FEB_strcasecmp(subcmd, "enable") == 0)
  {
    cmd_gps_enable();
  }
  else if (FEB_strcasecmp(subcmd, "disable") == 0)
  {
    cmd_gps_disable();
  }
  else if (FEB_strcasecmp(subcmd, "rate") == 0)
  {
    cmd_gps_rate(argc, argv);
  }
  else if (FEB_strcasecmp(subcmd, "pmtk") == 0)
  {
    cmd_gps_pmtk(argc, argv);
  }
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_gps_help();
  }
}

static void csv_gps_status(void)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);
  FEB_Console_CsvPrintf("gpsStat", "%d,%d,%d,%u,%u,%u,%u,%u\r\n", FEB_GPS_IsEnabled() ? 1 : 0, d.valid ? 1 : 0,
                        d.has_fix ? 1 : 0, (unsigned int)d.fix, (unsigned int)d.fix_mode, (unsigned int)d.sats_in_use,
                        (unsigned int)d.sats_in_view, (unsigned int)(HAL_GetTick() - d.last_update_ms));
}

static void csv_gps_pos(void)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);
  FEB_Console_CsvPrintf("gpsPos", "%d,%.6f,%.6f,%.1f\r\n", d.has_fix ? 1 : 0, d.latitude, d.longitude, d.altitude);
}

static void csv_gps_time(void)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);
  FEB_Console_CsvPrintf("gpsTime", "%02u,%02u,%02u,20%02u,%02u,%02u\r\n", d.hours, d.minutes, d.seconds, d.year,
                        d.month, d.day);
}

static void csv_gps_speed(void)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);
  FEB_Console_CsvPrintf("gpsSpeed", "%d,%.2f,%.1f\r\n", d.has_fix ? 1 : 0, d.speed_kmh, d.course);
}

static void csv_gps_sats(void)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);
  FEB_Console_CsvPrintf("gpsSats", "%u,%u,%.2f,%.2f,%.2f\r\n", (unsigned int)d.sats_in_use,
                        (unsigned int)d.sats_in_view, d.hdop, d.vdop, d.pdop);
}

static void csv_gps_rate(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_CsvPrintf("csv_err", "gps_rate_usage,hz=1|5|10\r\n");
    return;
  }
  int hz = atoi(argv[2]);
  if (hz != 1 && hz != 5 && hz != 10)
  {
    FEB_Console_CsvPrintf("csv_err", "gps_rate_value,%s\r\n", argv[2]);
    return;
  }
  int ok = FEB_GPS_SetUpdateRate((uint8_t)hz) >= 0 ? 1 : 0;
  FEB_Console_CsvPrintf("gpsRateAck", "%d,%d\r\n", hz, ok);
}

static void csv_gps_pmtk(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_CsvPrintf("csv_err", "gps_pmtk_usage,cmd\r\n");
    return;
  }
  int ok = FEB_GPS_SendPMTKCommand(argv[2]) >= 0 ? 1 : 0;

  /* PMTK sentences are comma-delimited by protocol, so raw echoing into
   * a CSV field breaks column alignment. RFC 4180 escape: wrap in
   * double quotes and double any embedded '"'. */
  char escaped[FEB_CONSOLE_LINE_BUFFER_SIZE];
  size_t pos = 0;
  if (pos + 1 < sizeof(escaped))
  {
    escaped[pos++] = '"';
  }
  for (const char *a = argv[2]; *a && pos + 1 < sizeof(escaped); a++)
  {
    if (*a == '"' && pos + 2 < sizeof(escaped))
    {
      escaped[pos++] = '"';
    }
    escaped[pos++] = *a;
  }
  if (pos + 1 < sizeof(escaped))
  {
    escaped[pos++] = '"';
  }
  escaped[pos] = '\0';
  FEB_Console_CsvPrintf("gpsPmtkAck", "%s,%d\r\n", escaped, ok);
}

static void cmd_gps_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvPrintf("csv_err", "gps_usage,status|pos|time|speed|sats|all|enable|disable|rate|pmtk\r\n");
    return;
  }
  const char *subcmd = argv[1];
  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    csv_gps_status();
  }
  else if (FEB_strcasecmp(subcmd, "pos") == 0)
  {
    csv_gps_pos();
  }
  else if (FEB_strcasecmp(subcmd, "time") == 0)
  {
    csv_gps_time();
  }
  else if (FEB_strcasecmp(subcmd, "speed") == 0)
  {
    csv_gps_speed();
  }
  else if (FEB_strcasecmp(subcmd, "sats") == 0)
  {
    csv_gps_sats();
  }
  else if (FEB_strcasecmp(subcmd, "all") == 0)
  {
    csv_gps_status();
    csv_gps_pos();
    csv_gps_time();
    csv_gps_speed();
    csv_gps_sats();
  }
  else if (FEB_strcasecmp(subcmd, "enable") == 0)
  {
    FEB_GPS_SetEnabled(true);
    FEB_Console_CsvPrintf("gpsEnAck", "1\r\n");
  }
  else if (FEB_strcasecmp(subcmd, "disable") == 0)
  {
    FEB_GPS_SetEnabled(false);
    FEB_Console_CsvPrintf("gpsDisAck", "1\r\n");
  }
  else if (FEB_strcasecmp(subcmd, "rate") == 0)
  {
    csv_gps_rate(argc, argv);
  }
  else if (FEB_strcasecmp(subcmd, "pmtk") == 0)
  {
    csv_gps_pmtk(argc, argv);
  }
  else
  {
    FEB_Console_CsvPrintf("csv_err", "gps_mode,%s\r\n", subcmd);
  }
}

/* -------------------------------------------------------------------------- */
/*                         Command Descriptors                                */
/* -------------------------------------------------------------------------- */

static const FEB_Console_Cmd_t imu_cmd = {
    .name = "IMU",
    .help = "IMU sensor commands (IMU|status, IMU|accel, IMU|gyro, IMU|temp, IMU|all)",
    .handler = cmd_imu,
    .csv_handler = cmd_imu_csv,
};

static const FEB_Console_Cmd_t mag_cmd = {
    .name = "MAG",
    .help = "Magnetometer commands (MAG|status, MAG|field, MAG|temp, MAG|all)",
    .handler = cmd_mag,
    .csv_handler = cmd_mag_csv,
};

static const FEB_Console_Cmd_t gps_cmd = {
    .name = "GPS",
    .help = "GPS commands (GPS|status, GPS|pos, GPS|time, GPS|all, ...)",
    .handler = cmd_gps,
    .csv_handler = cmd_gps_csv,
};

void SN_RegisterCommands(void)
{
  FEB_Console_Register(&imu_cmd);
  FEB_Console_Register(&mag_cmd);
  FEB_Console_Register(&gps_cmd);
}
