#include "FEB_Fusion.h"
#include "FEB_IMU.h"          // acceleration_mg[], angular_rate_mdps[]
#include "FEB_Magnetometer.h" // magnetic_mG[]
#include "Fusion.h"
#include <math.h>
#include <string.h>
#include "feb_console.h"

// https://github.com/xioTechnologies/Fusion/blob/main/Examples/C/Advanced/main.c
// ---------------------------------------------------------------------
// Calibration data (default = identity / zero)
// TODO: Replace with real calibration values from sensors
// ---------------------------------------------------------------------
// =====================================================================
// GYROSCOPE CALIBRATION
// =====================================================================
FusionMatrix gyroMisalignment = {.array = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
FusionVector gyroSensitivity = {.array = {1.0f, 1.0f, 1.0f}};
FusionVector gyroOffset = {.array = {0.0f, 0.0f, 0.0f}};
// ============================================================================
// ACCELEROMETER CALIBRATION
// ============================================================================
FusionMatrix accMisalignment = {.array = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
FusionVector accSensitivity = {.array = {1.0f, 1.0f, 1.0f}};
FusionVector accOffset = {.array = {0.0f, 0.0f, 0.0f}};
// ============================================================================
// MAGNETOMETER CALIBRATION
// ============================================================================
FusionMatrix softIron = {.array = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
FusionVector hardIron = {.array = {0.0f, 0.0f, 0.0f}};

// ---------------------------------------------------------------------
// Sensor fusion instances
// ---------------------------------------------------------------------
static FusionAhrs ahrs;
static FusionBias bias;
static bool initialized = false;

// Settings
#define SAMPLE_RATE_HZ (104)     // ODR_104Hz in imu.c
#define GYRO_RANGE_DPS (2000.0f) // LSM6DSOX 2000 dps // TODO

// ---------------------------------------------------------------------
// INIT
// ---------------------------------------------------------------------
void FEB_Fusion_Init(void)
{
  // AHRS
  FusionAhrsInitialise(&ahrs);
  const FusionAhrsSettings settings = {
      .convention = FusionConventionNwu, // X north, Y west, Z up // TODO
      .gain = 0.5f,                      // TODO
      .gyroscopeRange = GYRO_RANGE_DPS,
      .accelerationRejection = 10.0f,              // TODO
      .magneticRejection = 10.0f,                  // TODO
      .recoveryTriggerPeriod = 5 * SAMPLE_RATE_HZ, /* 5 seconds */
  };
  FusionAhrsSetSettings(&ahrs, &settings);

  // Instantiate bias algorithm

  FusionBiasInitialise(&bias);
  FusionBiasSettings biasSettings = fusionBiasDefaultSettings;
  biasSettings.sampleRate = SAMPLE_RATE_HZ;
  FusionBiasSetSettings(&bias, &biasSettings);

  initialized = true;
}

// ---------------------------------------------------------------------
// Update
// - dt = time since last call (seconds)
// ---------------------------------------------------------------------
void FEB_Fusion_Update(float dt)
{
  if (!initialized)
    return;
  if (dt <= 0.0f || dt > 0.1f)
    dt = 0.01f;

  // 1. Read raw data from global vars (mg, mdps, mG)
  FusionVector gyro_raw = {.array = {angular_rate_mdps[0], angular_rate_mdps[1], angular_rate_mdps[2]}};
  FusionVector acc_raw = {.array = {acceleration_mg[0], acceleration_mg[1], acceleration_mg[2]}};
  FusionVector mag_raw = {.array = {magnetic_mG[0], magnetic_mG[1], magnetic_mG[2]}};

  // 2. calibrate
  FusionVector gyro = FusionModelInertial(gyro_raw, gyroMisalignment, gyroSensitivity, gyroOffset);
  FusionVector acc = FusionModelInertial(acc_raw, accMisalignment, accSensitivity, accOffset);
  FusionVector mag = FusionModelMagnetic(mag_raw, softIron, hardIron);

  // 3. Update bias algorithm
  gyro = FusionBiasUpdate(&bias, gyro);

  // 4. Update AHRS algorithm
  FusionAhrsUpdate(&ahrs, gyro, acc, mag, dt);

  // Print
  const FusionEuler euler = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));
  const FusionVector earth = FusionAhrsGetEarthAcceleration(&ahrs);
  FEB_Console_Printf("Roll %0.1f, Pitch %0.1f, Yaw %0.1f, X %0.1f, Y %0.1f, Z %0.1f\n", euler.angle.roll,
                     euler.angle.pitch, euler.angle.yaw, earth.axis.x, earth.axis.y, earth.axis.z);
}

void FEB_Fusion_GetQuaternion(float q[4])
{
  if (!initialized)
  {
    return;
  }
  const FusionQuaternion quat = FusionAhrsGetQuaternion(&ahrs);
  q[0] = quat.element.w; // w
  q[1] = quat.element.x; // x
  q[2] = quat.element.y; // y
  q[3] = quat.element.z; // z
}
