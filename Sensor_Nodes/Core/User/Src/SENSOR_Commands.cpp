/**
 ******************************************************************************
 * @file           : SENSOR_Commands.cpp
 * @brief          : Sensor Node console commands (new FEB_Console_2 API)
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "SENSOR_Commands.h"

#include "FEB_SN_Config.h"
#include "FEB_IMU.h"
#include "FEB_Magnetometer.h"
#include "FEB_GPS.h"
#include "FEB_WSS.h"
#include "FEB_LinearPotentiometer.h"
#include "feb_commands_2.hpp"
#include "feb_console_2.hpp"
#include "lis3mdl_reg.h"
#include "lsm6dsox_reg.h"

#include <span>

using namespace feb::console;

namespace
{

/* ============================================================================
 * IMU commands
 * ============================================================================ */

void cmd_imu_status(Interaction &io, std::span<char *const>)
{
  uint8_t whoami = 0;
  const int32_t ret = lsm6dsox_device_id_get(&lsm6dsox_ctx, &whoami);
  const bool ok = (ret == 0) && (whoami == LSM6DSOX_ID);

  KVTable t(io, 14, 12, "IMU Status");
  t.row("Device", "%s", "LSM6DSOX");
  t.row("WHO_AM_I", "0x%02X", (unsigned)whoami);
  t.row("Expected", "0x%02X", (unsigned)LSM6DSOX_ID);
  t.row("Result", "%s", ok ? "OK" : "ERROR");
}

void cmd_imu_accel(Interaction &io, std::span<char *const>)
{
  int16_t raw[3] = {0};
  if (lsm6dsox_acceleration_raw_get(&lsm6dsox_ctx, raw) != 0)
  {
    io.error("error", "imu_accel_read");
    return;
  }
  KVTable t(io, 4, 12, "Acceleration [mg]");
  t.row("X", "%.2f", lsm6dsox_from_fs2_to_mg(raw[0]));
  t.row("Y", "%.2f", lsm6dsox_from_fs2_to_mg(raw[1]));
  t.row("Z", "%.2f", lsm6dsox_from_fs2_to_mg(raw[2]));
}

void cmd_imu_gyro(Interaction &io, std::span<char *const>)
{
  int16_t raw[3] = {0};
  if (lsm6dsox_angular_rate_raw_get(&lsm6dsox_ctx, raw) != 0)
  {
    io.error("error", "imu_gyro_read");
    return;
  }
  KVTable t(io, 4, 12, "Angular Rate [mdps]");
  t.row("X", "%.2f", lsm6dsox_from_fs2000_to_mdps(raw[0]));
  t.row("Y", "%.2f", lsm6dsox_from_fs2000_to_mdps(raw[1]));
  t.row("Z", "%.2f", lsm6dsox_from_fs2000_to_mdps(raw[2]));
}

void cmd_imu_temp(Interaction &io, std::span<char *const>)
{
  int16_t raw = 0;
  if (lsm6dsox_temperature_raw_get(&lsm6dsox_ctx, &raw) != 0)
  {
    io.error("error", "imu_temp_read");
    return;
  }
  KVTable t(io, 14, 12, "IMU Temperature");
  t.row("Temperature", "%.2f C", lsm6dsox_from_lsb_to_celsius(raw));
}

void cmd_imu_all(Interaction &io, std::span<char *const> args)
{
  cmd_imu_accel(io, args);
  cmd_imu_gyro(io, args);
  cmd_imu_temp(io, args);
}

/* ============================================================================
 * Magnetometer commands
 * ============================================================================ */

void cmd_mag_status(Interaction &io, std::span<char *const>)
{
  uint8_t whoami = 0;
  const int32_t ret = lis3mdl_device_id_get(&lis3mdl_ctx, &whoami);
  const bool ok = (ret == 0) && (whoami == LIS3MDL_ID);

  KVTable t(io, 14, 12, "Magnetometer Status");
  t.row("Device", "%s", "LIS3MDL");
  t.row("WHO_AM_I", "0x%02X", (unsigned)whoami);
  t.row("Expected", "0x%02X", (unsigned)LIS3MDL_ID);
  t.row("Result", "%s", ok ? "OK" : "ERROR");
}

void cmd_mag_field(Interaction &io, std::span<char *const>)
{
  int16_t raw[3] = {0};
  if (lis3mdl_magnetic_raw_get(&lis3mdl_ctx, raw) != 0)
  {
    io.error("error", "mag_field_read");
    return;
  }
  KVTable t(io, 4, 12, "Magnetic Field [mG]");
  t.row("X", "%.2f", 1000.0f * lis3mdl_from_fs16_to_gauss(raw[0]));
  t.row("Y", "%.2f", 1000.0f * lis3mdl_from_fs16_to_gauss(raw[1]));
  t.row("Z", "%.2f", 1000.0f * lis3mdl_from_fs16_to_gauss(raw[2]));
}

