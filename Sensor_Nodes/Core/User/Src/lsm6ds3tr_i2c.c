#include "lsm6ds3tr_i2c.h"

/* I2C timeout in milliseconds */
#define I2C_TIMEOUT 100

/* Helper function to write a single register */
bool LSM6DS3TR_WriteRegister(LSM6DS3TR_t *dev, uint8_t reg, uint8_t value)
{
  HAL_StatusTypeDef status;
  status = HAL_I2C_Mem_Write(dev->hi2c, dev->i2c_address, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, I2C_TIMEOUT);
  return (status == HAL_OK);
}

/* Helper function to read a single register */
bool LSM6DS3TR_ReadRegister(LSM6DS3TR_t *dev, uint8_t reg, uint8_t *value)
{
  HAL_StatusTypeDef status;
  status = HAL_I2C_Mem_Read(dev->hi2c, dev->i2c_address, reg, I2C_MEMADD_SIZE_8BIT, value, 1, I2C_TIMEOUT);
  return (status == HAL_OK);
}

/* Helper function to read multiple registers */
bool LSM6DS3TR_ReadRegisters(LSM6DS3TR_t *dev, uint8_t reg, uint8_t *buffer, uint16_t len)
{
  HAL_StatusTypeDef status;
  status = HAL_I2C_Mem_Read(dev->hi2c, dev->i2c_address, reg, I2C_MEMADD_SIZE_8BIT, buffer, len, I2C_TIMEOUT);
  return (status == HAL_OK);
}

/* Read WHO_AM_I register */
bool LSM6DS3TR_ReadWhoAmI(LSM6DS3TR_t *dev, uint8_t *who_am_i)
{
  return LSM6DS3TR_ReadRegister(dev, LSM6DS3TR_WHO_AM_I, who_am_i);
}

/* Initialize the sensor with default settings */
bool LSM6DS3TR_Init(LSM6DS3TR_t *dev, I2C_HandleTypeDef *hi2c)
{
  uint8_t who_am_i;

  /* Store I2C handle and address */
  dev->hi2c = hi2c;
  dev->i2c_address = LSM6DS3TR_I2C_ADDR;

  /* Wait for sensor to be ready */
  HAL_Delay(10);

  /* Check WHO_AM_I register */
  if (!LSM6DS3TR_ReadWhoAmI(dev, &who_am_i))
  {
    return false;
  }

  if (who_am_i != LSM6DS3TR_ID)
  {
    return false; // Wrong sensor ID
  }

  /* Software reset */
  if (!LSM6DS3TR_WriteRegister(dev, LSM6DS3TR_CTRL3_C, 0x01))
  {
    return false;
  }

  HAL_Delay(10); // Wait for reset to complete

  /* Enable Block Data Update (BDU) */
  if (!LSM6DS3TR_WriteRegister(dev, LSM6DS3TR_CTRL3_C, 0x40))
  {
    return false;
  }

  /* Configure accelerometer: 104 Hz, ±2g (default) */
  if (!LSM6DS3TR_ConfigAccel(dev, LSM6DS3TR_XL_ODR_104Hz, LSM6DS3TR_XL_FS_2g))
  {
    return false;
  }

  /* Configure gyroscope: 104 Hz, ±250 dps (default) */
  if (!LSM6DS3TR_ConfigGyro(dev, LSM6DS3TR_G_ODR_104Hz, LSM6DS3TR_G_FS_250dps))
  {
    return false;
  }

  return true;
}

/* Configure accelerometer */
bool LSM6DS3TR_ConfigAccel(LSM6DS3TR_t *dev, LSM6DS3TR_XL_ODR_t odr, LSM6DS3TR_XL_FS_t fs)
{
  uint8_t ctrl1_xl = odr | fs;

  /* Set sensitivity based on full scale */
  switch (fs)
  {
  case LSM6DS3TR_XL_FS_2g:
    dev->accel_sensitivity = 0.061f; // mg/LSB
    break;
  case LSM6DS3TR_XL_FS_4g:
    dev->accel_sensitivity = 0.122f;
    break;
  case LSM6DS3TR_XL_FS_8g:
    dev->accel_sensitivity = 0.244f;
    break;
  case LSM6DS3TR_XL_FS_16g:
    dev->accel_sensitivity = 0.488f;
    break;
  default:
    dev->accel_sensitivity = 0.061f;
    break;
  }

  return LSM6DS3TR_WriteRegister(dev, LSM6DS3TR_CTRL1_XL, ctrl1_xl);
}

