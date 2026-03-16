/**
 ******************************************************************************
 * @file           : feb_tps.h
 * @brief          : Public API for FEB TPS2482 Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * A common library for TPS2482 power monitoring ICs. Features:
 *   - Multi-device support (up to FEB_TPS_MAX_DEVICES)
 *   - Per-device shunt resistor and current range configuration
 *   - FreeRTOS-optional thread safety with I2C mutex
 *   - Correct sign-magnitude current conversion
 *   - Optional GPIO enable/power-good/alert integration
 *
 * Usage:
 *   1. Call FEB_TPS_Init() once at startup
 *   2. Call FEB_TPS_DeviceRegister() for each TPS2482 device
 *   3. Call FEB_TPS_Poll() or FEB_TPS_PollAll() to read measurements
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
 * @brief Handle to a TPS2482 device instance
 *
 * This is a pointer to an internal device structure. Users should treat it
 * as an opaque handle and not dereference it directly.
 */
typedef struct FEB_TPS_Device_s *FEB_TPS_Handle_t;

/* ============================================================================
 * Status Codes
 * ============================================================================ */

typedef enum {
    FEB_TPS_OK = 0,             /**< Success */
    FEB_TPS_ERR_INVALID_ARG,    /**< Invalid argument */
    FEB_TPS_ERR_I2C,            /**< I2C communication error */
    FEB_TPS_ERR_NOT_INIT,       /**< Library or device not initialized */
    FEB_TPS_ERR_CONFIG_MISMATCH,/**< Configuration readback mismatch */
    FEB_TPS_ERR_MAX_DEVICES,    /**< Maximum device count exceeded */
    FEB_TPS_ERR_TIMEOUT,        /**< Operation timeout */
} FEB_TPS_Status_t;

/* ============================================================================
 * Logging
 * ============================================================================ */

/**
 * @brief Log levels for TPS library (values match FEB_UART for easy mapping)
 */
typedef enum FEB_TPS_LogLevel_e {
    FEB_TPS_LOG_NONE = 0,       /**< No logging */
    FEB_TPS_LOG_ERROR = 1,      /**< Errors only */
    FEB_TPS_LOG_WARN = 2,       /**< Warnings and errors */
    FEB_TPS_LOG_INFO = 3,       /**< Informational messages */
    FEB_TPS_LOG_DEBUG = 4,      /**< Debug messages */
} FEB_TPS_LogLevel_t;

/**
 * @brief Logging callback function type
 *
 * The library formats messages internally and passes a pre-formatted string.
 * This makes it easy to wrap FEB_UART's LOG_* macros.
 *
 * @param level Log level of the message
 * @param msg Pre-formatted message string (no newline)
 */
typedef void (*FEB_TPS_LogFunc_t)(FEB_TPS_LogLevel_t level, const char *msg);

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief TPS2482 device configuration
 *
 * Used when registering a new device with FEB_TPS_DeviceRegister().
 */
typedef struct {
    /* I2C Configuration (required) */
    I2C_HandleTypeDef *hi2c;    /**< I2C handle */
    uint8_t i2c_addr;           /**< 7-bit I2C address (use FEB_TPS_ADDR macro) */

    /* Current Measurement Configuration (required) */
    float r_shunt_ohms;         /**< Shunt resistor value in Ohms (e.g., 0.002 for 2mOhm) */
    float i_max_amps;           /**< Maximum expected current in Amps (determines resolution) */

    /* Optional: Device Configuration Register Settings */
    uint16_t config_reg;        /**< CONFIG register value (0 = use default 0x4127) */
    uint16_t mask_reg;          /**< MASK register value (0 = no alerts) */
    uint16_t alert_limit;       /**< ALERT_LIMIT register value (0 = no limit) */

    /* Optional: GPIO for Enable/Power-Good/Alert */
    GPIO_TypeDef *en_gpio_port;     /**< GPIO port for EN pin (NULL = not used) */
    uint16_t en_gpio_pin;           /**< GPIO pin for EN (ignored if port is NULL) */
    GPIO_TypeDef *pg_gpio_port;     /**< GPIO port for Power-Good pin (NULL = not used) */
    uint16_t pg_gpio_pin;           /**< GPIO pin for Power-Good */
    GPIO_TypeDef *alert_gpio_port;  /**< GPIO port for Alert pin (NULL = not used) */
    uint16_t alert_gpio_pin;        /**< GPIO pin for Alert */

    /* Optional: Name for debugging */
    const char *name;           /**< Human-readable name (e.g., "LV", "BMS") */
} FEB_TPS_DeviceConfig_t;

/**
 * @brief Library initialization configuration
 */
typedef struct {
    uint32_t i2c_timeout_ms;    /**< I2C timeout in ms (0 = use default) */
    FEB_TPS_LogFunc_t log_func; /**< Logging callback (NULL = silent) */
    FEB_TPS_LogLevel_t log_level; /**< Minimum level to log (0 = use default INFO) */
} FEB_TPS_LibConfig_t;

