#ifndef LSM6DS3TR_I2C_H
#define LSM6DS3TR_I2C_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* LSM6DS3TR-C I2C Address */
// If SDO/SA0 pin is connected to GND: 0x6A
// If SDO/SA0 pin is connected to VDD: 0x6B
#define LSM6DS3TR_I2C_ADDR (0x6A << 1) // 7-bit address shifted for HAL

/* Register Addresses */
#define LSM6DS3TR_WHO_AM_I 0x0F
#define LSM6DS3TR_CTRL1_XL 0x10
#define LSM6DS3TR_CTRL2_G 0x11
#define LSM6DS3TR_CTRL3_C 0x12
#define LSM6DS3TR_CTRL6_C 0x15
#define LSM6DS3TR_CTRL7_G 0x16
#define LSM6DS3TR_CTRL8_XL 0x17
#define LSM6DS3TR_STATUS_REG 0x1E
#define LSM6DS3TR_OUT_TEMP_L 0x20
#define LSM6DS3TR_OUT_TEMP_H 0x21
#define LSM6DS3TR_OUTX_L_G 0x22
#define LSM6DS3TR_OUTX_H_G 0x23
#define LSM6DS3TR_OUTY_L_G 0x24
#define LSM6DS3TR_OUTY_H_G 0x25
#define LSM6DS3TR_OUTZ_L_G 0x26
#define LSM6DS3TR_OUTZ_H_G 0x27
#define LSM6DS3TR_OUTX_L_XL 0x28
#define LSM6DS3TR_OUTX_H_XL 0x29
#define LSM6DS3TR_OUTY_L_XL 0x2A
#define LSM6DS3TR_OUTY_H_XL 0x2B
#define LSM6DS3TR_OUTZ_L_XL 0x2C
#define LSM6DS3TR_OUTZ_H_XL 0x2D

/* WHO_AM_I value */
#define LSM6DS3TR_ID 0x6A

/* Output Data Rate (ODR) for Accelerometer */
typedef enum
{
  LSM6DS3TR_XL_ODR_OFF = 0x00,    // Power down
  LSM6DS3TR_XL_ODR_12_5Hz = 0x10, // 12.5 Hz
  LSM6DS3TR_XL_ODR_26Hz = 0x20,   // 26 Hz
  LSM6DS3TR_XL_ODR_52Hz = 0x30,   // 52 Hz
  LSM6DS3TR_XL_ODR_104Hz = 0x40,  // 104 Hz
  LSM6DS3TR_XL_ODR_208Hz = 0x50,  // 208 Hz
  LSM6DS3TR_XL_ODR_416Hz = 0x60,  // 416 Hz
  LSM6DS3TR_XL_ODR_833Hz = 0x70,  // 833 Hz
  LSM6DS3TR_XL_ODR_1660Hz = 0x80, // 1.66 kHz
  LSM6DS3TR_XL_ODR_3330Hz = 0x90, // 3.33 kHz
  LSM6DS3TR_XL_ODR_6660Hz = 0xA0  // 6.66 kHz
} LSM6DS3TR_XL_ODR_t;

/* Full Scale for Accelerometer */
typedef enum
{
  LSM6DS3TR_XL_FS_2g = 0x00, // ±2g
  LSM6DS3TR_XL_FS_4g = 0x08, // ±4g
  LSM6DS3TR_XL_FS_8g = 0x0C, // ±8g
  LSM6DS3TR_XL_FS_16g = 0x04 // ±16g
} LSM6DS3TR_XL_FS_t;

/* Output Data Rate (ODR) for Gyroscope */
typedef enum
{
  LSM6DS3TR_G_ODR_OFF = 0x00,    // Power down
  LSM6DS3TR_G_ODR_12_5Hz = 0x10, // 12.5 Hz
  LSM6DS3TR_G_ODR_26Hz = 0x20,   // 26 Hz
  LSM6DS3TR_G_ODR_52Hz = 0x30,   // 52 Hz
  LSM6DS3TR_G_ODR_104Hz = 0x40,  // 104 Hz
  LSM6DS3TR_G_ODR_208Hz = 0x50,  // 208 Hz
  LSM6DS3TR_G_ODR_416Hz = 0x60,  // 416 Hz
  LSM6DS3TR_G_ODR_833Hz = 0x70,  // 833 Hz
  LSM6DS3TR_G_ODR_1660Hz = 0x80  // 1.66 kHz
} LSM6DS3TR_G_ODR_t;

/* Full Scale for Gyroscope */
typedef enum
{
  LSM6DS3TR_G_FS_125dps = 0x02,  // ±125 dps
  LSM6DS3TR_G_FS_250dps = 0x00,  // ±250 dps
  LSM6DS3TR_G_FS_500dps = 0x04,  // ±500 dps
  LSM6DS3TR_G_FS_1000dps = 0x08, // ±1000 dps
  LSM6DS3TR_G_FS_2000dps = 0x0C  // ±2000 dps
} LSM6DS3TR_G_FS_t;

