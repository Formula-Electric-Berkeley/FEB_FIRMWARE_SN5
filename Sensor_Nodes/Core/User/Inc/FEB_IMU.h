#ifndef IMU_H
#define IMU_H

#include "stm32f4xx_hal.h"
#include "lsm6dsox_reg.h"

extern I2C_HandleTypeDef hi2c3;
extern stmdev_ctx_t lsm6dsox_ctx;

extern int16_t data_raw_acceleration[3];
extern float_t acceleration_mg[3];

extern int16_t data_raw_angular_rate[3];
extern float_t angular_rate_mdps[3];

/* Public initialization - returns 0 on success, negative on failure */
int lsm6dsox_init(void);

/* Read functions */
void read_Acceleration(void);
void read_Angular_Rate(void); // gyro

// platform sharing
int32_t platform_write(void *handle, uint8_t devaddress, uint8_t reg, const uint8_t *bufp, uint16_t len);
int32_t platform_read(void *handle, uint8_t devaddress, uint8_t reg, uint8_t *bufp, uint16_t len);

#endif /* IMU_H */
