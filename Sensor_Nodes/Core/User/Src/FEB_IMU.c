#include "lsm6dsox_reg.h"  //
#include "stm32f4xx_hal.h" //
#include "stm32f4xx_hal_def.h"
#include "FEB_IMU.h"
#include <stdio.h>
#include <string.h>
#include "FEB_Main.h"

stmdev_ctx_t lsm6dsox_ctx;
extern I2C_HandleTypeDef hi2c3;
#define LSM6DSOX_I2C_ADDR 0x6A
#define I2C_TIMEOUT_MS 100
extern UART_HandleTypeDef huart2;

static int16_t data_raw_acceleration[3];
static float_t acceleration_mg[3];

static int16_t data_raw_angular_rate[3];
static float_t angular_rate_mdps[3];

// static uint8_t tx_buffer[1000];

int32_t platform_write(void *handle, uint8_t devaddress, uint8_t reg, const uint8_t *bufp, uint16_t len)
{
  HAL_StatusTypeDef ret;
  ret = HAL_I2C_Mem_Write(handle, devaddress << 1, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)bufp, len, I2C_TIMEOUT_MS);
  return (ret == HAL_OK) ? 0 : -1;
}

int32_t platform_read(void *handle, uint8_t devaddress, uint8_t reg, uint8_t *bufp, uint16_t len)
{
  HAL_StatusTypeDef ret;
  ret = HAL_I2C_Mem_Read(handle, devaddress << 1, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)bufp, len, I2C_TIMEOUT_MS);
  return (ret == HAL_OK) ? 0 : -1;
}
// 0x6a or 0x6b

int32_t lsm6dsox_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len)
{
  return platform_read(handle, LSM6DSOX_I2C_ADDR, reg, bufp, len); // TODO: adjust Devaddress
}
int32_t lsm6dsox_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len)
{
  return platform_write(handle, LSM6DSOX_I2C_ADDR, reg, bufp, len); // TODO: adjust Devaddress
}

void i2c_scan(void)
{
  printf("Scanning I2C3...\r\n");
  for (uint8_t addr = 0x00; addr < 0x80; addr++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c3, addr << 1, 1, 10) == HAL_OK)
    {
      printf("  Device found at 0x%02X\r\n", addr);
    }
  }
  printf("Scan complete.\r\n");
}

void lsm6dsox_init(void)
{
  lsm6dsox_ctx.write_reg = lsm6dsox_write;
  lsm6dsox_ctx.read_reg = lsm6dsox_read;
  lsm6dsox_ctx.mdelay = HAL_Delay;
  lsm6dsox_ctx.handle = &hi2c3;

  i2c_scan();

  HAL_Delay(10);

  uint8_t whoamI;
  lsm6dsox_device_id_get(&lsm6dsox_ctx, &whoamI);
  if (whoamI != LSM6DSOX_ID)
  {
    printf("IMU not found (WHO_AM_I: 0x%02X)\r\n", whoamI);
    return;
  }

  lsm6dsox_reset_set(&lsm6dsox_ctx, PROPERTY_ENABLE);
  uint8_t rst;
  uint32_t timeout = 1000;
  do
  {
    lsm6dsox_reset_get(&lsm6dsox_ctx, &rst);
    if (--timeout == 0)
    {
      printf("IMU reset timeout\r\n");
      return;
    }
  } while (rst);

  lsm6dsox_block_data_update_set(&lsm6dsox_ctx, PROPERTY_ENABLE);

  lsm6dsox_xl_data_rate_set(&lsm6dsox_ctx, LSM6DSOX_XL_ODR_104Hz);
  lsm6dsox_xl_full_scale_set(&lsm6dsox_ctx, LSM6DSOX_2g);

  lsm6dsox_gy_data_rate_set(&lsm6dsox_ctx, LSM6DSOX_GY_ODR_104Hz);
  lsm6dsox_gy_full_scale_set(&lsm6dsox_ctx, LSM6DSOX_2000dps);
}

void read_Acceleration(void)
{
  /* Read and print acceleration field data */
  memset(data_raw_acceleration, 0x00, 3 * sizeof(int16_t));
  if (lsm6dsox_acceleration_raw_get(&lsm6dsox_ctx, data_raw_acceleration) != 0)
  {
    printf("Accel read failed\r\n");
    return;
  }

  acceleration_mg[0] = lsm6dsox_from_fs2_to_mg(data_raw_acceleration[0]);
  acceleration_mg[1] = lsm6dsox_from_fs2_to_mg(data_raw_acceleration[1]);
  acceleration_mg[2] = lsm6dsox_from_fs2_to_mg(data_raw_acceleration[2]);

  printf("Acceleration [mg]: %4.2f\t%4.2f\t%4.2f\r\n", acceleration_mg[0], acceleration_mg[1], acceleration_mg[2]);
}

void read_Angular_Rate(void)
{
  /* Read and print angular rate field data (gyro) */
  memset(data_raw_angular_rate, 0x00, 3 * sizeof(int16_t));
  if (lsm6dsox_angular_rate_raw_get(&lsm6dsox_ctx, data_raw_angular_rate) != 0)
  {
    printf("Gyro read failed\r\n");
    return;
  }

  angular_rate_mdps[0] = lsm6dsox_from_fs2000_to_mdps(data_raw_angular_rate[0]);
  angular_rate_mdps[1] = lsm6dsox_from_fs2000_to_mdps(data_raw_angular_rate[1]);
  angular_rate_mdps[2] = lsm6dsox_from_fs2000_to_mdps(data_raw_angular_rate[2]);

  printf("Angular Rate [mdps]: %4.2f\t%4.2f\t%4.2f\r\n", angular_rate_mdps[0], angular_rate_mdps[1],
         angular_rate_mdps[2]);
}