/* ============================================================================
 * Measurement Data Structures
 * ============================================================================ */

/**
 * @brief Measured values from a TPS2482 device (floating point)
 */
typedef struct {
    float bus_voltage_v;        /**< Bus voltage in Volts */
    float current_a;            /**< Current in Amps (signed) */
    float shunt_voltage_mv;     /**< Shunt voltage in millivolts (signed) */
    float power_w;              /**< Power in Watts */

    /* Raw register values (for debugging/CAN transmission) */
    uint16_t bus_voltage_raw;   /**< Raw bus voltage register */
    int16_t current_raw;        /**< Sign-corrected current register */
    int16_t shunt_voltage_raw;  /**< Sign-corrected shunt voltage register */
    uint16_t power_raw;         /**< Raw power register */
} FEB_TPS_Measurement_t;

/**
 * @brief Scaled integer values for CAN transmission
 *
 * Avoids floating point on CAN bus. Values are already scaled to common units.
 * Uses wider types to support high-power applications (e.g., 24V @ 20A = 480W).
 */
typedef struct {
    uint32_t bus_voltage_mv;    /**< Bus voltage in millivolts */
    int32_t current_ma;         /**< Current in milliamps */
    int32_t shunt_voltage_uv;   /**< Shunt voltage in microvolts */
    uint32_t power_mw;          /**< Power in milliwatts */
} FEB_TPS_MeasurementScaled_t;

/* ============================================================================
 * Library Initialization
 * ============================================================================ */

/**
 * @brief Initialize the TPS library
 *
 * Must be called before registering any devices.
 *
 * @param config Library configuration (NULL for defaults)
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_Init(const FEB_TPS_LibConfig_t *config);

/**
 * @brief Deinitialize the TPS library and all devices
 */
void FEB_TPS_DeInit(void);

/**
 * @brief Check if library is initialized
 */
bool FEB_TPS_IsInitialized(void);

/* ============================================================================
 * Device Management
 * ============================================================================ */

/**
 * @brief Register and initialize a TPS2482 device
 *
 * Performs I2C communication to configure the device and verify settings.
 *
 * @param config Device configuration
 * @param handle Output: device handle on success
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_DeviceRegister(const FEB_TPS_DeviceConfig_t *config,
                                         FEB_TPS_Handle_t *handle);

/**
 * @brief Unregister a device
 *
 * @param handle Device handle
 */
void FEB_TPS_DeviceUnregister(FEB_TPS_Handle_t handle);

/**
 * @brief Get device by index (for iteration)
 *
 * @param index Device index (0 to count-1)
 * @return Device handle, or NULL if index out of range
 */
FEB_TPS_Handle_t FEB_TPS_DeviceGetByIndex(uint8_t index);

/**
 * @brief Get number of registered devices
 */
uint8_t FEB_TPS_DeviceGetCount(void);

/* ============================================================================
 * Measurement API
 * ============================================================================ */

/**
 * @brief Poll all measurements from a single device
 *
 * Reads bus voltage, current, shunt voltage, and power registers.
 * Thread-safe when FreeRTOS is enabled.
 *
 * @param handle Device handle
 * @param measurement Output: measurement data
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_Poll(FEB_TPS_Handle_t handle,
                               FEB_TPS_Measurement_t *measurement);

/**
 * @brief Poll and get scaled integer values (for CAN)
 *
 * @param handle Device handle
 * @param scaled Output: scaled measurement data
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_PollScaled(FEB_TPS_Handle_t handle,
                                     FEB_TPS_MeasurementScaled_t *scaled);

/**
 * @brief Poll only bus voltage
 *
 * @param handle Device handle
 * @param voltage_v Output: voltage in Volts
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_PollBusVoltage(FEB_TPS_Handle_t handle, float *voltage_v);

/**
 * @brief Poll only current
 *
 * @param handle Device handle
 * @param current_a Output: current in Amps (signed)
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_PollCurrent(FEB_TPS_Handle_t handle, float *current_a);

/**
 * @brief Poll raw register values (for debugging or custom processing)
 *
 * @param handle Device handle
 * @param bus_v_raw Output: raw bus voltage register (can be NULL)
 * @param current_raw Output: raw current register (sign-corrected) (can be NULL)
 * @param shunt_v_raw Output: raw shunt voltage register (sign-corrected) (can be NULL)
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_PollRaw(FEB_TPS_Handle_t handle,
                                  uint16_t *bus_v_raw,
                                  int16_t *current_raw,
                                  int16_t *shunt_v_raw);

/* ============================================================================
 * Batch Operations (for multi-device boards like LVPDB)
 * ============================================================================ */

/**
 * @brief Poll all registered devices
 *
 * Thread-safe when FreeRTOS is enabled (single I2C transaction batch).
 *
 * @param measurements Array to store results (must be at least DeviceGetCount() elements)
 * @param count Number of elements in array
 * @return Number of devices successfully polled
 */
