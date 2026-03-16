/**
 ******************************************************************************
 * @file           : feb_tps_internal.h
 * @brief          : Internal types and FreeRTOS/bare-metal abstraction
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Internal implementation details. Do not include directly in user code.
 * Contains:
 *   - FreeRTOS/bare-metal mutex abstraction macros
 *   - Internal device structure definition
 *
 ******************************************************************************
 */

#ifndef FEB_TPS_INTERNAL_H
#define FEB_TPS_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "feb_tps_config.h"
#include "feb_tps_registers.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * Note: This header requires STM32 HAL to be included first.
 * HAL types (I2C_HandleTypeDef, GPIO_TypeDef) must already be defined.
 */

/* ============================================================================
 * FreeRTOS Abstraction Layer
 * ============================================================================ */

#if FEB_TPS_USE_FREERTOS

#include "FreeRTOS.h"
#include "cmsis_os2.h"

typedef osMutexId_t FEB_TPS_Mutex_t;

#define FEB_TPS_MUTEX_CREATE()      osMutexNew(NULL)
#define FEB_TPS_MUTEX_DELETE(m)     osMutexDelete(m)
#define FEB_TPS_MUTEX_LOCK(m)       osMutexAcquire(m, osWaitForever)
#define FEB_TPS_MUTEX_UNLOCK(m)     osMutexRelease(m)

#define FEB_TPS_IN_ISR()            (xPortIsInsideInterrupt())
#define FEB_TPS_DELAY_MS(ms)        osDelay(ms)

#define FEB_TPS_ENTER_CRITICAL()    /* Use mutex instead in FreeRTOS */
#define FEB_TPS_EXIT_CRITICAL()

#else /* Bare-metal */

typedef uint8_t FEB_TPS_Mutex_t;

#define FEB_TPS_MUTEX_CREATE()      (0)
#define FEB_TPS_MUTEX_DELETE(m)     ((void)0)
#define FEB_TPS_MUTEX_LOCK(m)       __disable_irq()
#define FEB_TPS_MUTEX_UNLOCK(m)     __enable_irq()

#define FEB_TPS_IN_ISR()            ((__get_IPSR() & 0xFF) != 0)
#define FEB_TPS_DELAY_MS(ms)        HAL_Delay(ms)

#define FEB_TPS_ENTER_CRITICAL()    __disable_irq()
#define FEB_TPS_EXIT_CRITICAL()     __enable_irq()

#endif /* FEB_TPS_USE_FREERTOS */

/* ============================================================================
 * Internal Device Structure
 * ============================================================================ */

/**
 * @brief Internal device state structure
 *
 * This structure holds the configuration and computed calibration values
 * for a single TPS2482 device.
 */
typedef struct FEB_TPS_Device_s {
    /* I2C Configuration */
    I2C_HandleTypeDef *hi2c;        /**< I2C handle */
    uint8_t i2c_addr;               /**< 7-bit I2C address */

    /* Current Measurement Configuration */
    float r_shunt_ohms;             /**< Shunt resistor value in Ohms */
    float i_max_amps;               /**< Maximum expected current in Amps */

    /* Computed Calibration Values */
    float current_lsb;              /**< Current LSB in A/bit (I_max / 32768) */
    float power_lsb;                /**< Power LSB in W/bit (current_lsb * 25) */
    uint16_t cal_reg;               /**< Calibration register value */

    /* GPIO Configuration (optional) */
    void *en_gpio_port;             /**< GPIO port for EN pin (NULL = not used) */
    uint16_t en_gpio_pin;           /**< GPIO pin for EN */
    void *pg_gpio_port;             /**< GPIO port for Power-Good pin */
    uint16_t pg_gpio_pin;           /**< GPIO pin for Power-Good */
    void *alert_gpio_port;          /**< GPIO port for Alert pin */
    uint16_t alert_gpio_pin;        /**< GPIO pin for Alert */

    /* Metadata */
    const char *name;               /**< Human-readable name for debugging */
    uint16_t device_id;             /**< Device unique ID (read from chip) */
    bool initialized;               /**< Device initialized flag */
} FEB_TPS_Device_t;

/* ============================================================================
 * Library State Structure
 * ============================================================================ */

/**
 * @brief Library global state
 */
typedef struct {
    FEB_TPS_Device_t devices[FEB_TPS_MAX_DEVICES];  /**< Registered devices */
    uint8_t device_count;                            /**< Number of registered devices */
    FEB_TPS_Mutex_t i2c_mutex;                       /**< I2C access mutex */
    uint32_t i2c_timeout_ms;                         /**< I2C timeout */
    bool initialized;                                /**< Library initialized flag */
} FEB_TPS_Context_t;

/* ============================================================================
 * Sign-Magnitude Conversion
 * ============================================================================ */

/**
 * @brief Convert raw register value to signed using sign-magnitude format
 *
 * TPS2482 uses sign-magnitude format for current and shunt voltage:
 *   - Bit 15 = sign (0 = positive, 1 = negative)
 *   - Bits 14:0 = magnitude
 *
 * @param raw Raw 16-bit register value
 * @return Signed 16-bit value
 */
static inline int16_t feb_tps_sign_magnitude(uint16_t raw) {
    if (raw & 0x8000) {
        return -(int16_t)(raw & 0x7FFF);
    }
    return (int16_t)(raw & 0x7FFF);
}

#ifdef __cplusplus
}
#endif

#endif /* FEB_TPS_INTERNAL_H */