void cmd_mag_temp(Interaction &io, std::span<char *const>)
{
  int16_t raw = 0;
  if (lis3mdl_temperature_raw_get(&lis3mdl_ctx, &raw) != 0)
  {
    io.error("error", "mag_temp_read");
    return;
  }
  KVTable t(io, 14, 12, "Magnetometer Temperature");
  t.row("Temperature", "%.2f C", lis3mdl_from_lsb_to_celsius(raw));
}

void cmd_mag_all(Interaction &io, std::span<char *const> args)
{
  cmd_mag_field(io, args);
  cmd_mag_temp(io, args);
}

/* ============================================================================
 * GPS commands
 * ============================================================================ */

static const char *gps_fix_name(uint8_t fix)
{
  switch (fix)
  {
  case 0:
    return "Invalid";
  case 1:
    return "GPS";
  case 2:
    return "DGPS";
  case 3:
    return "PPS";
  default:
    return "Unknown";
  }
}

static const char *gps_mode_name(uint8_t mode)
{
  switch (mode)
  {
  case 1:
    return "No Fix";
  case 2:
    return "2D";
  case 3:
    return "3D";
  default:
    return "Unknown";
  }
}

void cmd_gps_status(Interaction &io, std::span<char *const>)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);

  KVTable t(io, 14, 22, "GPS Status");
  t.row("Module", "%s", FEB_GPS_IsEnabled() ? "Enabled" : "Disabled");
  t.row("Valid", "%s", d.valid ? "Yes" : "No");
  t.row("Fix Type", "%s", gps_fix_name(d.fix));
  t.row("Fix Mode", "%s", gps_mode_name(d.fix_mode));
  t.row("Satellites", "%u in use, %u in view", (unsigned)d.sats_in_use, (unsigned)d.sats_in_view);
  t.row("Last Update", "%u ms ago", (unsigned)(HAL_GetTick() - d.last_update_ms));
}

void cmd_gps_pos(Interaction &io, std::span<char *const>)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);

  KVTable t(io, 12, 18, "GPS Position");
  if (!d.has_fix)
  {
    t.row("Status", "%s", "No fix available");
    return;
  }
  t.row("Latitude", "%.6f deg", d.latitude);
  t.row("Longitude", "%.6f deg", d.longitude);
  t.row("Altitude", "%.1f m", d.altitude);
}

void cmd_gps_time(Interaction &io, std::span<char *const>)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);

  KVTable t(io, 10, 18, "GPS Time (UTC)");
  t.row("Time", "%02u:%02u:%02u", d.hours, d.minutes, d.seconds);
  t.row("Date", "20%02u-%02u-%02u", d.year, d.month, d.day);
}

void cmd_gps_speed(Interaction &io, std::span<char *const>)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);

  KVTable t(io, 10, 18, "GPS Speed");
  if (!d.has_fix)
  {
    t.row("Status", "%s", "No fix available");
    return;
  }
  t.row("Speed", "%.2f km/h", d.speed_kmh);
  t.row("Course", "%.1f deg", d.course);
}

void cmd_gps_sats(Interaction &io, std::span<char *const>)
{
  FEB_GPS_Data_t d;
  FEB_GPS_GetLatestData(&d);

  KVTable t(io, 10, 18, "GPS Satellites");
  t.row("In Use", "%u", (unsigned)d.sats_in_use);
  t.row("In View", "%u", (unsigned)d.sats_in_view);
  t.row("HDOP", "%.2f", d.hdop);
  t.row("VDOP", "%.2f", d.vdop);
  t.row("PDOP", "%.2f", d.pdop);
}

void cmd_gps_all(Interaction &io, std::span<char *const> args)
{
  cmd_gps_status(io, args);
  cmd_gps_pos(io, args);
  cmd_gps_time(io, args);
  cmd_gps_speed(io, args);
  cmd_gps_sats(io, args);
}

void cmd_gps_enable(Interaction &io, std::span<char *const>)
{
  FEB_GPS_SetEnabled(true);
  io.println("GPS module enabled");
}

void cmd_gps_disable(Interaction &io, std::span<char *const>)
{
  FEB_GPS_SetEnabled(false);
  io.println("GPS module disabled");
}

void cmd_gps_rate(Interaction &io, std::span<char *const>)
{
  const long hz = io.param_int(0);
  if (FEB_GPS_SetUpdateRate((uint8_t)hz) >= 0)
  {
    io.println("GPS update rate set to %ld Hz", hz);
  }
  else
  {
    io.error("error", "gps_rate_set");
  }
}

void cmd_gps_pmtk(Interaction &io, std::span<char *const>)
{
  const char *cmd = io.param_str(0);
  if (FEB_GPS_SendPMTKCommand(cmd) >= 0)
  {
    io.println("PMTK sent: %s", cmd);
  }
  else
  {
    io.error("error", "gps_pmtk_send");
  }
}

/* ============================================================================
 * WSS commands (Milestone 3)
 * ============================================================================ */