/* Configure gyroscope */
bool LSM6DS3TR_ConfigGyro(LSM6DS3TR_t *dev, LSM6DS3TR_G_ODR_t odr, LSM6DS3TR_G_FS_t fs)
{
  uint8_t ctrl2_g = odr | fs;

  /* Set sensitivity based on full scale */
  switch (fs)
  {
  case LSM6DS3TR_G_FS_125dps:
    dev->gyro_sensitivity = 4.375f; // mdps/LSB
    break;
  case LSM6DS3TR_G_FS_250dps:
    dev->gyro_sensitivity = 8.75f;
    break;
  case LSM6DS3TR_G_FS_500dps:
    dev->gyro_sensitivity = 17.50f;
    break;
  case LSM6DS3TR_G_FS_1000dps:
    dev->gyro_sensitivity = 35.0f;
    break;
  case LSM6DS3TR_G_FS_2000dps:
    dev->gyro_sensitivity = 70.0f;
    break;
  default:
    dev->gyro_sensitivity = 8.75f;
    break;
  }

  return LSM6DS3TR_WriteRegister(dev, LSM6DS3TR_CTRL2_G, ctrl2_g);
}

/* Read raw accelerometer data */
bool LSM6DS3TR_ReadAccelRaw(LSM6DS3TR_t *dev, LSM6DS3TR_RawData_t *data)
{
  uint8_t buffer[6];

  if (!LSM6DS3TR_ReadRegisters(dev, LSM6DS3TR_OUTX_L_XL, buffer, 6))
  {
    return false;
  }

  /* Combine low and high bytes (little endian) */
  data->x = (int16_t)((buffer[1] << 8) | buffer[0]);
  data->y = (int16_t)((buffer[3] << 8) | buffer[2]);
  data->z = (int16_t)((buffer[5] << 8) | buffer[4]);

  return true;
}

/* Read raw gyroscope data */
bool LSM6DS3TR_ReadGyroRaw(LSM6DS3TR_t *dev, LSM6DS3TR_RawData_t *data)
{
  uint8_t buffer[6];

  if (!LSM6DS3TR_ReadRegisters(dev, LSM6DS3TR_OUTX_L_G, buffer, 6))
  {
    return false;
  }

  /* Combine low and high bytes (little endian) */
  data->x = (int16_t)((buffer[1] << 8) | buffer[0]);
  data->y = (int16_t)((buffer[3] << 8) | buffer[2]);
  data->z = (int16_t)((buffer[5] << 8) | buffer[4]);

  return true;
}

/* Read accelerometer data in g */
bool LSM6DS3TR_ReadAccel(LSM6DS3TR_t *dev, LSM6DS3TR_Data_t *data)
{
  LSM6DS3TR_RawData_t raw;

  if (!LSM6DS3TR_ReadAccelRaw(dev, &raw))
  {
    return false;
  }

  /* Convert to g (sensitivity is in mg/LSB, so divide by 1000) */
  data->x = (float)raw.x * dev->accel_sensitivity / 1000.0f;
  data->y = (float)raw.y * dev->accel_sensitivity / 1000.0f;
  data->z = (float)raw.z * dev->accel_sensitivity / 1000.0f;

  return true;
}

/* Read gyroscope data in dps (degrees per second) */
bool LSM6DS3TR_ReadGyro(LSM6DS3TR_t *dev, LSM6DS3TR_Data_t *data)
{
  LSM6DS3TR_RawData_t raw;

  if (!LSM6DS3TR_ReadGyroRaw(dev, &raw))
  {
    return false;
  }

  /* Convert to dps (sensitivity is in mdps/LSB, so divide by 1000) */
  data->x = (float)raw.x * dev->gyro_sensitivity / 1000.0f;
  data->y = (float)raw.y * dev->gyro_sensitivity / 1000.0f;
  data->z = (float)raw.z * dev->gyro_sensitivity / 1000.0f;

  return true;
}

/* Read temperature in degrees Celsius */
bool LSM6DS3TR_ReadTemperature(LSM6DS3TR_t *dev, float *temperature)
{
  uint8_t buffer[2];
  int16_t temp_raw;

  if (!LSM6DS3TR_ReadRegisters(dev, LSM6DS3TR_OUT_TEMP_L, buffer, 2))
  {
    return false;
  }

  /* Combine low and high bytes */
  temp_raw = (int16_t)((buffer[1] << 8) | buffer[0]);

  /* Convert to Celsius: 25°C + (value / 16) */
  *temperature = 25.0f + ((float)temp_raw / 16.0f);

  return true;
}

/* Check if new accelerometer data is available */
bool LSM6DS3TR_AccelDataAvailable(LSM6DS3TR_t *dev)
{
  uint8_t status;

  if (!LSM6DS3TR_ReadRegister(dev, LSM6DS3TR_STATUS_REG, &status))
  {
    return false;
  }

  return (status & 0x01) != 0; // Bit 0: XLDA (accelerometer data available)
}

/* Check if new gyroscope data is available */
bool LSM6DS3TR_GyroDataAvailable(LSM6DS3TR_t *dev)
{
  uint8_t status;

  if (!LSM6DS3TR_ReadRegister(dev, LSM6DS3TR_STATUS_REG, &status))
  {
    return false;
  }

  return (status & 0x02) != 0; // Bit 1: GDA (gyroscope data available)
}
