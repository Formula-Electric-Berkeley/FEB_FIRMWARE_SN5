#include "lsm6dsox_reg.h"  //
#include "stm32f4xx_hal.h" //
#include "stm32f4xx_hal_def.h"

stmdev_ctx_t lsm6dsox_ctx;

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

void read_Acceleration() {}