void cmd_wss(Interaction &io, std::span<char *const>)
{
  KVTable t(io, 12, 18, "Wheel Speed Sensors");
  t.row("Left", "%u.%02u mph", (unsigned)(left_mph_x100 / 100), (unsigned)(left_mph_x100 % 100));
  t.row("Right", "%u.%02u mph", (unsigned)(right_mph_x100 / 100), (unsigned)(right_mph_x100 % 100));
  t.row("Left dir", "%s", left_dir < 0 ? "REV" : (left_dir > 0 ? "FWD" : "STOP"));
  t.row("Right dir", "%s", right_dir < 0 ? "REV" : (right_dir > 0 ? "FWD" : "STOP"));
}

/* ============================================================================
 * Linear potentiometer commands (Milestone 3)
 * ============================================================================ */

void cmd_linpot(Interaction &io, std::span<char *const>)
{
  KVTable t(io, 12, 18, "Linear Potentiometers");
  t.row("Raw [0]", "%u", (unsigned)lp_raw[0]);
  t.row("Raw [1]", "%u", (unsigned)lp_raw[1]);
  t.row("Pos [0]", "%.2f mm", lp_position_mm[0]);
  t.row("Pos [1]", "%.2f mm", lp_position_mm[1]);
}

/* ============================================================================
 * Sensor die temperature commands (Milestone 3)
 * ============================================================================ */

void cmd_temps(Interaction &io, std::span<char *const>)
{
  KVTable t(io, 12, 18, "Sensor Die Temperatures");
  t.row("IMU", "%.2f C", imu_temp_c);
  t.row("MAG", "%.2f C", mag_temp_c);
}

/* ============================================================================
 * Command tree
 * ============================================================================ */

constexpr std::array<Command, 5> kImuSubcommands = {{
    {.name = "status", .description = "IMU init status + WHO_AM_I", .handler = cmd_imu_status},
    {.name = "accel", .description = "Acceleration X/Y/Z [mg]", .handler = cmd_imu_accel},
    {.name = "gyro", .description = "Angular rate X/Y/Z [mdps]", .handler = cmd_imu_gyro},
    {.name = "temp", .description = "IMU die temperature [C]", .handler = cmd_imu_temp},
    {.name = "all", .description = "Accel + gyro + temp", .handler = cmd_imu_all},
}};

constexpr std::array<Command, 4> kMagSubcommands = {{
    {.name = "status", .description = "MAG init status + WHO_AM_I", .handler = cmd_mag_status},
    {.name = "field", .description = "Magnetic field X/Y/Z [mG]", .handler = cmd_mag_field},
    {.name = "temp", .description = "MAG die temperature [C]", .handler = cmd_mag_temp},
    {.name = "all", .description = "Field + temp", .handler = cmd_mag_all},
}};

constexpr std::array<Param, 1> kGpsRateParams = {{param_int("hz", 1, 10)}};
constexpr std::array<Param, 1> kGpsPmtkParams = {{param_str("cmd")}};

constexpr std::array<Command, 10> kGpsSubcommands = {{
    {.name = "status", .description = "GPS status and fix info", .handler = cmd_gps_status},
    {.name = "pos", .description = "Position (lat, lon, alt)", .handler = cmd_gps_pos},
    {.name = "time", .description = "UTC time and date", .handler = cmd_gps_time},
    {.name = "speed", .description = "Speed and course", .handler = cmd_gps_speed},
    {.name = "sats", .description = "Satellite and DOP info", .handler = cmd_gps_sats},
    {.name = "all", .description = "All GPS info", .handler = cmd_gps_all},
    {.name = "enable", .description = "Enable GPS module", .handler = cmd_gps_enable},
    {.name = "disable", .description = "Disable GPS module", .handler = cmd_gps_disable},
    {.name = "rate", .description = "Set update rate (1|5|10 Hz)", .handler = cmd_gps_rate, .params = kGpsRateParams},
    {.name = "pmtk", .description = "Send raw PMTK command", .handler = cmd_gps_pmtk, .params = kGpsPmtkParams},
}};

constexpr std::array<Command, 6> kSensorCommands = {{
    group("imu", "IMU sensor commands", kImuSubcommands),
    group("mag", "Magnetometer commands", kMagSubcommands),
    group("gps", "GPS commands", kGpsSubcommands),
    {.name = "wss", .description = "Wheel speed sensors", .handler = cmd_wss},
    {.name = "linpot", .description = "Linear potentiometers", .handler = cmd_linpot},
    {.name = "temps", .description = "Sensor die temperatures", .handler = cmd_temps},
}};

inline constexpr auto kAll = concat(kSystemCommands, kSensorCommands);
static_assert(!has_duplicate_names(kAll), "duplicate console command name");
static_assert(params_fit(kAll), "a command declares more params than kMaxParams");

inline constexpr Console kConsole{FEB_UART_INSTANCE_1, kAll};

} // namespace

extern "C" void SENSOR_Console_ProcessLine(const char *line, size_t len)
{
  kConsole.process_line(line, len);
}
