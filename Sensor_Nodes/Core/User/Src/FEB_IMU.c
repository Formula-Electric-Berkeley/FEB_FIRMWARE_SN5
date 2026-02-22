#include "lsm6dsox_reg.h"  //
#include "stm32f4xx_hal.h" //
#include "stm32f4xx_hal_def.h"
#include <stdio.h>
#include <string.h>

stmdev_ctx_t lsm6dsox_ctx;

static int16_t data_raw_acceleration[3];
static float_t acceleration_mg[3];

static int16_t data_raw_angular_rate[3];
static float_t angular_rate_mdps[3];

static uint8_t tx_buffer[1000];

int32_t platform_write(void *handle, uint8_t devaddress, uint8_t reg, const uint8_t *bufp, uint16_t len)
{
  HAL_StatusTypeDef ret;
  ret = HAL_I2C_Mem_Write(handle, devaddress << 1, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)bufp, len, HAL_MAX_DELAY);
  return (ret == HAL_OK) ? 0 : -1;
}

int32_t platform_read(void *handle, uint8_t devaddress, uint8_t reg, uint8_t *bufp, uint16_t len)
{
  HAL_StatusTypeDef ret;
  ret = HAL_I2C_Mem_Read(handle, devaddress << 1, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)bufp, len, HAL_MAX_DELAY);
  return (ret == HAL_OK) ? 0 : -1;
}

int32_t lsm6dsox_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len)
{
  return platform_read(handle, 0x47, reg, bufp, len); // TODO: adjust Devaddress
}
int32_t lsm6dsox_write(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len)
{
  return platform_write(handle, 0x47, reg, bufp, len); // TODO: adjust Devaddress
}

void lsm6dsox_init()
{
  lsm6dsox_ctx.write_reg = lsm6dsox_write;
  lsm6dsox_ctx.read_reg = lsm6dsox_read;
  lsm6dsox_ctx.mdelay = HAL_Delay;
  lsm6dsox_ctx.handle = &hi2c1;
}

void read_Acceleration()
{
  /* Read and print acceleration field data */
  memset(data_raw_acceleration, 0x00, 3 * sizeof(int16_t));
  lsm6dsox_acceleration_raw_get(&lsm6dsox_ctx, data_raw_acceleration);

  acceleration_mg[0] = lsm6dsox_from_fs2_to_mg(data_raw_acceleration[0]);
  acceleration_mg[1] = lsm6dsox_from_fs2_to_mg(data_raw_acceleration[1]);
  acceleration_mg[2] = lsm6dsox_from_fs2_to_mg(data_raw_acceleration[2]);

  snprintf((char *)tx_buffer, sizeof(tx_buffer), "Acceleration [mg]:%4.2f\t%4.2f\t%4.2f\r\n", acceleration_mg[0],
           acceleration_mg[1], acceleration_mg[2]);
}

void read_Angular_Rate()
{
  /* Read and print angular rate field data (gyro) */
  memset(data_raw_angular_rate, 0x00, 3 * sizeof(int16_t));
  lsm6dsox_angular_rate_raw_get(&lsm6dsox_ctx, data_raw_angular_rate);

  angular_rate_mdps[0] = lsm6dsox_from_fs2000_to_mdps(data_raw_angular_rate[0]);
  angular_rate_mdps[1] = lsm6dsox_from_fs2000_to_mdps(data_raw_angular_rate[1]);
  angular_rate_mdps[2] = lsm6dsox_from_fs2000_to_mdps(data_raw_angular_rate[2]);

  snprintf((char *)tx_buffer, sizeof(tx_buffer), "Angular rate [mdps]:%4.2f\t%4.2f\t%4.2f\r\n", angular_rate_mdps[0],
           angular_rate_mdps[1], angular_rate_mdps[2]);
}
