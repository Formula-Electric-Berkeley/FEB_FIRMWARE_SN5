#include "FEB_Fusion.h"
#include "FEB_IMU.h"          // acceleration_mg[], angular_rate_mdps[]
#include "FEB_Magnetometer.h" // magnetic_mG[]
#include "Fusion.h"
#include "stm32f4xx_hal.h" // HAL_Delay for startup gyro cal
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

#define SAMPLE_RATE_HZ (1000)    // matches FEB_Main_Loop IMU tick (1 ms)
#define GYRO_RANGE_DPS (2000.0f) // LSM6DSOX FS=2000dps

/* ---------------------------------------------------------------------
 * Online magnetometer calibration
 *
 * Hard-iron and soft-iron are body-frame, mounting-dependent. We can't
 * measure them at startup (the car is parked) but we can refine them
 * during normal driving by tracking per-axis min/max of incoming mag
 * samples. Initialized to identity at boot; converges to a sensible
 * calibration after ~30-60 s of varied yaw motion.
 * --------------------------------------------------------------------- */
#define MAG_CAL_UPDATE_PERIOD 100u // refine every 100 fusion ticks (100 ms @ 1 kHz)
#define MAG_CAL_MIN_SPAN_mG 100.0f // skip update until per-axis span exceeds this
static float mag_min[3] = {1.0e9f, 1.0e9f, 1.0e9f};
static float mag_max[3] = {-1.0e9f, -1.0e9f, -1.0e9f};
static uint32_t mag_cal_tick_count = 0;

static void update_mag_cal_online(const FusionVector raw_mg)
{
  for (int i = 0; i < 3; i++)
  {
    if (raw_mg.array[i] < mag_min[i])
      mag_min[i] = raw_mg.array[i];
    if (raw_mg.array[i] > mag_max[i])
      mag_max[i] = raw_mg.array[i];
  }
  if ((++mag_cal_tick_count % MAG_CAL_UPDATE_PERIOD) != 0u)
    return;

  const float span_x = mag_max[0] - mag_min[0];
  const float span_y = mag_max[1] - mag_min[1];
  const float span_z = mag_max[2] - mag_min[2];
  if (span_x < MAG_CAL_MIN_SPAN_mG || span_y < MAG_CAL_MIN_SPAN_mG || span_z < MAG_CAL_MIN_SPAN_mG)
    return;

  hardIron.array[0] = 0.5f * (mag_max[0] + mag_min[0]);
  hardIron.array[1] = 0.5f * (mag_max[1] + mag_min[1]);
  hardIron.array[2] = 0.5f * (mag_max[2] + mag_min[2]);

  const float span_avg = (span_x + span_y + span_z) / 3.0f;
  softIron.array[0] = span_avg / span_x;
  softIron.array[4] = span_avg / span_y;
  softIron.array[8] = span_avg / span_z;
}

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

void FEB_Fusion_AutoCalibrate_Gyro(void)
{
  /* 1 second of static samples at 1 ms per sample. The car must be still during
   * boot (it is, by definition — this runs before the main loop). If motion is
   * present the offset will be wrong; FusionBias will refine it during operation. */
  const uint32_t SAMPLES = 1000u;
  double sx = 0.0, sy = 0.0, sz = 0.0;

  for (uint32_t i = 0; i < SAMPLES; i++)
  {
    read_Angular_Rate();
    sx += angular_rate_mdps[0];
    sy += angular_rate_mdps[1];
    sz += angular_rate_mdps[2];
    HAL_Delay(1);
  }

  /* Driver gives mdps; FusionVector expects dps. */
  gyroOffset.array[0] = (float)((sx / (double)SAMPLES) * 0.001);
  gyroOffset.array[1] = (float)((sy / (double)SAMPLES) * 0.001);
  gyroOffset.array[2] = (float)((sz / (double)SAMPLES) * 0.001);
}

void FEB_Fusion_Update(float dt)
{
  if (!initialized)
    return;
  if (dt <= 1e-5f || dt > 0.005f)
    dt = 0.001f;

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

  /* Refine hardIron/softIron based on running min/max before applying. */
  update_mag_cal_online(mag_raw);

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
