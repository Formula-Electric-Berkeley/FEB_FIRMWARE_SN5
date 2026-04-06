/**
 * @file    FEB_RFM95_HW.c
 * @brief   RFM95 Hardware Abstraction Implementation
 * @author  Formula Electric @ Berkeley
 */

#include "FEB_RFM95_HW.h"
#include "FEB_RFM95_Const.h"
#include "cmsis_os.h"

/* ============================================================================
 * Module Power Control
 * ============================================================================ */

void FEB_RFM95_HW_Enable(void)
{
  HAL_GPIO_WritePin(FEB_RFM95_EN_PORT, FEB_RFM95_EN_PIN, GPIO_PIN_SET);
}

void FEB_RFM95_HW_Disable(void)
{
  HAL_GPIO_WritePin(FEB_RFM95_EN_PORT, FEB_RFM95_EN_PIN, GPIO_PIN_RESET);
}

void FEB_RFM95_HW_Reset(void)
{
  HAL_GPIO_WritePin(FEB_RFM95_NRST_PORT, FEB_RFM95_NRST_PIN, GPIO_PIN_RESET);
  osDelay(1);
  HAL_GPIO_WritePin(FEB_RFM95_NRST_PORT, FEB_RFM95_NRST_PIN, GPIO_PIN_SET);
  osDelay(10);
}

/* ============================================================================
 * SPI Communication
 * ============================================================================ */

static inline void nss_select(void)
{
  HAL_GPIO_WritePin(FEB_RFM95_NSS_PORT, FEB_RFM95_NSS_PIN, GPIO_PIN_RESET);
}

static inline void nss_deselect(void)
{
  HAL_GPIO_WritePin(FEB_RFM95_NSS_PORT, FEB_RFM95_NSS_PIN, GPIO_PIN_SET);
}

uint8_t FEB_RFM95_HW_ReadRegister(uint8_t reg)
{
  uint8_t tx_data[2] = {reg & 0x7F, 0x00};
  uint8_t rx_data[2] = {0};

  nss_select();
  HAL_SPI_TransmitReceive(FEB_RFM95_SPI_HANDLE, tx_data, rx_data, 2, RFM95_SPI_TIMEOUT_MS);
  nss_deselect();

  return rx_data[1];
}

void FEB_RFM95_HW_WriteRegister(uint8_t reg, uint8_t value)
{
  uint8_t tx_data[2] = {reg | 0x80, value};

  nss_select();
  HAL_SPI_Transmit(FEB_RFM95_SPI_HANDLE, tx_data, 2, RFM95_SPI_TIMEOUT_MS);
  nss_deselect();
}

void FEB_RFM95_HW_ReadBuffer(uint8_t reg, uint8_t *buffer, uint8_t length)
{
  uint8_t addr = reg & 0x7F;

  nss_select();
  HAL_SPI_Transmit(FEB_RFM95_SPI_HANDLE, &addr, 1, RFM95_SPI_TIMEOUT_MS);
  HAL_SPI_Receive(FEB_RFM95_SPI_HANDLE, buffer, length, RFM95_SPI_TIMEOUT_MS);
  nss_deselect();
}

void FEB_RFM95_HW_WriteBuffer(uint8_t reg, const uint8_t *buffer, uint8_t length)
{
  uint8_t addr = reg | 0x80;

  nss_select();
  HAL_SPI_Transmit(FEB_RFM95_SPI_HANDLE, &addr, 1, RFM95_SPI_TIMEOUT_MS);
  HAL_SPI_Transmit(FEB_RFM95_SPI_HANDLE, (uint8_t *)buffer, length, RFM95_SPI_TIMEOUT_MS);
  nss_deselect();
}