uint8_t FEB_TPS_PollAll(FEB_TPS_Measurement_t *measurements, uint8_t count);

/**
 * @brief Poll all registered devices (scaled values for CAN)
 *
 * @param scaled Array to store results
 * @param count Number of elements in array
 * @return Number of devices successfully polled
 */
uint8_t FEB_TPS_PollAllScaled(FEB_TPS_MeasurementScaled_t *scaled, uint8_t count);

/**
 * @brief Poll all registered devices (raw values only)
 *
 * This is the most efficient batch operation - only reads raw registers.
 * Current and shunt voltage values are sign-magnitude converted to signed.
 *
 * @param bus_v_raw Array for raw bus voltage values (can be NULL)
 * @param current_raw Array for sign-corrected current values (can be NULL)
 * @param shunt_v_raw Array for sign-corrected shunt voltage values (can be NULL)
 * @param count Number of elements in arrays
 * @return Number of devices successfully polled
 */
uint8_t FEB_TPS_PollAllRaw(uint16_t *bus_v_raw, int16_t *current_raw,
                            int16_t *shunt_v_raw, uint8_t count);

/* ============================================================================
 * GPIO Control (Enable/Power-Good/Alert)
 * ============================================================================ */

/**
 * @brief Enable or disable a device via EN pin
 *
 * @param handle Device handle
 * @param enable true to enable, false to disable
 * @return FEB_TPS_OK on success, FEB_TPS_ERR_INVALID_ARG if no EN pin configured
 */
FEB_TPS_Status_t FEB_TPS_Enable(FEB_TPS_Handle_t handle, bool enable);

/**
 * @brief Read power-good status
 *
 * @param handle Device handle
 * @param pg_state Output: true if power is good
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_ReadPowerGood(FEB_TPS_Handle_t handle, bool *pg_state);

/**
 * @brief Read alert status
 *
 * @param handle Device handle
 * @param alert_active Output: true if alert is active
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_ReadAlert(FEB_TPS_Handle_t handle, bool *alert_active);

/**
 * @brief Enable all registered devices that have EN pins configured
 *
 * @param enable true to enable all, false to disable all
 * @return Number of devices successfully enabled/disabled
 */
uint8_t FEB_TPS_EnableAll(bool enable);

/**
 * @brief Read power-good status for all devices
 *
 * @param pg_states Array to store results (one per device)
 * @param count Number of elements in array
 * @return Number of devices successfully read
 */
uint8_t FEB_TPS_ReadAllPowerGood(bool *pg_states, uint8_t count);

/* ============================================================================
 * Configuration/Calibration
 * ============================================================================ */

/**
 * @brief Get device's current LSB value (for manual conversion)
 *
 * @param handle Device handle
 * @return Current LSB in Amps/bit
 */
float FEB_TPS_GetCurrentLSB(FEB_TPS_Handle_t handle);

/**
 * @brief Get device's calibration register value
 *
 * @param handle Device handle
 * @return Calibration register value
 */
uint16_t FEB_TPS_GetCalibration(FEB_TPS_Handle_t handle);

/**
 * @brief Read device unique ID
 *
 * @param handle Device handle
 * @param id Output: 16-bit device ID
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_ReadID(FEB_TPS_Handle_t handle, uint16_t *id);

/**
 * @brief Reconfigure a device (change shunt/current settings at runtime)
 *
 * @param handle Device handle
 * @param r_shunt_ohms New shunt resistor value
 * @param i_max_amps New maximum current
 * @return FEB_TPS_OK on success
 */
FEB_TPS_Status_t FEB_TPS_Reconfigure(FEB_TPS_Handle_t handle,
                                      float r_shunt_ohms,
                                      float i_max_amps);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert status code to string
 *
 * @param status Status code
 * @return String representation
 */
const char *FEB_TPS_StatusToString(FEB_TPS_Status_t status);

/**
 * @brief Get device name (from config)
 *
 * @param handle Device handle
 * @return Device name, or "Unknown" if not set
 */
const char *FEB_TPS_GetDeviceName(FEB_TPS_Handle_t handle);

/**
 * Convert a 16-bit sign-magnitude register value into a signed 16-bit integer.
 *
 * Interprets bit 15 as the sign (1 = negative) and bits 14:0 as the magnitude.
 *
 * @param raw Raw 16-bit sign-magnitude register value.
 * @return Signed 16-bit integer with sign applied (negative if input bit 15 is set).
 */
static inline int16_t FEB_TPS_SignMagnitude(uint16_t raw) {
    if (raw & 0x8000) {
        return -(int16_t)(raw & 0x7FFF);
    }
    return (int16_t)(raw & 0x7FFF);
}

#ifdef __cplusplus
}
#endif

#endif /* FEB_TPS_H */
