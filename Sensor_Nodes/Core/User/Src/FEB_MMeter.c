#include "lis3mdl_reg.h"   //
#include "stm32f4xx_hal.h" //
#include "stm32f4xx_hal_def.h"
#include <stdio.h>
#include <string.h>
#include <FEB_MAIN.h>
#include <FEB_IMU.h>

stmdev_ctx_t lis3mdl_ctx;
extern I2C_HandleTypeDef hi2c3;
#define LIS3MDL_I2C_ADDR 0x1C

static int16_t data_raw_magnetic[3];
static float_t magnetic_mG[3];

// static uint8_t tx_buffer[1000];

int32_t lis3mdl_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len)
{
  return platform_read(handle, LIS3MDL_I2C_ADDR, reg, bufp, len); // TODO: adjust Devaddress
}
int32_t lis3mdl_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len)
{
  return platform_write(handle, LIS3MDL_I2C_ADDR, reg, bufp, len); // TODO: adjust Devaddress
}

void lis3mdl_init()
{
  lis3mdl_ctx.write_reg = lis3mdl_write;
  lis3mdl_ctx.read_reg = lis3mdl_read;
  lis3mdl_ctx.mdelay = HAL_Delay;
  lis3mdl_ctx.handle = &hi2c3;
}

void read_Magnetic_Field_Data()
{
  /* Read and Print Magnetic Field Data */
  memset(data_raw_magnetic, 0x00, 3 * sizeof(int16_t));
  lis3mdl_magnetic_raw_get(&lis3mdl_ctx, data_raw_magnetic);

  magnetic_mG[0] = 1000 * lis3mdl_from_fs16_to_gauss(data_raw_magnetic[0]);
  magnetic_mG[1] = 1000 * lis3mdl_from_fs16_to_gauss(data_raw_magnetic[1]);
  magnetic_mG[2] = 1000 * lis3mdl_from_fs16_to_gauss(data_raw_magnetic[2]);

  printf("Magnetic field [mG]: %4.2f\t%4.2f\t%4.2f\r\n", magnetic_mG[0], magnetic_mG[1], magnetic_mG[2]);
}