/* Data structure for raw sensor data */
typedef struct
{
  int16_t x;
  int16_t y;
  int16_t z;
} LSM6DS3TR_RawData_t;

/* Data structure for converted sensor data */
typedef struct
{
  float x;
  float y;
  float z;
} LSM6DS3TR_Data_t;

/* LSM6DS3TR-C handle structure */
typedef struct
{
  I2C_HandleTypeDef *hi2c;
  uint8_t i2c_address;
  float accel_sensitivity; // mg/LSB
  float gyro_sensitivity;  // mdps/LSB
} LSM6DS3TR_t;

/* Function Prototypes */

/**
 * @brief Initialize LSM6DS3TR-C sensor
 * @param dev: Pointer to LSM6DS3TR_t device structure
 * @param hi2c: Pointer to I2C handle
 * @return true if successful, false otherwise
 */
bool LSM6DS3TR_Init(LSM6DS3TR_t *dev, I2C_HandleTypeDef *hi2c);

/**
 * @brief Read WHO_AM_I register
 * @param dev: Pointer to device structure
 * @param who_am_i: Pointer to store WHO_AM_I value
 * @return true if successful, false otherwise
 */
bool LSM6DS3TR_ReadWhoAmI(LSM6DS3TR_t *dev, uint8_t *who_am_i);

/**
 * @brief Configure accelerometer
 * @param dev: Pointer to device structure
 * @param odr: Output data rate
 * @param fs: Full scale range
 * @return true if successful, false otherwise
 */
bool LSM6DS3TR_ConfigAccel(LSM6DS3TR_t *dev, LSM6DS3TR_XL_ODR_t odr, LSM6DS3TR_XL_FS_t fs);

/**
 * @brief Configure gyroscope
 * @param dev: Pointer to device structure
 * @param odr: Output data rate
 * @param fs: Full scale range
 * @return true if successful, false otherwise
 */
bool LSM6DS3TR_ConfigGyro(LSM6DS3TR_t *dev, LSM6DS3TR_G_ODR_t odr, LSM6DS3TR_G_FS_t fs);

/**
 * @brief Read raw accelerometer data
 * @param dev: Pointer to device structure
 * @param data: Pointer to store raw data
 * @return true if successful, false otherwise
 */
bool LSM6DS3TR_ReadAccelRaw(LSM6DS3TR_t *dev, LSM6DS3TR_RawData_t *data);

/**
 * @brief Read raw gyroscope data
 * @param dev: Pointer to device structure
 * @param data: Pointer to store raw data
 * @return true if successful, false otherwise
 */
bool LSM6DS3TR_ReadGyroRaw(LSM6DS3TR_t *dev, LSM6DS3TR_RawData_t *data);

/**
 * @brief Read accelerometer data in g
 * @param dev: Pointer to device structure
 * @param data: Pointer to store data in g
 * @return true if successful, false otherwise
 */
bool LSM6DS3TR_ReadAccel(LSM6DS3TR_t *dev, LSM6DS3TR_Data_t *data);

/**
 * @brief Read gyroscope data in dps (degrees per second)
 * @param dev: Pointer to device structure
 * @param data: Pointer to store data in dps
 * @return true if successful, false otherwise
 */
bool LSM6DS3TR_ReadGyro(LSM6DS3TR_t *dev, LSM6DS3TR_Data_t *data);

/**
 * @brief Read temperature in degrees Celsius
 * @param dev: Pointer to device structure
 * @param temperature: Pointer to store temperature value
 * @return true if successful, false otherwise
 */
bool LSM6DS3TR_ReadTemperature(LSM6DS3TR_t *dev, float *temperature);

/**
 * @brief Check if new accelerometer data is available
 * @param dev: Pointer to device structure
 * @return true if new data available, false otherwise
 */
bool LSM6DS3TR_AccelDataAvailable(LSM6DS3TR_t *dev);

/**
 * @brief Check if new gyroscope data is available
 * @param dev: Pointer to device structure
 * @return true if new data available, false otherwise
 */
bool LSM6DS3TR_GyroDataAvailable(LSM6DS3TR_t *dev);

/* Low-level register access functions */
bool LSM6DS3TR_WriteRegister(LSM6DS3TR_t *dev, uint8_t reg, uint8_t value);
bool LSM6DS3TR_ReadRegister(LSM6DS3TR_t *dev, uint8_t reg, uint8_t *value);
bool LSM6DS3TR_ReadRegisters(LSM6DS3TR_t *dev, uint8_t reg, uint8_t *buffer, uint16_t len);

#endif /* LSM6DS3TR_I2C_H */
