/**
 ******************************************************************************
 * @file           : FEB_Steering.c
 * @brief          : AS5600L magnetic steering encoder driver.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Steering.h"
#include <string.h>

/* ============================================================================
 * AS5600L Register Map
 * ============================================================================ */
#define AS5600L_REG_CONF ((uint8_t)0x07)
#define AS5600L_REG_RAW_ANGLE ((uint8_t)0x0C)
#define AS5600L_REG_ANGLE ((uint8_t)0x0E)
#define AS5600L_REG_STATUS ((uint8_t)0x0B)
#define AS5600L_REG_AGC ((uint8_t)0x1A)
#define AS5600L_REG_MAGNITUDE ((uint8_t)0x1B)

/* STATUS register bits [5:3] = MH, ML, MD — shift to [2:0] for CAN */
#define AS5600L_STATUS_MASK ((uint8_t)0x38)

/* No hysteresis, no slow filter */
#define AS5600L_CONF_VAL ((uint16_t)0x0300)

/* ============================================================================
 * Hardware
 * ============================================================================ */
extern I2C_HandleTypeDef hi2c3;

/* AS5600L default I2C address 0x36, shifted left for HAL 8-bit form. */
static const uint8_t I2C_ADDR = 0x36 << 1;
#define I2C_TIMEOUT_MS 5

/* ============================================================================
 * Globals (extern declared in FEB_Steering.h, consumed by FEB_CAN_Steering.c)
 * ============================================================================ */
uint16_t steer_angle = 0;
uint16_t steer_raw_angle = 0;
uint8_t steer_status = 0;
uint8_t steer_agc = 0;
uint16_t steer_magnitude = 0;

static bool initialized = false;

/* ============================================================================
 * Internal helpers
 * ============================================================================ */
static int steer_i2c_read(uint8_t reg, uint8_t *buf, uint8_t len)
{
  if (HAL_I2C_Master_Transmit(&hi2c3, I2C_ADDR, &reg, 1, I2C_TIMEOUT_MS) != HAL_OK)
    return -1;
  if (HAL_I2C_Master_Receive(&hi2c3, I2C_ADDR, buf, len, I2C_TIMEOUT_MS) != HAL_OK)
    return -1;
  return 0;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Write and verify the CONF register. Call once from FEB_Init().
 * @return true on success.
 */
bool FEB_Steering_Init(void)
{
  uint8_t cfg_buf[3] = {
      AS5600L_REG_CONF,
      (uint8_t)((AS5600L_CONF_VAL >> 8) & 0xFF),
      (uint8_t)(AS5600L_CONF_VAL & 0xFF),
  };
  if (HAL_I2C_Master_Transmit(&hi2c3, I2C_ADDR, cfg_buf, 3, I2C_TIMEOUT_MS) != HAL_OK)
    return false;

  uint8_t rb[2] = {0};
  if (steer_i2c_read(AS5600L_REG_CONF, rb, 2) != 0)
    return false;

  uint16_t readback = (uint16_t)(((uint16_t)(rb[0] & 0x3F) << 8) | rb[1]);
  if (readback != AS5600L_CONF_VAL)
    return false;

  initialized = true;
  return true;
}

/**
 * @brief Read all sensor values into globals. Call from FEB_Main_Loop().
 */
void FEB_Steering_Read(void)
{
  if (!initialized)
    return;

  uint8_t buf[2];

  if (steer_i2c_read(AS5600L_REG_ANGLE, buf, 2) == 0)
    steer_angle = (uint16_t)(((uint16_t)(buf[0] & 0x0F) << 8) | buf[1]);

  if (steer_i2c_read(AS5600L_REG_RAW_ANGLE, buf, 2) == 0)
    steer_raw_angle = (uint16_t)(((uint16_t)(buf[0] & 0x0F) << 8) | buf[1]);

  if (steer_i2c_read(AS5600L_REG_STATUS, buf, 1) == 0)
    steer_status = (uint8_t)((buf[0] & AS5600L_STATUS_MASK) >> 3);

  if (steer_i2c_read(AS5600L_REG_AGC, buf, 1) == 0)
    steer_agc = buf[0];

  if (steer_i2c_read(AS5600L_REG_MAGNITUDE, buf, 2) == 0)
    steer_magnitude = (uint16_t)(((uint16_t)(buf[0] & 0x0F) << 8) | buf[1]);
}
