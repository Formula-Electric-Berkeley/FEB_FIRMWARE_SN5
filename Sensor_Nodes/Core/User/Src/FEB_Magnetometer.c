#include "lis3mdl_reg.h"   //
#include "stm32f4xx_hal.h" //
#include "stm32f4xx_hal_def.h"
#include <string.h>
#include "FEB_Main.h"
#include "FEB_IMU.h"

#include "feb_log.h"

#define TAG_MAG "[MAG]"

stmdev_ctx_t lis3mdl_ctx;
extern I2C_HandleTypeDef hi2c3;
#define LIS3MDL_I2C_ADDR 0x1C

int16_t data_raw_magnetometer[3];
float_t magnetic_mG[3];

int16_t data_raw_mag_temperature;
float_t mag_temp_c;

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

  HAL_Delay(10);

  uint8_t whoamI;
  lis3mdl_device_id_get(&lis3mdl_ctx, &whoamI);
  if (whoamI != LIS3MDL_ID)
  {
    LOG_E(TAG_MAG, "Magnetometer not found (WHO_AM_I: 0x%02X)", whoamI);
    return;
  }

  lis3mdl_block_data_update_set(&lis3mdl_ctx, PROPERTY_ENABLE);
  lis3mdl_temperature_meas_set(&lis3mdl_ctx, PROPERTY_ENABLE);
  lis3mdl_data_rate_set(&lis3mdl_ctx, LIS3MDL_HP_80Hz);
  lis3mdl_full_scale_set(&lis3mdl_ctx, LIS3MDL_16_GAUSS);
  lis3mdl_operating_mode_set(&lis3mdl_ctx, LIS3MDL_CONTINUOUS_MODE);
}

void read_Magnetic_Field_Data()
{
  /* Read and Print Magnetic Field Data */
  memset(data_raw_magnetometer, 0x00, 3 * sizeof(int16_t));
  if (lis3mdl_magnetic_raw_get(&lis3mdl_ctx, data_raw_magnetometer) != 0)
  {
    LOG_E(TAG_MAG, "Mag read failed");
    return;
  }

  magnetic_mG[0] = 1000 * lis3mdl_from_fs16_to_gauss(data_raw_magnetometer[0]);
  magnetic_mG[1] = 1000 * lis3mdl_from_fs16_to_gauss(data_raw_magnetometer[1]);
  magnetic_mG[2] = 1000 * lis3mdl_from_fs16_to_gauss(data_raw_magnetometer[2]);

  // LOG_D(TAG_MAG, "Magnetic field [mG]: %4.2f\t%4.2f\t%4.2f", magnetic_mG[0], magnetic_mG[1], magnetic_mG[2]);
}

void read_Mag_Temperature(void)
{
  data_raw_mag_temperature = 0;
  if (lis3mdl_temperature_raw_get(&lis3mdl_ctx, &data_raw_mag_temperature) != 0)
  {
    LOG_E(TAG_MAG, "Temp read failed");
    mag_temp_c = 0.0f;
    return;
  }
  mag_temp_c = lis3mdl_from_lsb_to_celsius(data_raw_mag_temperature);
}
