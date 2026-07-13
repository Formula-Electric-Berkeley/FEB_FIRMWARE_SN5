/**
 ******************************************************************************
 * @file           : feb_tps.h
 * @brief          : Public API for FEB TPS2482 Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Minimal driver for TPS2482 power monitors. Each call talks directly to the
 * chip — no caching, no retries, no peripheral resets. Optional FreeRTOS
 * mutex serializes I2C access so multiple tasks can share a bus.
 *
 * Usage:
 *   1. FEB_TPS_Init()
 *   2. FEB_TPS_DeviceRegister() per chip
 *   3. FEB_TPS_Poll() / FEB_TPS_PollScaled() / FEB_TPS_PollRaw() in your loop
 *
 ******************************************************************************
 */

#ifndef FEB_TPS_H
#define FEB_TPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "feb_tps_config.h"
#include "feb_tps_registers.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Note: This header requires STM32 HAL to be included first.
 * Include stm32f4xx_hal.h (or your platform's HAL header) before this file.
 */

/* ============================================================================
 * Device Handle Type
 * ============================================================================ */

/**
 * @brief Opaque handle to a TPS2482 device instance
 */
typedef struct FEB_TPS_Device_s *FEB_TPS_Handle_t;

/* ============================================================================
 * Status Codes
 * ============================================================================ */

typedef enum {
    FEB_TPS_OK = 0,                 /**< Success */
    FEB_TPS_ERR_INVALID_ARG,        /**< Invalid argument */
    FEB_TPS_ERR_I2C,                /**< I2C communication error */
    FEB_TPS_ERR_NOT_INIT,           /**< Library or device not initialized */
    FEB_TPS_ERR_CONFIG_MISMATCH,    /**< CONFIG/CAL readback differed from write */
    FEB_TPS_ERR_MAX_DEVICES,        /**< Too many devices already registered */
} FEB_TPS_Status_t;

/* ============================================================================
 * Logging
 * ============================================================================ */

typedef enum FEB_TPS_LogLevel_e {
    FEB_TPS_LOG_NONE = 0,
    FEB_TPS_LOG_ERROR = 1,
    FEB_TPS_LOG_WARN = 2,
    FEB_TPS_LOG_INFO = 3,
    FEB_TPS_LOG_DEBUG = 4,
} FEB_TPS_LogLevel_t;

/**
 * Library log callback. Receives a pre-formatted, NUL-terminated string with no
 * trailing newline. NULL = silent.
 */
typedef void (*FEB_TPS_LogFunc_t)(FEB_TPS_LogLevel_t level, const char *msg);

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Per-device configuration passed to FEB_TPS_DeviceRegister()
 *
 * Register field semantics:
 *   - config_reg: 0 → use default 0x4127; otherwise written verbatim.
 *   - mask_reg / alert_limit: 0 → register write skipped.
 */
typedef struct {
    /* I2C (required) */
    I2C_HandleTypeDef *hi2c;
    uint8_t i2c_addr;                /**< 7-bit address (typically 0x40-0x4F) */

    /* Calibration (required) */
    float r_shunt_ohms;              /**< Shunt resistor in Ohms */
    float i_max_amps;                /**< Max expected current in Amps */

    /* Optional CONFIG/MASK/ALERT register values */
    uint16_t config_reg;
    uint16_t mask_reg;
    uint16_t alert_limit;

    /* Optional GPIO control (NULL port = unused) */
    GPIO_TypeDef *en_gpio_port;
    uint16_t en_gpio_pin;
    GPIO_TypeDef *pg_gpio_port;
    uint16_t pg_gpio_pin;

    /* Human-readable name for log lines */
    const char *name;
} FEB_TPS_DeviceConfig_t;

/* ============================================================================
 * FreeRTOS sync primitive type (only meaningful when FEB_TPS_USE_FREERTOS=1)
 * ============================================================================ */
#if FEB_TPS_USE_FREERTOS
#include "cmsis_os2.h"
typedef osMutexId_t FEB_TPS_MutexHandle_t;
#else
typedef void *FEB_TPS_MutexHandle_t;
#endif

/**
 * @brief Library-wide configuration passed to FEB_TPS_Init()
 *
 * In FreeRTOS builds the caller must provide an i2c_mutex so the library can
 * serialize bus access across tasks. In bare-metal builds the field is unused.
 */
typedef struct {
    FEB_TPS_LogFunc_t log_func;          /**< NULL = silent */
    FEB_TPS_LogLevel_t log_level;        /**< 0 → defaults to FEB_TPS_LOG_INFO */

#if FEB_TPS_USE_FREERTOS
    FEB_TPS_MutexHandle_t i2c_mutex;     /**< Required: serializes I2C access */
#endif
} FEB_TPS_LibConfig_t;

/* ============================================================================
 * Measurement Data Structures
 * ============================================================================ */

typedef struct {
    float bus_voltage_v;
    float current_a;
    float shunt_voltage_mv;
    float power_w;

    /* Raw register values (current and shunt voltage are sign-corrected) */
    uint16_t bus_voltage_raw;
    int16_t current_raw;
    int16_t shunt_voltage_raw;
    uint16_t power_raw;
} FEB_TPS_Measurement_t;

/**
 * Scaled-integer measurement for CAN packing. Wider types support high-power
 * channels (24V × 20A = 480W).
 */
typedef struct {
    uint32_t bus_voltage_mv;
    int32_t current_ma;
    int32_t shunt_voltage_uv;
    uint32_t power_mw;
} FEB_TPS_MeasurementScaled_t;

/* ============================================================================
 * Library Lifecycle
 * ============================================================================ */

/**
 * Initialize the library. Must be called before any device registration.
 *
 * @param config NULL = silent, no mutex (bare-metal default).
 *               Non-NULL: log callback + level, plus i2c_mutex in FreeRTOS builds.
 * @return FEB_TPS_OK if initialized (or already initialized).
 *         FEB_TPS_ERR_INVALID_ARG if FreeRTOS build and i2c_mutex == NULL.
 */
FEB_TPS_Status_t FEB_TPS_Init(const FEB_TPS_LibConfig_t *config);

/** Returns true if FEB_TPS_Init() has succeeded. */
bool FEB_TPS_IsInitialized(void);

/* ============================================================================
 * Device Registration
 * ============================================================================ */

/**
 * Register a TPS2482, write CONFIG + CAL, read both back, compare. If either
 * register reads back differently from what was written, registration fails
 * with FEB_TPS_ERR_CONFIG_MISMATCH (the bus is alive enough to ACK but the
 * device isn't latching the config — usually wiring or address collision).
 *
 * MASK / ALERT_LIMIT writes are optional (skipped when the config field is 0)
 * and are NOT readback-verified.
 *
 * @param config Device config (must be non-NULL with valid hi2c, positive
 *               r_shunt_ohms and i_max_amps).
 * @param handle Output handle on success.
 * @return FEB_TPS_OK on success;
 *         FEB_TPS_ERR_NOT_INIT if FEB_TPS_Init() not called;
 *         FEB_TPS_ERR_INVALID_ARG for bad arguments;
 *         FEB_TPS_ERR_MAX_DEVICES if FEB_TPS_MAX_DEVICES already registered;
 *         FEB_TPS_ERR_I2C on a CONFIG/CAL write failure;
 *         FEB_TPS_ERR_CONFIG_MISMATCH on readback disagreement.
 */
FEB_TPS_Status_t FEB_TPS_DeviceRegister(const FEB_TPS_DeviceConfig_t *config,
                                        FEB_TPS_Handle_t *handle);

/* ============================================================================
 * Measurement API
 * ============================================================================ */

/**
 * Read BUS_VOLT, CURRENT, SHUNT_VOLT, POWER and convert to floats. Stops at
 * the first I2C error; fields read before the failure remain populated.
 */
FEB_TPS_Status_t FEB_TPS_Poll(FEB_TPS_Handle_t handle,
                              FEB_TPS_Measurement_t *measurement);

/**
 * Read all four measurement registers and convert to scaled integers
 * (mV / mA / uV / mW) suitable for CAN packing.
 */
FEB_TPS_Status_t FEB_TPS_PollScaled(FEB_TPS_Handle_t handle,
                                    FEB_TPS_MeasurementScaled_t *scaled);

/**
 * Read raw BUS_VOLT, CURRENT, SHUNT_VOLT registers. CURRENT and SHUNT_VOLT
 * are sign-magnitude converted to signed values. Any output pointer may be
 * NULL to skip that register's read.
 */
FEB_TPS_Status_t FEB_TPS_PollRaw(FEB_TPS_Handle_t handle,
                                 uint16_t *bus_v_raw,
                                 int16_t *current_raw,
                                 int16_t *shunt_v_raw);

/* ============================================================================
 * GPIO Control
 * ============================================================================ */

/**
 * Drive the device's EN pin. Returns FEB_TPS_ERR_INVALID_ARG if no EN GPIO
 * was configured at registration.
 */
FEB_TPS_Status_t FEB_TPS_Enable(FEB_TPS_Handle_t handle, bool enable);

/**
 * Read the device's PG (power-good) pin. *pg_state is true when the pin is
 * high. Returns FEB_TPS_ERR_INVALID_ARG if no PG GPIO was configured.
 */
FEB_TPS_Status_t FEB_TPS_ReadPowerGood(FEB_TPS_Handle_t handle, bool *pg_state);

/* ============================================================================
 * Diagnostics & Utilities
 * ============================================================================ */

/** Read the TPS2482 device ID register (0xFF). */
FEB_TPS_Status_t FEB_TPS_ReadID(FEB_TPS_Handle_t handle, uint16_t *id);

/** Get the device's computed current LSB in A/bit (i_max / 32768). */
float FEB_TPS_GetCurrentLSB(FEB_TPS_Handle_t handle);

/** Get the device name passed at registration, or "Unknown" if NULL. */
const char *FEB_TPS_GetDeviceName(FEB_TPS_Handle_t handle);

/** Convert a status code to a static string. */
const char *FEB_TPS_StatusToString(FEB_TPS_Status_t status);

/**
 * Convert a 16-bit sign-magnitude register value to a signed int16.
 * Bit 15 = sign, bits 14:0 = magnitude.
 */
static inline int16_t FEB_TPS_SignMagnitude(uint16_t raw) {
    if (raw & FEB_TPS_SIGN_BIT) {
        return -(int16_t)(raw & FEB_TPS_MAGNITUDE_MASK);
    }
    return (int16_t)(raw & FEB_TPS_MAGNITUDE_MASK);
}

#ifdef __cplusplus
}
#endif

#endif /* FEB_TPS_H */
