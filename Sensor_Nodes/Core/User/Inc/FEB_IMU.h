#ifndef IMU_H
#define IMU_H

#include "stm32f4xx_hal.h"
#include "lsm6dsox_reg.h"

/* Public initialization */
void lsm6dsox_init(void);

/* Sensor configuration */
void imu_config(void);

/* Read functions */
void read_Acceleration(void);
void read_Angular_Rate(void); // gyro

#endif /* IMU_H */
