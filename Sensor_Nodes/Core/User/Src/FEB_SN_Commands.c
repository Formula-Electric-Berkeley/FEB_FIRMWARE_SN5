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
#include "feb_console.h"
#include "feb_string_utils.h"
#include "lsm6dsox_reg.h"
#include "lis3mdl_reg.h"
#include <string.h>

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

/* -------------------------------------------------------------------------- */
/*                         Command Descriptors                                */
/* -------------------------------------------------------------------------- */

static const FEB_Console_Cmd_t imu_cmd = {
    .name = "IMU",
    .help = "IMU sensor commands (IMU|status, IMU|accel, IMU|gyro, IMU|temp, IMU|all)",
    .handler = cmd_imu,
};

static const FEB_Console_Cmd_t mag_cmd = {
    .name = "MAG",
    .help = "Magnetometer commands (MAG|status, MAG|field, MAG|temp, MAG|all)",
    .handler = cmd_mag,
};

void SN_RegisterCommands(void)
{
  FEB_Console_Register(&imu_cmd);
  FEB_Console_Register(&mag_cmd);
}
