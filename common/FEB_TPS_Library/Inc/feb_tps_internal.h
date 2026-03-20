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

typedef uint32_t FEB_TPS_Mutex_t;

/*
 * Bare-metal sync primitive behavior depends on FEB_TPS_FORCE_BARE_METAL:
 *
 * When FORCE_BARE_METAL == 0 (default):
 *   - Mutex operations are NO-OPs
 *   - Safe for single-threaded applications
 *
 * When FORCE_BARE_METAL == 1 (explicit):
 *   - Uses __disable_irq() / __enable_irq() for critical sections
 */
#if FEB_TPS_FORCE_BARE_METAL

#define FEB_TPS_MUTEX_CREATE()      (0U)
#define FEB_TPS_MUTEX_DELETE(m)     ((void)0)
#define FEB_TPS_MUTEX_LOCK(m)       do { (m) = __get_PRIMASK(); __disable_irq(); } while(0)
#define FEB_TPS_MUTEX_UNLOCK(m)     __set_PRIMASK(m)

#else /* Safe no-op defaults */

#define FEB_TPS_MUTEX_CREATE()      (0U)
#define FEB_TPS_MUTEX_DELETE(m)     ((void)0)
#define FEB_TPS_MUTEX_LOCK(m)       ((void)0)
#define FEB_TPS_MUTEX_UNLOCK(m)     ((void)0)

#endif /* FEB_TPS_FORCE_BARE_METAL */

#define FEB_TPS_IN_ISR()            ((__get_IPSR() & 0xFF) != 0)
#define FEB_TPS_DELAY_MS(ms)        HAL_Delay(ms)

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
    bool in_use;                    /**< Slot is in use (for stable handles) */

#if FEB_TPS_USE_FREERTOS
    /* Cached measurement data (updated by polling task) */
    float cached_voltage_v;
    float cached_current_a;
    float cached_power_w;
    uint32_t cached_last_update_ms;
    bool cached_valid;
#endif
} FEB_TPS_Device_t;

/* ============================================================================
 * Library State Structure
 * ============================================================================ */

/**
 * @brief Library global state
 *
 * Note: FEB_TPS_LogFunc_t and FEB_TPS_LogLevel_t are defined in feb_tps.h
 * which must be included before this header in feb_tps.c
 */
typedef struct {
    FEB_TPS_Device_t devices[FEB_TPS_MAX_DEVICES];  /**< Registered devices */
    uint8_t device_count;                            /**< Number of registered devices */
    uint32_t i2c_timeout_ms;                         /**< I2C timeout */
    FEB_TPS_LogFunc_t log_func;                      /**< User logging callback */
    uint8_t log_level;                               /**< Minimum log level (FEB_TPS_LogLevel_t) */
    bool initialized;                                /**< Library initialized flag */

#if FEB_TPS_USE_FREERTOS
    /* User-provided sync primitives (NOT created internally) */
    FEB_TPS_MutexHandle_t data_mutex;                /**< Protects cached data reads */
    FEB_TPS_MutexHandle_t i2c_mutex;                 /**< Protects I2C bus access */
    uint32_t poll_interval_ms;                       /**< Polling interval for auto-poll task */
    uint32_t (*get_tick_ms)(void);                   /**< Timestamp function */
#else
    FEB_TPS_Mutex_t i2c_mutex;                       /**< Bare-metal: PRIMASK storage */
#endif
} FEB_TPS_Context_t;

/* ============================================================================
 * Internal Logging
 * ============================================================================ */

/**
 * @brief Internal logging function (formats and calls user callback)
 *
 * @param level Log level (use TPS_LOG_* macros)
 * @param fmt Printf-style format string
 */
void FEB_TPS_Log(uint8_t level, const char *fmt, ...);

/* Log level values for macros (must match FEB_TPS_LogLevel_t in feb_tps.h) */
#define TPS_LOG_E(fmt, ...) FEB_TPS_Log(1, fmt, ##__VA_ARGS__)
#define TPS_LOG_W(fmt, ...) FEB_TPS_Log(2, fmt, ##__VA_ARGS__)
#define TPS_LOG_I(fmt, ...) FEB_TPS_Log(3, fmt, ##__VA_ARGS__)
#define TPS_LOG_D(fmt, ...) FEB_TPS_Log(4, fmt, ##__VA_ARGS__)

/* ============================================================================
 * Sign-Magnitude Conversion
 * ============================================================================ */

/*
 * NOTE: feb_tps_internal.h requires feb_tps.h for FEB_TPS_SignMagnitude.
 * Include it automatically if not already included.
 */
#ifndef FEB_TPS_H
#include "feb_tps.h"
#endif

/**
 * @brief Internal sign-magnitude conversion (delegates to public API)
 */
#define feb_tps_sign_magnitude(raw) FEB_TPS_SignMagnitude(raw)

#ifdef __cplusplus
}
#endif

#endif /* FEB_TPS_INTERNAL_H */
