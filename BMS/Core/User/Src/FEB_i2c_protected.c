#include "FEB_i2c_protected.h"
#include "cmsis_os2.h" // for osMutexAcquire / osMutexRelease
#include <stm32f4xx_hal_i2c.h>

extern osMutexId_t FEB_I2C_MutexHandle;

// ---------------- WRAPPER FUNCTIONS -----------------

HAL_StatusTypeDef FEB_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size,
                                          uint32_t Timeout)
{
  HAL_StatusTypeDef status;

  osMutexAcquire(FEB_I2C_MutexHandle, osWaitForever);
  status = HAL_I2C_Master_Transmit(hi2c, DevAddress, pData, Size, Timeout);
  osMutexRelease(FEB_I2C_MutexHandle);

  return status;
}

HAL_StatusTypeDef FEB_I2C_Master_Receive(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size,
                                         uint32_t Timeout)
{
  HAL_StatusTypeDef status;

  osMutexAcquire(FEB_I2C_MutexHandle, osWaitForever);
  status = HAL_I2C_Master_Receive(hi2c, DevAddress, pData, Size, Timeout);
  osMutexRelease(FEB_I2C_MutexHandle);

  return status;
}

HAL_StatusTypeDef FEB_I2C_Slave_Transmit(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  HAL_StatusTypeDef status;

  osMutexAcquire(FEB_I2C_MutexHandle, osWaitForever);
  status = HAL_I2C_Slave_Transmit(hi2c, pData, Size, Timeout);
  osMutexRelease(FEB_I2C_MutexHandle);

  return status;
}

HAL_StatusTypeDef FEB_I2C_Slave_Receive(I2C_HandleTypeDef *hi2c, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  HAL_StatusTypeDef status;

  osMutexAcquire(FEB_I2C_MutexHandle, osWaitForever);
  status = HAL_I2C_Slave_Receive(hi2c, pData, Size, Timeout);
  osMutexRelease(FEB_I2C_MutexHandle);

  return status;
}

HAL_StatusTypeDef FEB_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress,
                                    uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  HAL_StatusTypeDef status;

  osMutexAcquire(FEB_I2C_MutexHandle, osWaitForever);
  status = HAL_I2C_Mem_Write(hi2c, DevAddress, MemAddress, MemAddSize, pData, Size, Timeout);
  osMutexRelease(FEB_I2C_MutexHandle);

  return status;
}

HAL_StatusTypeDef FEB_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress,
                                   uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
  HAL_StatusTypeDef status;

  osMutexAcquire(FEB_I2C_MutexHandle, osWaitForever);
  status = HAL_I2C_Mem_Read(hi2c, DevAddress, MemAddress, MemAddSize, pData, Size, Timeout);
  osMutexRelease(FEB_I2C_MutexHandle);

  return status;
}
