/**
 ******************************************************************************
 * @file           : FEB_CAN_Fusion.h
 * @brief          : CAN reporter for Fusion AHRS outputs.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_FUSION_H
#define FEB_CAN_FUSION_H

/**
 * Pack and transmit Fusion AHRS results onto CAN1:
 *   0x47 fusion_quaternion_data, 0x48 fusion_euler_data,
 *   0x49 fusion_linear_accel_data, 0x4A fusion_earth_accel_data,
 *   0x4B fusion_status_data.
 */
void FEB_CAN_Fusion_Tick(void);

#endif /* FEB_CAN_FUSION_H */
