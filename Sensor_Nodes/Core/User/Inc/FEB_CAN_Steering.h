/**
 ******************************************************************************
 * @file           : FEB_CAN_Steering.h
 * @brief          : CAN reporter for AS5600L steering encoder.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Frame FEB_CAN_STEER_ANGLE_DATA_FRAME_ID  — steer_angle_data:
 *   [0..1]  angle      uint16  12-bit filtered angle    (0–4095, LSB = 0.0879°)
 *   [2..3]  raw_angle  uint16  12-bit raw angle         (0–4095, LSB = 0.0879°)
 *   [4]     agc        uint8   AGC gain                 (0–255)
 *
 * Frame FEB_CAN_STEER_STATUS_DATA_FRAME_ID — steer_status_data:
 *   [0]     status     uint8   magnet flags (bit0=MH, bit1=ML, bit2=MD)
 *   [1..2]  magnitude  uint16  12-bit CORDIC magnitude  (0–4095)
 *
 ******************************************************************************
 */

#ifndef FEB_CAN_STEERING_H
#define FEB_CAN_STEERING_H

void FEB_CAN_Steering_Tick(void);

#endif /* FEB_CAN_STEERING_H */
