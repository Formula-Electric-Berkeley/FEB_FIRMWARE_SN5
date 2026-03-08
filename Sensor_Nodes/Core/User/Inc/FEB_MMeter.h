#ifndef MMETER_H
#define MMETER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32f4xx_hal.h"
#include "lis3mdl_reg.h"
#include "FEB_IMU.h" // for shared platform_read / platform_write

  extern I2C_HandleTypeDef hi2c1;

  /* Public initialization */
  void lis3mdl_init(void);

  /* Read functions */
  void read_Magnetic_Field_Data(void);

  /* Data access */
  extern float magnetic_mG[3];

#ifdef __cplusplus
}
#endif

#endif /* MMETER_H */
