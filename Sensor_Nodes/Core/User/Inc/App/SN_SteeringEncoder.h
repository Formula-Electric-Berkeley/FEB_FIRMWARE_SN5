/**
 ******************************************************************************
 * @file           : SN_SteeringEncoder.h
 * @brief          : Steering Encoder Driver
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef SN_STEERING_ENCODER_H
#define SN_STEERING_ENCODER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

  extern uint16_t steer_angle;
  extern uint16_t steer_raw_angle;
  extern uint8_t steer_status;
  extern uint8_t steer_agc;
  extern uint16_t steer_magnitude;
  extern bool steer_initialized;

  bool FEB_Steering_Init(void);
  void read_SteeringPosition(void);

#ifdef __cplusplus
}
#endif

#endif /* SN_STEERING_ENCODER_H */
