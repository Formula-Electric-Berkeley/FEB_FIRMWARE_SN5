#ifndef IMU_H
#define IMU_H

#include "stm32f4xx_hal.h"
#include "lsm6dsox_reg.h"

extern I2C_HandleTypeDef hi2c3;
extern stmdev_ctx_t lsm6dsox_ctx;

/* Both sensors share the I2C3 platform layer below, so their addresses live
 * here rather than being defined twice in the two .c files. */
#define LSM6DSOX_I2C_ADDR 0x6A
#define LIS3MDL_I2C_ADDR 0x1C

/* Cumulative I2C3 transaction failures, attributed by device address. The
 * read_* functions return void, so these counters are the only way to tell a
 * chip that has fallen off the bus from one that is merely reading zero. */
extern volatile uint32_t imu_bus_error_count;
extern volatile uint32_t mag_bus_error_count;

extern int16_t data_raw_acceleration[3];
extern float_t acceleration_mg[3];

extern int16_t data_raw_angular_rate[3];
extern float_t angular_rate_mdps[3];

extern int16_t data_raw_imu_temperature;
extern float_t imu_temp_c;

/* Public initialization - returns 0 on success, negative on failure */
int lsm6dsox_init(void);

/* Read functions */
void read_Acceleration(void);
void read_Angular_Rate(void); // gyro
void read_IMU_Temperature(void);

// platform sharing
int32_t platform_write(void *handle, uint8_t devaddress, uint8_t reg, const uint8_t *bufp, uint16_t len);
int32_t platform_read(void *handle, uint8_t devaddress, uint8_t reg, uint8_t *bufp, uint16_t len);

#endif /* IMU_H */
