/**
 * @file hal_mock.h
 * @brief HAL mock types and function declarations for host-based unit testing
 *
 * Provides minimal STM32 HAL-compatible types and mock functions so that
 * the FEB_TPS_Library can be compiled and tested on a standard Linux host
 * without any embedded toolchain.
 */

#ifndef HAL_MOCK_H
#define HAL_MOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * HAL Status Codes
 * ============================================================================ */

typedef enum {
    HAL_OK      = 0x00U,
    HAL_ERROR   = 0x01U,
    HAL_BUSY    = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

/* ============================================================================
 * GPIO Types
 * ============================================================================ */

typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET
} GPIO_PinState;

typedef struct {
    uint32_t dummy; /* placeholder - tests use address as ID */
} GPIO_TypeDef;

/* ============================================================================
 * I2C Types
 * ============================================================================ */

#define I2C_MEMADD_SIZE_8BIT  0x01U
#define I2C_MEMADD_SIZE_16BIT 0x02U

typedef struct {
    uint32_t dummy; /* placeholder */
} I2C_HandleTypeDef;

/* ============================================================================
 * ARM Cortex-M stubs (used by bare-metal mutex path)
 * ============================================================================ */

static inline void __disable_irq(void) { /* no-op on host */ }
static inline void __enable_irq(void)  { /* no-op on host */ }
static inline uint32_t __get_IPSR(void) { return 0; } /* always outside ISR */

/* ============================================================================
 * HAL Mock Configuration
 * ============================================================================ */

/**
 * Per-call mock response for HAL_I2C_Mem_Read.
 * When the queue is empty, defaults to HAL_OK with zero data.
 */
typedef struct {
    HAL_StatusTypeDef status;   /**< Return status */
    uint8_t data[2];            /**< 2 bytes to return (MSB first) */
} I2C_ReadResponse_t;

/**
 * Per-call mock response for HAL_I2C_Mem_Write.
 */
typedef struct {
    HAL_StatusTypeDef status;
} I2C_WriteResponse_t;

/**
 * Recorded I2C write call (for verification).
 */
typedef struct {
    uint16_t dev_addr;  /**< Device address (shifted, as passed to HAL) */
    uint8_t  mem_addr;  /**< Register address */
    uint16_t value;     /**< 16-bit value written (MSB first reconstruction) */
} I2C_WriteRecord_t;

/* ============================================================================
 * Mock Control API
 * ============================================================================ */

/** Reset all mock state (call between tests) */
void hal_mock_reset(void);

/** Queue a response for the next HAL_I2C_Mem_Read call */
void hal_mock_i2c_read_push(HAL_StatusTypeDef status, uint8_t msb, uint8_t lsb);

/** Queue a response for the next HAL_I2C_Mem_Write call */
void hal_mock_i2c_write_push(HAL_StatusTypeDef status);

/** Get number of write calls recorded */
int hal_mock_i2c_write_count(void);

/** Get a recorded write call by index */
I2C_WriteRecord_t hal_mock_i2c_write_get(int index);

/** Get number of read calls made */
int hal_mock_i2c_read_count(void);

/** Set the GPIO pin state returned by HAL_GPIO_ReadPin */
void hal_mock_gpio_set_pin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);

/** Get the last GPIO state written via HAL_GPIO_WritePin */
GPIO_PinState hal_mock_gpio_get_written(GPIO_TypeDef *port, uint16_t pin);

/** Get the number of HAL_GPIO_WritePin calls */
int hal_mock_gpio_write_count(void);

/* ============================================================================
 * HAL Function Declarations (implemented in hal_mock.c)
 * ============================================================================ */

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c,
                                    uint16_t DevAddress,
                                    uint16_t MemAddress,
                                    uint16_t MemAddSize,
                                    uint8_t *pData,
                                    uint16_t Size,
                                    uint32_t Timeout);

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c,
                                     uint16_t DevAddress,
                                     uint16_t MemAddress,
                                     uint16_t MemAddSize,
                                     uint8_t *pData,
                                     uint16_t Size,
                                     uint32_t Timeout);

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

void HAL_Delay(uint32_t Delay);

#endif /* HAL_MOCK_H */