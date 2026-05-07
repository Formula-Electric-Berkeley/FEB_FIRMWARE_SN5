#ifndef FEB_FUSION_H
#define FEB_FUSION_H

#include "Fusion.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// GYROSCOPE CALIBRATION
// ============================================================================
// Misalignment matrix (3x3, row-major) each row = output axis, each column = input axis.
// Identity: no cross-axis coupling.
extern FusionMatrix gyroMisalignment;
// Sensitivity [X,Y,Z] in dps per dps (unity = no rescaling)
extern FusionVector gyroSensitivity;
// Offset [X,Y,Z] in dps (average stationary reading)
extern FusionVector gyroOffset;

// ============================================================================
// ACCELEROMETER CALIBRATION
// ============================================================================
extern FusionMatrix accMisalignment;
extern FusionVector accSensitivity;
extern FusionVector accOffset;

// ============================================================================
// MAGNETOMETER CALIBRATION
// ============================================================================
extern FusionMatrix softIron;
extern FusionVector hardIron;

void FEB_Fusion_Init(void);

// One-time startup gyro bias capture: assumes the board is static for ~1 s.
// Blocks the caller. Writes gyroOffset. Call after IMU init, before main loop.
void FEB_Fusion_AutoCalibrate_Gyro(void);

// dt in seconds (use TIM5 microsecond delta for precision)
void FEB_Fusion_Update(float dt);

// Quaternion components in [-1, 1], order (w, x, y, z).
void FEB_Fusion_GetQuaternion(float q[4]);

// Euler angles in degrees: [roll, pitch, yaw]
void FEB_Fusion_GetEuler(float euler[3]);

// Linear acceleration with gravity removed, body frame, milli-g
void FEB_Fusion_GetLinearAcceleration_mg(float a_mg[3]);

// Linear acceleration with gravity removed, earth frame, milli-g
void FEB_Fusion_GetEarthAcceleration_mg(float a_mg[3]);

// Internal flags from FusionAhrsGetFlags
void FEB_Fusion_GetFlags(FusionAhrsFlags *flags);

// Internal states (rejection error magnitudes etc.)
void FEB_Fusion_GetInternalStates(FusionAhrsInternalStates *states);

#endif
