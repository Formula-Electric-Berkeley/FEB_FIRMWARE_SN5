#ifndef FEB_MAGNETOMETER_H
#define FEB_MAGNETOMETER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32f4xx_hal.h"
#include "lis3mdl_reg.h"
#include "FEB_IMU.h" // for shared platform_read / platform_write

  extern I2C_HandleTypeDef hi2c3;
  extern stmdev_ctx_t lis3mdl_ctx;

  /* Public initialization */
  void lis3mdl_init(void);

  /* Read functions */
  void read_Magnetic_Field_Data(void);

  /* Data access */
  extern int16_t data_raw_magnetometer[3];
  extern float magnetic_mG[3];

#ifdef __cplusplus
}
#endif

#endif /* FEB_MAGNETOMETER_H */
