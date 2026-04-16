/**
 * @file FEB_ADBMS_Platform.c
 * @brief Platform implementation for ADBMS6830B register driver
 *
 * Implements the weak platform functions from ADBMS6830B_Registers.h
 * using STM32 HAL directly.
 */

#include "FEB_ADBMS_Platform.h"
#include "ADBMS6830B_Registers.h"
#include "main.h"
#include "spi.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"

/*============================================================================
 * Configuration
 *============================================================================*/

/* SPI timeout in milliseconds */
#define ADBMS_SPI_TIMEOUT_MS 100

/* Use SPI1 as primary for ADBMS communication */
#define ADBMS_SPI_HANDLE (&hspi1)
#define ADBMS_CS_PORT SPI1_CS_GPIO_Port
#define ADBMS_CS_PIN SPI1_CS_Pin

/*============================================================================
 * Platform Function Implementations
 *============================================================================*/

/**
 * @brief Map HAL status to platform status
 */
static ADBMS_PlatformStatus_t _hal_to_platform_status(HAL_StatusTypeDef hal_status)
{
  switch (hal_status)
  {
  case HAL_OK:
    return ADBMS_PLATFORM_OK;
  case HAL_TIMEOUT:
    return ADBMS_PLATFORM_ERR_TIMEOUT;
  default:
    return ADBMS_PLATFORM_ERR_SPI;
  }
}

/**
 * @brief SPI write (transmit only)
 */
ADBMS_PlatformStatus_t ADBMS_Platform_SPI_Write(const uint8_t *data, uint16_t len)
{
  HAL_StatusTypeDef status = HAL_SPI_Transmit(ADBMS_SPI_HANDLE, (uint8_t *)data, len, ADBMS_SPI_TIMEOUT_MS);
  return _hal_to_platform_status(status);
}

/**
 * @brief SPI read (receive only)
 */
ADBMS_PlatformStatus_t ADBMS_Platform_SPI_Read(uint8_t *data, uint16_t len)
{
  HAL_StatusTypeDef status = HAL_SPI_Receive(ADBMS_SPI_HANDLE, data, len, ADBMS_SPI_TIMEOUT_MS);
  return _hal_to_platform_status(status);
}

/**
 * @brief SPI write then read (separate transactions)
 */
ADBMS_PlatformStatus_t ADBMS_Platform_SPI_WriteRead(const uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data,
                                                    uint16_t rx_len)
{
  HAL_StatusTypeDef status = HAL_SPI_Transmit(ADBMS_SPI_HANDLE, (uint8_t *)tx_data, tx_len, ADBMS_SPI_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return _hal_to_platform_status(status);
  }
  status = HAL_SPI_Receive(ADBMS_SPI_HANDLE, rx_data, rx_len, ADBMS_SPI_TIMEOUT_MS);
  return _hal_to_platform_status(status);
}

/**
 * @brief Assert chip select (active low)
 */
void ADBMS_Platform_CS_Low(void)
{
  HAL_GPIO_WritePin(ADBMS_CS_PORT, ADBMS_CS_PIN, GPIO_PIN_RESET);
}

/**
 * @brief Deassert chip select
 */
void ADBMS_Platform_CS_High(void)
{
  HAL_GPIO_WritePin(ADBMS_CS_PORT, ADBMS_CS_PIN, GPIO_PIN_SET);
}

/**
 * @brief Microsecond delay using DWT cycle counter
 * @note DWT must be enabled before calling this function
 */
void ADBMS_Platform_DelayUs(uint32_t us)
{
  /* Use DWT cycle counter for microsecond precision */
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = us * (SystemCoreClock / 1000000);

  while ((DWT->CYCCNT - start) < cycles)
  {
    __NOP();
  }
}

/**
 * @brief Millisecond delay using FreeRTOS
 */
void ADBMS_Platform_DelayMs(uint32_t ms)
{
  osDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief Get current tick count in milliseconds
 */
uint32_t ADBMS_Platform_GetTickMs(void)
{
  return HAL_GetTick();
}

/*============================================================================
 * Platform Initialization
 *============================================================================*/

/**
 * @brief Initialize platform hardware for ADBMS communication
 */
void FEB_ADBMS_Platform_Init(void)
{
  /* Enable DWT cycle counter for microsecond delays */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* Ensure CS is high (deasserted) at init */
  ADBMS_Platform_CS_High();
}
