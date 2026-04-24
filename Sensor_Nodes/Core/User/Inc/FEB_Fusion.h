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
// Sensitivity [X,Y,Z] in mdps per LSB
extern FusionVector gyroSensitivity;
// Offset [X,Y,Z] in mdps (average stationary reading)
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

void FEB_Fusion_Update(float dt);

// Quaternion (w,x, y, z)
void FEB_Fusion_GetQuaternion(float q[4]);

#endif
