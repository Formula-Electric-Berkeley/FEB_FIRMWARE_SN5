/**
 ******************************************************************************
 * @file           : feb_tps_internal.h
 * @brief          : Internal types and FreeRTOS/bare-metal abstraction
 * @author         : Formula Electric @ Berkeley
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
 * Note: This header requires STM32 HAL to be included first so HAL types
 * (I2C_HandleTypeDef, GPIO_TypeDef) are defined.
 */

/* ============================================================================
 * FreeRTOS / bare-metal mutex abstraction
 * ============================================================================ */

#if FEB_TPS_USE_FREERTOS

#include "FreeRTOS.h"
#include "cmsis_os2.h"

#define FEB_TPS_MUTEX_LOCK(m)    do { if ((m) != NULL) { osMutexAcquire((m), osWaitForever); } } while (0)
#define FEB_TPS_MUTEX_UNLOCK(m)  do { if ((m) != NULL) { osMutexRelease((m)); } } while (0)

#else /* Bare-metal: mutex ops are no-ops */

#define FEB_TPS_MUTEX_LOCK(m)    ((void)0)
#define FEB_TPS_MUTEX_UNLOCK(m)  ((void)0)

#endif /* FEB_TPS_USE_FREERTOS */

/* ============================================================================
 * Internal device structure (one slot per registered TPS2482)
 * ============================================================================ */

typedef struct FEB_TPS_Device_s {
    /* I2C */
    I2C_HandleTypeDef *hi2c;
    uint8_t i2c_addr;

    /* Calibration inputs */
    float r_shunt_ohms;
    float i_max_amps;

    /* Computed calibration */
    float current_lsb;          /**< i_max / 32768 (A/bit) */
    float power_lsb;            /**< current_lsb * 25 (W/bit) */
    uint16_t cal_reg;           /**< CAL register value written to chip */

    /* Optional GPIOs */
    void *en_gpio_port;         /**< NULL = unused */
    uint16_t en_gpio_pin;
    void *pg_gpio_port;         /**< NULL = unused */
    uint16_t pg_gpio_pin;

    /* Metadata */
    const char *name;
    bool initialized;
} FEB_TPS_Device_t;

/* ============================================================================
 * Library context
 * ============================================================================ */

typedef struct {
    FEB_TPS_Device_t devices[FEB_TPS_MAX_DEVICES];
    uint8_t device_count;
    FEB_TPS_LogFunc_t log_func;
    uint8_t log_level;
    bool initialized;

#if FEB_TPS_USE_FREERTOS
    FEB_TPS_MutexHandle_t i2c_mutex;
#endif
} FEB_TPS_Context_t;

/* ============================================================================
 * Internal logging (printf-style, calls user callback if registered)
 * ============================================================================ */

void FEB_TPS_Log(uint8_t level, const char *fmt, ...);

#define TPS_LOG_E(fmt, ...) FEB_TPS_Log(FEB_TPS_LOG_ERROR, fmt, ##__VA_ARGS__)
#define TPS_LOG_W(fmt, ...) FEB_TPS_Log(FEB_TPS_LOG_WARN,  fmt, ##__VA_ARGS__)
#define TPS_LOG_I(fmt, ...) FEB_TPS_Log(FEB_TPS_LOG_INFO,  fmt, ##__VA_ARGS__)
#define TPS_LOG_D(fmt, ...) FEB_TPS_Log(FEB_TPS_LOG_DEBUG, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* FEB_TPS_INTERNAL_H */
