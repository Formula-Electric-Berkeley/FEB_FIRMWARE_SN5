/**
 * @file    FEB_RFM95_HW.h
 * @brief   RFM95 Hardware Abstraction Layer
 * @author  Formula Electric @ Berkeley
 */

#ifndef FEB_RFM95_HW_H
#define FEB_RFM95_HW_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "spi.h"
#include <stdint.h>

  /* ============================================================================
   * Hardware Pin Configuration
   * ============================================================================ */

#define FEB_RFM95_SPI_HANDLE (&hspi1)
#define FEB_RFM95_NSS_PORT RD_CS_GPIO_Port
#define FEB_RFM95_NSS_PIN RD_CS_Pin
#define FEB_RFM95_NRST_PORT RD_RST_GPIO_Port
#define FEB_RFM95_NRST_PIN RD_RST_Pin
#define FEB_RFM95_EN_PORT RD_EN_GPIO_Port
#define FEB_RFM95_EN_PIN RD_EN_Pin

  /* ============================================================================
   * Public API
   * ============================================================================ */

  /**
   * @brief Enable RFM95 module power
   */
  void FEB_RFM95_HW_Enable(void);

  /**
   * @brief Disable RFM95 module power
   */
  void FEB_RFM95_HW_Disable(void);

  /**
   * @brief Reset the RFM95 module
   */
  void FEB_RFM95_HW_Reset(void);

  /**
   * @brief Read a single register
   * @param reg Register address
   * @return Register value
   */
  uint8_t FEB_RFM95_HW_ReadRegister(uint8_t reg);

  /**
   * @brief Write a single register
   * @param reg Register address
   * @param value Value to write
   */
  void FEB_RFM95_HW_WriteRegister(uint8_t reg, uint8_t value);

  /**
   * @brief Read multiple bytes from FIFO
   * @param reg Starting register address
   * @param buffer Buffer to store data
   * @param length Number of bytes to read
   */
  void FEB_RFM95_HW_ReadBuffer(uint8_t reg, uint8_t *buffer, uint8_t length);

  /**
   * @brief Write multiple bytes to FIFO
   * @param reg Starting register address
   * @param buffer Data to write
   * @param length Number of bytes to write
   */
  void FEB_RFM95_HW_WriteBuffer(uint8_t reg, const uint8_t *buffer, uint8_t length);

#ifdef __cplusplus
}
#endif

#endif /* FEB_RFM95_HW_H */
