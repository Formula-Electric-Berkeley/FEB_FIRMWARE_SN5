#include "FEB_Fusion.h"
#include "FEB_IMU.h"          // acceleration_mg[], angular_rate_mdps[]
#include "FEB_Magnetometer.h" // magnetic_mG[]
#include "Fusion.h"
#include <math.h>
#include <string.h>

// https://github.com/xioTechnologies/Fusion/blob/main/Examples/C/Advanced/main.c
// ---------------------------------------------------------------------
// Calibration data (default = identity / zero)
// TODO: Replace with real calibration values from sensors
// ---------------------------------------------------------------------
FusionMatrix gyroMisalignment = {.array = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
FusionVector gyroSensitivity = {.array = {1.0f, 1.0f, 1.0f}};
FusionVector gyroOffset = {.array = {0.0f, 0.0f, 0.0f}};

FusionMatrix accMisalignment = {.array = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
FusionVector accSensitivity = {.array = {1.0f, 1.0f, 1.0f}};
FusionVector accOffset = {.array = {0.0f, 0.0f, 0.0f}};

FusionMatrix softIron = {.array = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f}};
FusionVector hardIron = {.array = {0.0f, 0.0f, 0.0f}};

static FusionAhrs ahrs;
static FusionBias bias;
static bool initialized = false;

#define SAMPLE_RATE_HZ (100)     // matches FEB_Main_Loop IMU tick (10 ms)
#define GYRO_RANGE_DPS (2000.0f) // LSM6DSOX FS=2000dps

void FEB_Fusion_Init(void)
{
  FusionAhrsInitialise(&ahrs);
  const FusionAhrsSettings settings = {
      .convention = FusionConventionNwu, // X north, Y west, Z up
      .gain = 0.5f,
      .gyroscopeRange = GYRO_RANGE_DPS,
      .accelerationRejection = 10.0f,
      .magneticRejection = 10.0f,
      .recoveryTriggerPeriod = 5 * SAMPLE_RATE_HZ, // 5 seconds
  };
  FusionAhrsSetSettings(&ahrs, &settings);

  FusionBiasInitialise(&bias);
  FusionBiasSettings biasSettings = fusionBiasDefaultSettings;
  biasSettings.sampleRate = SAMPLE_RATE_HZ;
  FusionBiasSetSettings(&bias, &biasSettings);

  initialized = true;
}

void FEB_Fusion_Update(float dt)
{
  if (!initialized)
    return;
  if (dt <= 1e-4f || dt > 0.05f)
    dt = 0.01f;

  // FusionAhrsUpdate expects gyro in dps and accel in g; drivers publish mdps and mg.
  FusionVector gyro_raw = {.array = {
                               angular_rate_mdps[0] * 0.001f,
                               angular_rate_mdps[1] * 0.001f,
                               angular_rate_mdps[2] * 0.001f,
                           }};
  FusionVector acc_raw = {.array = {
                              acceleration_mg[0] * 0.001f,
                              acceleration_mg[1] * 0.001f,
                              acceleration_mg[2] * 0.001f,
                          }};
  FusionVector mag_raw = {.array = {magnetic_mG[0], magnetic_mG[1], magnetic_mG[2]}};

  FusionVector gyro = FusionModelInertial(gyro_raw, gyroMisalignment, gyroSensitivity, gyroOffset);
  FusionVector acc = FusionModelInertial(acc_raw, accMisalignment, accSensitivity, accOffset);
  FusionVector mag = FusionModelMagnetic(mag_raw, softIron, hardIron);

  gyro = FusionBiasUpdate(&bias, gyro);
  FusionAhrsUpdate(&ahrs, gyro, acc, mag, dt);
}

void FEB_Fusion_GetQuaternion(float q[4])
{
  if (!initialized)
    return;
  const FusionQuaternion quat = FusionAhrsGetQuaternion(&ahrs);
  q[0] = quat.element.w;
  q[1] = quat.element.x;
  q[2] = quat.element.y;
  q[3] = quat.element.z;
}

void FEB_Fusion_GetEuler(float euler[3])
{
  if (!initialized)
    return;
  const FusionEuler eu = FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));
  euler[0] = eu.angle.roll;
  euler[1] = eu.angle.pitch;
  euler[2] = eu.angle.yaw;
}

void FEB_Fusion_GetLinearAcceleration_mg(float a_mg[3])
{
  if (!initialized)
    return;
  const FusionVector v = FusionAhrsGetLinearAcceleration(&ahrs); // in g
  a_mg[0] = v.axis.x * 1000.0f;
  a_mg[1] = v.axis.y * 1000.0f;
  a_mg[2] = v.axis.z * 1000.0f;
}

void FEB_Fusion_GetEarthAcceleration_mg(float a_mg[3])
{
  if (!initialized)
    return;
  const FusionVector v = FusionAhrsGetEarthAcceleration(&ahrs); // in g
  a_mg[0] = v.axis.x * 1000.0f;
  a_mg[1] = v.axis.y * 1000.0f;
  a_mg[2] = v.axis.z * 1000.0f;
}

void FEB_Fusion_GetFlags(FusionAhrsFlags *flags)
{
  if (!initialized || flags == NULL)
    return;
  *flags = FusionAhrsGetFlags(&ahrs);
}

void FEB_Fusion_GetInternalStates(FusionAhrsInternalStates *states)
{
  if (!initialized || states == NULL)
    return;
  *states = FusionAhrsGetInternalStates(&ahrs);
}
