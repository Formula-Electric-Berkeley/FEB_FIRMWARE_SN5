/**
 ******************************************************************************
 * @file           : FEB_CAN_Sensors.h
 * @brief          : CAN reporter for misc sensor health (temperatures, etc.).
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_SENSORS_H
#define FEB_CAN_SENSORS_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* Pack and TX 0x4C sensor_temps_data (IMU + magnetometer die temperature). */
  void FEB_CAN_Temps_Tick(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_SENSORS_H */
