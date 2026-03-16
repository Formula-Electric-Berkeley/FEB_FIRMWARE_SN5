/**
 ******************************************************************************
 * @file           : feb_tps.c
 * @brief          : FEB TPS2482 Library Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

/* HAL includes - MUST be included before library headers */
#if defined(STM32F446xx) || defined(STM32F407xx) || defined(STM32F405xx) || defined(STM32F4)
#include "stm32f4xx_hal.h"
#elif defined(STM32F0) || defined(STM32F091xC) || defined(STM32F030x8)
#include "stm32f0xx_hal.h"
#elif defined(STM32G4) || defined(STM32G431xx) || defined(STM32G474xx)
#include "stm32g4xx_hal.h"
#else
/* Fallback - try to include generic HAL via main.h */
#include "main.h"
#endif

#include "feb_tps.h"
#include "feb_tps_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Buffer size for formatted log messages */
#define TPS_LOG_BUFFER_SIZE 128

/* ============================================================================
 * Global Context
 * ============================================================================ */

static FEB_TPS_Context_t feb_tps_ctx = {0};

/* ============================================================================
 * Internal Logging
 * ============================================================================ */

/**
 * @brief Internal logging function - formats message and calls user callback
 */
void FEB_TPS_Log(uint8_t level, const char *fmt, ...) {
    /* Early exit if no callback or level filtering */
    if (feb_tps_ctx.log_func == NULL) {
        return;
    }

    /* Skip if level is higher (less important) than configured level */
    if (level > feb_tps_ctx.log_level) {
        return;
    }

    /* Format message */
    char buffer[TPS_LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    /* Call user's logging callback */
    feb_tps_ctx.log_func((FEB_TPS_LogLevel_t)level, buffer);
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * Read a 16-bit register value from a TPS2482 device over I2C.
 *
 * On success stores the register's 16-bit value (MSB first / big-endian) into `value`.
 *
 * @param hi2c Pointer to the HAL I2C handle used for the transaction.
 * @param i2c_addr 7-bit I2C address of the TPS2482 device.
 * @param reg 8-bit register address to read.
 * @param value Output pointer that receives the 16-bit register value on success.
 * @return HAL_OK if the read succeeded, otherwise the HAL error status. 
 */
static HAL_StatusTypeDef feb_tps_read_reg(I2C_HandleTypeDef *hi2c,
                                           uint8_t i2c_addr,
                                           uint8_t reg,
                                           uint16_t *value) {
    uint8_t buf[2];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(hi2c, (uint16_t)(i2c_addr << 1), reg,
                               I2C_MEMADD_SIZE_8BIT, buf, 2,
                               feb_tps_ctx.i2c_timeout_ms);

    if (status == HAL_OK) {
        /* TPS2482 sends MSB first */
        *value = ((uint16_t)buf[0] << 8) | buf[1];
    }

    return status;
}

/**
 * Write a 16-bit value to a TPS2482 register over I2C, sending MSB first.
 *
 * @param i2c_addr 7-bit I2C device address.
 * @param reg      8-bit register address within the TPS2482.
 * @param value    16-bit register value to write (MSB transmitted first).
 * @returns HAL status code: `HAL_OK` on success, otherwise an error code such as
 *          `HAL_ERROR`, `HAL_BUSY`, or `HAL_TIMEOUT`.
 */
static HAL_StatusTypeDef feb_tps_write_reg(I2C_HandleTypeDef *hi2c,
                                            uint8_t i2c_addr,
                                            uint8_t reg,
                                            uint16_t value) {
    uint8_t buf[2];

    /* TPS2482 expects MSB first */
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);

    return HAL_I2C_Mem_Write(hi2c, (uint16_t)(i2c_addr << 1), reg,
                              I2C_MEMADD_SIZE_8BIT, buf, 2,
                              feb_tps_ctx.i2c_timeout_ms);
}

/**
 * Compute and update a device's measurement LSBs and calibration register from its shunt
 * resistance and configured maximum current.
 *
 * This updates the device fields `current_lsb`, `power_lsb`, and `cal_reg` using the
 * device's `i_max_amps` and `r_shunt_ohms` values.
 *
 * @param dev Pointer to the device to update; `i_max_amps` and `r_shunt_ohms` must be set. 
 */
static void feb_tps_compute_calibration(FEB_TPS_Device_t *dev) {
    dev->current_lsb = FEB_TPS_CALC_CURRENT_LSB(dev->i_max_amps);
    dev->power_lsb = FEB_TPS_CALC_POWER_LSB(dev->current_lsb);
    dev->cal_reg = FEB_TPS_CALC_CAL(dev->current_lsb, dev->r_shunt_ohms);
}

/**
 * Initialize the FEB TPS2482 library with an optional configuration.
 *
 * If `config` is provided, its `i2c_timeout_ms` overrides the default I2C timeout,
 * and `log_func`/`log_level` are registered for library logging. The call allocates
 * an internal I2C mutex and initializes global library state.
 *
 * @param config Optional pointer to library configuration; may be NULL.
 *               - If non-NULL and `i2c_timeout_ms > 0` that value is used;
 *                 otherwise the default timeout is applied.
 *               - If `log_func` is non-NULL it will be used for library logging;
 *                 `log_level` defaults to `FEB_TPS_LOG_INFO` when zero.
 *
 * @returns FEB_TPS_OK if the library is successfully initialized or was already initialized.
 * @returns FEB_TPS_ERR_NOT_INIT if initialization failed due to mutex creation failure (FreeRTOS build).
 */

FEB_TPS_Status_t FEB_TPS_Init(const FEB_TPS_LibConfig_t *config) {
    if (feb_tps_ctx.initialized) {
        return FEB_TPS_OK; /* Already initialized */
    }

    memset(&feb_tps_ctx, 0, sizeof(feb_tps_ctx));

    /* Set I2C timeout */
    if (config && config->i2c_timeout_ms > 0) {
        feb_tps_ctx.i2c_timeout_ms = config->i2c_timeout_ms;
    } else {
        feb_tps_ctx.i2c_timeout_ms = FEB_TPS_DEFAULT_I2C_TIMEOUT_MS;
    }

    /* Set logging configuration */
    if (config && config->log_func != NULL) {
        feb_tps_ctx.log_func = config->log_func;
        feb_tps_ctx.log_level = (config->log_level != 0) ?
                                 config->log_level : FEB_TPS_LOG_INFO;
    }

    /* Create mutex for FreeRTOS */
    feb_tps_ctx.i2c_mutex = FEB_TPS_MUTEX_CREATE();

#if FEB_TPS_USE_FREERTOS
    /* On FreeRTOS, mutex creation can fail if out of memory */
    if (feb_tps_ctx.i2c_mutex == NULL) {
        TPS_LOG_E("Mutex creation failed");
        return FEB_TPS_ERR_NOT_INIT;
    }
#endif

    feb_tps_ctx.initialized = true;

    TPS_LOG_I("TPS library initialized");

    return FEB_TPS_OK;
}

/**
 * Deinitializes the FEB TPS library, unregistering all devices and resetting internal state.
 *
 * Clears registered device entries, deletes the I2C mutex, and resets the library's global
 * context so the library returns to an uninitialized state.
 */
void FEB_TPS_DeInit(void) {
    if (!feb_tps_ctx.initialized) {
        return;
    }

    /* Unregister all devices */
    for (uint8_t i = 0; i < feb_tps_ctx.device_count; i++) {
        feb_tps_ctx.devices[i].initialized = false;
    }

    FEB_TPS_MUTEX_DELETE(feb_tps_ctx.i2c_mutex);

    memset(&feb_tps_ctx, 0, sizeof(feb_tps_ctx));
}

/**
 * Query whether the FEB TPS library has been initialized.
 *
 * @returns `true` if the library is initialized, `false` otherwise.
 */
bool FEB_TPS_IsInitialized(void) {
    return feb_tps_ctx.initialized;
}

/**
 * Register a TPS2482 device instance and program its configuration and calibration over I2C.
 *
 * Copies the provided device configuration into an internal slot, computes calibration
 * values, writes required registers (CONFIG, CAL and optionally MASK and ALERT_LIM),
 * reads the device ID (non-fatal), marks the device initialized and returns a handle.
 *
 * @param config Pointer to the device configuration to register (must be non-NULL, with valid I2C handle,
 *               positive shunt resistance and positive maximum current).
 * @param handle  Output pointer that will receive the device handle on success (must be non-NULL).
 *
 * @returns FEB_TPS_OK on successful registration and configuration.
 * @returns FEB_TPS_ERR_NOT_INIT if the library has not been initialized.
 * @returns FEB_TPS_ERR_INVALID_ARG if any required argument or configuration field is invalid.
 * @returns FEB_TPS_ERR_MAX_DEVICES if the maximum number of device slots is already registered.
 * @returns FEB_TPS_ERR_I2C if one of the required I2C register writes fails.
 */

FEB_TPS_Status_t FEB_TPS_DeviceRegister(const FEB_TPS_DeviceConfig_t *config,
                                         FEB_TPS_Handle_t *handle) {
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    if (config == NULL || handle == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    if (config->hi2c == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    if (config->r_shunt_ohms <= 0.0f || config->i_max_amps <= 0.0f) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    /* Find first available slot (supports stable handles after unregister) */
    FEB_TPS_Device_t *dev = NULL;
    for (uint8_t i = 0; i < FEB_TPS_MAX_DEVICES; i++) {
        if (!feb_tps_ctx.devices[i].in_use) {
            dev = &feb_tps_ctx.devices[i];
            break;
        }
    }

    if (dev == NULL) {
        return FEB_TPS_ERR_MAX_DEVICES;
    }

    /* Copy configuration */
    dev->hi2c = config->hi2c;
    dev->i2c_addr = config->i2c_addr;
    dev->r_shunt_ohms = config->r_shunt_ohms;
    dev->i_max_amps = config->i_max_amps;
    dev->name = config->name;

    /* Copy GPIO configuration */
    dev->en_gpio_port = config->en_gpio_port;
    dev->en_gpio_pin = config->en_gpio_pin;
    dev->pg_gpio_port = config->pg_gpio_port;
    dev->pg_gpio_pin = config->pg_gpio_pin;
    dev->alert_gpio_port = config->alert_gpio_port;
    dev->alert_gpio_pin = config->alert_gpio_pin;

    /* Compute calibration values */
    feb_tps_compute_calibration(dev);

    /* Lock I2C for device configuration */
    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    HAL_StatusTypeDef hal_status;
    FEB_TPS_Status_t status = FEB_TPS_OK;

    /* Write configuration register */
    uint16_t config_val = (config->config_reg != 0) ?
                          config->config_reg : FEB_TPS_CONFIG_REG_DEFAULT;
    hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                    FEB_TPS_REG_CONFIG, config_val);
    if (hal_status != HAL_OK) {
        TPS_LOG_E("I2C write CONFIG failed: %s", dev->name ? dev->name : "?");
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }

    /* Write calibration register */
    hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                    FEB_TPS_REG_CAL, dev->cal_reg);
    if (hal_status != HAL_OK) {
        TPS_LOG_E("I2C write CAL failed: %s", dev->name ? dev->name : "?");
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }

    /* Write mask register if specified */
    if (config->mask_reg != 0) {
        hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                        FEB_TPS_REG_MASK, config->mask_reg);
        if (hal_status != HAL_OK) {
            status = FEB_TPS_ERR_I2C;
            goto cleanup;
        }
    }

    /* Write alert limit if specified */
    if (config->alert_limit != 0) {
        hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                        FEB_TPS_REG_ALERT_LIM, config->alert_limit);
        if (hal_status != HAL_OK) {
            status = FEB_TPS_ERR_I2C;
            goto cleanup;
        }
    }

    /* Read device ID */
    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                   FEB_TPS_REG_ID, &dev->device_id);
    if (hal_status != HAL_OK) {
        /* Non-fatal - continue without ID */
        dev->device_id = 0;
    }

    dev->initialized = true;
    dev->in_use = true;
    feb_tps_ctx.device_count++;
    *handle = dev;

    TPS_LOG_I("Registered '%s' at 0x%02X (cal=0x%04X)",
              dev->name ? dev->name : "?", dev->i2c_addr, dev->cal_reg);

cleanup:
    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    return status;
}

/**
 * Unregisters a previously registered TPS device.
 *
 * Removes the device identified by `handle` from the library's internal device list. If removal succeeds the device array is compacted (last device moved into the freed slot) and the device count is decremented. The call is a no-op if `handle` is NULL or the device is not found.
 *
 * @param handle Handle returned by FEB_TPS_DeviceRegister identifying the device to remove.
 */
void FEB_TPS_DeviceUnregister(FEB_TPS_Handle_t handle) {
    if (handle == NULL) {
        return;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    /* Validate device is in our array and in use */
    bool found = false;
    int dev_index = -1;
    for (uint8_t i = 0; i < FEB_TPS_MAX_DEVICES; i++) {
        if (&feb_tps_ctx.devices[i] == dev && dev->in_use) {
            found = true;
            dev_index = i;
            break;
        }
    }

    if (!found) {
        return; /* Device not found or not in use */
    }

    /* Save name for logging before clearing */
    const char *name = dev->name;

    /* Mark slot as unused without relocating (preserves other handles) */
    dev->in_use = false;
    dev->initialized = false;
    memset(dev, 0, sizeof(FEB_TPS_Device_t));

    feb_tps_ctx.device_count--;

    TPS_LOG_I("Unregistered device '%s' at index %d", name ? name : "?", dev_index);
}

/**
 * Get a registered device handle by its zero-based index.
 *
 * @param index Zero-based device index to retrieve.
 * @returns Pointer to the device handle at the given index, or `NULL` if the index is out of range or the device is not initialized.
 */
FEB_TPS_Handle_t FEB_TPS_DeviceGetByIndex(uint8_t index) {
    /* Iterate through slots, counting only in-use devices */
    uint8_t found = 0;
    for (uint8_t i = 0; i < FEB_TPS_MAX_DEVICES; i++) {
        if (feb_tps_ctx.devices[i].in_use && feb_tps_ctx.devices[i].initialized) {
            if (found == index) {
                return &feb_tps_ctx.devices[i];
            }
            found++;
        }
    }

    return NULL;
}

/**
 * Get number of registered TPS2482 devices.
 *
 * @returns Number of currently registered devices. */
uint8_t FEB_TPS_DeviceGetCount(void) {
    return feb_tps_ctx.device_count;
}

/**
 * Polls a registered TPS2482 device and fills a measurement snapshot.
 *
 * Reads bus voltage, current (sign-magnitude), shunt voltage (sign-magnitude),
 * and power from the device over I2C and converts raw register values into
 * physical units which are stored in @p measurement. If an I2C error occurs
 * during any register read, the function stops and returns the error; fields
 * read before the failure remain populated.
 *
 * @param handle Handle to the device previously returned by DeviceRegister.
 * @param measurement Pointer to a FEB_TPS_Measurement_t structure that will be
 *        filled with raw and converted measurement values.
 *
 * @returns FEB_TPS_OK if all registers were read and converted successfully,
 *          FEB_TPS_ERR_INVALID_ARG if @p handle or @p measurement is NULL,
 *          FEB_TPS_ERR_NOT_INIT if the library or device is not initialized,
 *          FEB_TPS_ERR_I2C if an I2C transaction failed (partial results may be present).
 */

FEB_TPS_Status_t FEB_TPS_Poll(FEB_TPS_Handle_t handle,
                               FEB_TPS_Measurement_t *measurement) {
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    if (handle == NULL || measurement == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    if (!dev->initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    HAL_StatusTypeDef hal_status;
    uint16_t raw;
    FEB_TPS_Status_t status = FEB_TPS_OK;

    /* Read bus voltage */
    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                   FEB_TPS_REG_BUS_VOLT, &raw);
    if (hal_status != HAL_OK) {
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }
    measurement->bus_voltage_raw = raw;
    measurement->bus_voltage_v = (float)raw * FEB_TPS_CONV_VBUS_V_PER_LSB;

    /* Read current (sign-magnitude) */
    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                   FEB_TPS_REG_CURRENT, &raw);
    if (hal_status != HAL_OK) {
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }
    measurement->current_raw = feb_tps_sign_magnitude(raw);
    measurement->current_a = (float)measurement->current_raw * dev->current_lsb;

    /* Read shunt voltage (sign-magnitude) */
    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                   FEB_TPS_REG_SHUNT_VOLT, &raw);
    if (hal_status != HAL_OK) {
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }
    measurement->shunt_voltage_raw = feb_tps_sign_magnitude(raw);
    measurement->shunt_voltage_mv = (float)measurement->shunt_voltage_raw *
                                     FEB_TPS_CONV_VSHUNT_MV_PER_LSB;

    /* Read power */
    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                   FEB_TPS_REG_POWER, &raw);
    if (hal_status != HAL_OK) {
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }
    measurement->power_raw = raw;
    measurement->power_w = (float)raw * dev->power_lsb;

cleanup:
    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    return status;
}

/**
 * Read a device measurement and convert the results into scaled metric units.
 *
 * @param handle Handle of the registered TPS device to poll.
 * @param scaled If non-NULL and the poll succeeds, receives converted values:
 *        - bus_voltage_mv: bus voltage in millivolts
 *        - current_ma: current in milliamps (signed)
 *        - shunt_voltage_uv: shunt voltage in microvolts (signed)
 *        - power_mw: power in milliwatts
 * @returns FEB_TPS_Status_t `FEB_TPS_OK` if the measurement was read and conversion applied,
 *          otherwise an error status indicating why the poll failed.
 */
FEB_TPS_Status_t FEB_TPS_PollScaled(FEB_TPS_Handle_t handle,
                                     FEB_TPS_MeasurementScaled_t *scaled) {
    FEB_TPS_Measurement_t meas;
    FEB_TPS_Status_t status = FEB_TPS_Poll(handle, &meas);

    if (status == FEB_TPS_OK && scaled != NULL) {
        scaled->bus_voltage_mv = (uint32_t)(meas.bus_voltage_v * 1000.0f);
        scaled->current_ma = (int32_t)(meas.current_a * 1000.0f);
        scaled->shunt_voltage_uv = (int32_t)(meas.shunt_voltage_mv * 1000.0f);
        scaled->power_mw = (uint32_t)(meas.power_w * 1000.0f);
    }

    return status;
}

/**
 * Read the device bus voltage and provide it in volts.
 *
 * Reads the TPS device BUS_VOLT register over I2C, converts the raw value to
 * volts using the library conversion constant, and stores the result at
 * *voltage_v.
 *
 * @param handle Device handle previously returned by FEB_TPS_DeviceRegister.
 * @param voltage_v Pointer to a float that will receive the bus voltage in volts.
 * @returns FEB_TPS_OK on success.
 *          FEB_TPS_ERR_NOT_INIT if the library or the specified device is not initialized.
 *          FEB_TPS_ERR_INVALID_ARG if `handle` or `voltage_v` is NULL.
 *          FEB_TPS_ERR_I2C if the I2C register read fails.
 */
FEB_TPS_Status_t FEB_TPS_PollBusVoltage(FEB_TPS_Handle_t handle, float *voltage_v) {
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }
    if (handle == NULL || voltage_v == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    if (!dev->initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    uint16_t raw;
    HAL_StatusTypeDef hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                                     FEB_TPS_REG_BUS_VOLT, &raw);

    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    if (hal_status != HAL_OK) {
        return FEB_TPS_ERR_I2C;
    }

    *voltage_v = (float)raw * FEB_TPS_CONV_VBUS_V_PER_LSB;
    return FEB_TPS_OK;
}

/**
 * Read the device current register and provide the measured current in amperes.
 *
 * @param handle Handle to the registered TPS device.
 * @param current_a Pointer to store the measured current in amperes.
 * @returns FEB_TPS_OK on success.
 * @returns FEB_TPS_ERR_NOT_INIT if the library or device is not initialized.
 * @returns FEB_TPS_ERR_INVALID_ARG if `handle` or `current_a` is NULL.
 * @returns FEB_TPS_ERR_I2C if an I2C transfer error occurred while reading the register.
 */
FEB_TPS_Status_t FEB_TPS_PollCurrent(FEB_TPS_Handle_t handle, float *current_a) {
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }
    if (handle == NULL || current_a == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    if (!dev->initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    uint16_t raw;
    HAL_StatusTypeDef hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                                     FEB_TPS_REG_CURRENT, &raw);

    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    if (hal_status != HAL_OK) {
        return FEB_TPS_ERR_I2C;
    }

    *current_a = (float)feb_tps_sign_magnitude(raw) * dev->current_lsb;
    return FEB_TPS_OK;
}

/**
 * Read raw register values from a TPS device.
 *
 * Retrieves the raw 16-bit BUS_VOLT register and the signed (sign-magnitude)
 * raw values of the CURRENT and SHUNT_VOLT registers as requested.
 *
 * @param handle Device handle obtained from FEB_TPS_DeviceRegister.
 * @param bus_v_raw If non-NULL, receives the raw BUS_VOLT register value (16-bit).
 *                  If NULL, the BUS_VOLT register is not read.
 * @param current_raw If non-NULL, receives the signed raw CURRENT value after
 *                    sign-magnitude conversion. If NULL, the CURRENT register
 *                    is not read.
 * @param shunt_v_raw If non-NULL, receives the signed raw SHUNT_VOLT value after
 *                    sign-magnitude conversion. If NULL, the SHUNT_VOLT register
 *                    is not read.
 *
 * @returns `FEB_TPS_OK` on success,
 *          `FEB_TPS_ERR_INVALID_ARG` if the library is uninitialized or `handle` is NULL,
 *          `FEB_TPS_ERR_NOT_INIT` if the device is not initialized,
 *          `FEB_TPS_ERR_I2C` if an I2C register read fails.
 */
FEB_TPS_Status_t FEB_TPS_PollRaw(FEB_TPS_Handle_t handle,
                                  uint16_t *bus_v_raw,
                                  int16_t *current_raw,
                                  int16_t *shunt_v_raw) {
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    if (handle == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    if (!dev->initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    HAL_StatusTypeDef hal_status;
    uint16_t raw;
    FEB_TPS_Status_t status = FEB_TPS_OK;

    if (bus_v_raw != NULL) {
        hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                       FEB_TPS_REG_BUS_VOLT, &raw);
        if (hal_status != HAL_OK) {
            status = FEB_TPS_ERR_I2C;
            goto cleanup;
        }
        *bus_v_raw = raw;
    }

    if (current_raw != NULL) {
        hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                       FEB_TPS_REG_CURRENT, &raw);
        if (hal_status != HAL_OK) {
            status = FEB_TPS_ERR_I2C;
            goto cleanup;
        }
        *current_raw = feb_tps_sign_magnitude(raw);
    }

    if (shunt_v_raw != NULL) {
        hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                       FEB_TPS_REG_SHUNT_VOLT, &raw);
        if (hal_status != HAL_OK) {
            status = FEB_TPS_ERR_I2C;
            goto cleanup;
        }
        *shunt_v_raw = feb_tps_sign_magnitude(raw);
    }

cleanup:
    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    return status;
}

/**
 * Polls registered TPS devices and fills the provided measurements array.
 *
 * @param measurements Output array that will be populated with each device's measurement; must have space for at least `count` entries.
 * @param count Maximum number of devices to poll (and maximum entries to write into `measurements`).
 * @returns The number of devices successfully polled and written into `measurements` (0 if the library is not initialized or `measurements` is NULL). 
 */

uint8_t FEB_TPS_PollAll(FEB_TPS_Measurement_t *measurements, uint8_t count) {
    if (!feb_tps_ctx.initialized || measurements == NULL) {
        return 0;
    }

    uint8_t actual_count = (count < feb_tps_ctx.device_count) ?
                           count : feb_tps_ctx.device_count;
    uint8_t success_count = 0;

    for (uint8_t i = 0; i < actual_count; i++) {
        FEB_TPS_Handle_t handle = FEB_TPS_DeviceGetByIndex(i);
        if (handle != NULL) {
            if (FEB_TPS_Poll(handle, &measurements[i]) == FEB_TPS_OK) {
                success_count++;
            }
        }
    }

    return success_count;
}

/**
 * Polls up to `count` registered devices and fills `scaled` with scaled measurements.
 *
 * For each device up to min(count, registered device count), attempts to read and
 * scale measurements via FEB_TPS_PollScaled and store them into the corresponding
 * element of `scaled`.
 *
 * @param scaled Pointer to an array that will be filled with scaled measurements; must not be NULL.
 * @param count  Number of entries available in `scaled`.
 * @returns Number of devices successfully polled and written into `scaled`. Returns `0` if the library is not initialized or `scaled` is NULL.
 */
uint8_t FEB_TPS_PollAllScaled(FEB_TPS_MeasurementScaled_t *scaled, uint8_t count) {
    if (!feb_tps_ctx.initialized || scaled == NULL) {
        return 0;
    }

    uint8_t actual_count = (count < feb_tps_ctx.device_count) ?
                           count : feb_tps_ctx.device_count;
    uint8_t success_count = 0;

    for (uint8_t i = 0; i < actual_count; i++) {
        FEB_TPS_Handle_t handle = FEB_TPS_DeviceGetByIndex(i);
        if (handle != NULL) {
            if (FEB_TPS_PollScaled(handle, &scaled[i]) == FEB_TPS_OK) {
                success_count++;
            }
        }
    }

    return success_count;
}

/**
 * Read raw BUS_VOLT, CURRENT, and SHUNT_VOLT register values for up to `count` devices.
 *
 * For each registered device (by index) up to `count`, this fills the corresponding
 * element in the provided arrays with the raw register value. Any measurement pointer
 * may be NULL to skip that field. Devices that are not initialized are skipped.
 *
 * @param bus_v_raw  Buffer to receive BUS_VOLT raw values indexed by device; may be NULL.
 * @param current_raw Buffer to receive SIGN-MAGNITUDE CURRENT values indexed by device; may be NULL.
 * @param shunt_v_raw Buffer to receive SIGN-MAGNITUDE SHUNT_VOLT values indexed by device; may be NULL.
 * @param count      Maximum number of devices to poll (size of the provided buffers).
 * @returns Number of devices for which all requested fields were successfully read; returns 0 if the library is not initialized.
 */
uint8_t FEB_TPS_PollAllRaw(uint16_t *bus_v_raw, int16_t *current_raw,
                            int16_t *shunt_v_raw, uint8_t count) {
    if (!feb_tps_ctx.initialized) {
        return 0;
    }

    uint8_t actual_count = (count < feb_tps_ctx.device_count) ?
                           count : feb_tps_ctx.device_count;
    uint8_t success_count = 0;

    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    /* Use logical indexing (same as FEB_TPS_PollAll via DeviceGetByIndex) */
    uint8_t logical_index = 0;
    for (uint8_t i = 0; i < FEB_TPS_MAX_DEVICES && logical_index < actual_count; i++) {
        FEB_TPS_Device_t *dev = &feb_tps_ctx.devices[i];
        if (!dev->in_use || !dev->initialized) {
            continue;
        }

        HAL_StatusTypeDef hal_status;
        uint16_t raw;
        bool success = true;

        if (bus_v_raw != NULL) {
            hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                           FEB_TPS_REG_BUS_VOLT, &raw);
            if (hal_status == HAL_OK) {
                bus_v_raw[logical_index] = raw;
            } else {
                success = false;
            }
        }

        if (current_raw != NULL && success) {
            hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                           FEB_TPS_REG_CURRENT, &raw);
            if (hal_status == HAL_OK) {
                current_raw[logical_index] = feb_tps_sign_magnitude(raw);
            } else {
                success = false;
            }
        }

        if (shunt_v_raw != NULL && success) {
            hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                           FEB_TPS_REG_SHUNT_VOLT, &raw);
            if (hal_status == HAL_OK) {
                shunt_v_raw[logical_index] = feb_tps_sign_magnitude(raw);
            } else {
                success = false;
            }
        }

        if (success) {
            success_count++;
        }
        logical_index++;
    }

    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    return success_count;
}

/**
 * Control the device enable GPIO for a registered TPS device.
 *
 * Sets or clears the device's enable pin according to `enable`.
 *
 * @param handle Pointer to a registered FEB TPS device handle.
 * @param enable `true` to set (enable) the pin, `false` to reset (disable) it.
 * @returns `FEB_TPS_OK` on success, `FEB_TPS_ERR_INVALID_ARG` if `handle` is NULL
 *          or the device does not have an enable GPIO configured.
 */

FEB_TPS_Status_t FEB_TPS_Enable(FEB_TPS_Handle_t handle, bool enable) {
    if (handle == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    if (dev->en_gpio_port == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    HAL_GPIO_WritePin((GPIO_TypeDef *)dev->en_gpio_port, dev->en_gpio_pin,
                      enable ? GPIO_PIN_SET : GPIO_PIN_RESET);

    TPS_LOG_D("%s %s", dev->name ? dev->name : "?", enable ? "enabled" : "disabled");

    return FEB_TPS_OK;
}

/**
 * Read the device's power-good (PG) GPIO state.
 *
 * Sets *pg_state to `true` when the PG pin is high (GPIO_PIN_SET) and `false` when low.
 *
 * @param handle Device handle returned by FEB_TPS_DeviceRegister.
 * @param pg_state Pointer to a bool that will be updated with the PG state.
 * @returns FEB_TPS_OK on success, FEB_TPS_ERR_INVALID_ARG if `handle` or `pg_state` is NULL or the device has no PG GPIO configured.
 */
FEB_TPS_Status_t FEB_TPS_ReadPowerGood(FEB_TPS_Handle_t handle, bool *pg_state) {
    if (handle == NULL || pg_state == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    if (dev->pg_gpio_port == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    GPIO_PinState state = HAL_GPIO_ReadPin((GPIO_TypeDef *)dev->pg_gpio_port,
                                            dev->pg_gpio_pin);
    *pg_state = (state == GPIO_PIN_SET);

    return FEB_TPS_OK;
}

/**
 * Read the device's alert pin and report whether an alert is active.
 *
 * @param handle Pointer to the registered device handle.
 * @param alert_active Output set to `true` if the device's alert is active (alert pin is active-low), `false` otherwise.
 * @returns `FEB_TPS_OK` on success, `FEB_TPS_ERR_INVALID_ARG` if `handle` or `alert_active` is NULL or the device has no alert GPIO configured.
 */
FEB_TPS_Status_t FEB_TPS_ReadAlert(FEB_TPS_Handle_t handle, bool *alert_active) {
    if (handle == NULL || alert_active == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    if (dev->alert_gpio_port == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    GPIO_PinState state = HAL_GPIO_ReadPin((GPIO_TypeDef *)dev->alert_gpio_port,
                                            dev->alert_gpio_pin);
    /* Alert is typically active low */
    *alert_active = (state == GPIO_PIN_RESET);

    return FEB_TPS_OK;
}

/**
 * Set the enable GPIO pin for all registered devices that have an enable GPIO configured.
 * @param enable `true` to set the enable pin (enable device), `false` to reset the pin (disable device).
 * @returns Number of devices whose enable GPIO pin was written. */
uint8_t FEB_TPS_EnableAll(bool enable) {
    uint8_t count = 0;

    for (uint8_t i = 0; i < FEB_TPS_MAX_DEVICES; i++) {
        FEB_TPS_Device_t *dev = &feb_tps_ctx.devices[i];
        if (dev->in_use && dev->initialized && dev->en_gpio_port != NULL) {
            HAL_GPIO_WritePin((GPIO_TypeDef *)dev->en_gpio_port, dev->en_gpio_pin,
                              enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
            count++;
        }
    }

    return count;
}

/**
 * Read the power-good (PG) input state for up to `count` registered devices.
 *
 * Populates `pg_states` with the PG boolean for each device index (true = PG asserted,
 * false = PG not asserted). If a device is not initialized or does not have a PG GPIO
 * configured, its entry is set to false.
 *
 * @param pg_states Pointer to an array where PG states will be written; must have at least
 *                  `count` elements.
 * @param count     Maximum number of device entries to read from (reads at most the number
 *                  of currently registered devices).
 * @returns         Number of devices for which a PG GPIO was present and read successfully.
 */
uint8_t FEB_TPS_ReadAllPowerGood(bool *pg_states, uint8_t count) {
    if (pg_states == NULL) {
        return 0;
    }

    uint8_t actual_count = (count < feb_tps_ctx.device_count) ?
                           count : feb_tps_ctx.device_count;
    uint8_t success_count = 0;

    /* Use logical indexing (same as FEB_TPS_PollAllRaw) */
    uint8_t logical_index = 0;
    for (uint8_t i = 0; i < FEB_TPS_MAX_DEVICES && logical_index < actual_count; i++) {
        FEB_TPS_Device_t *dev = &feb_tps_ctx.devices[i];
        if (!dev->in_use || !dev->initialized) {
            continue;
        }

        if (dev->pg_gpio_port != NULL) {
            GPIO_PinState state = HAL_GPIO_ReadPin((GPIO_TypeDef *)dev->pg_gpio_port,
                                                    dev->pg_gpio_pin);
            pg_states[logical_index] = (state == GPIO_PIN_SET);
            success_count++;
        } else {
            pg_states[logical_index] = false;
        }
        logical_index++;
    }

    return success_count;
}

/**
 * Retrieve the current LSB (amperes per least-significant bit) configured for a device.
 * @param handle Pointer to a registered FEB TPS device handle; may be NULL.
 * @returns The device's `current_lsb` value (A/LSB). Returns `0.0f` if `handle` is NULL.
 */

float FEB_TPS_GetCurrentLSB(FEB_TPS_Handle_t handle) {
    if (handle == NULL) {
        return 0.0f;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    return dev->current_lsb;
}

/**
 * Get the stored calibration register value for a TPS device.
 * @param handle Device handle returned by FEB_TPS_DeviceRegister.
 * @returns The device's calibration register value (uint16_t); `0` if `handle` is NULL.
 */
uint16_t FEB_TPS_GetCalibration(FEB_TPS_Handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    return dev->cal_reg;
}

/**
 * Read the TPS2482 device ID register for a registered device.
 *
 * @param handle Handle to the registered device; must refer to an initialized device.
 * @param id Pointer to a uint16_t that will be set to the device ID on success.
 * @returns FEB_TPS_OK if the ID was read and stored in `*id`,
 *          FEB_TPS_ERR_INVALID_ARG if `handle` or `id` is NULL,
 *          FEB_TPS_ERR_NOT_INIT if the provided handle is not initialized,
 *          FEB_TPS_ERR_I2C if an I2C transaction failed.
 */
FEB_TPS_Status_t FEB_TPS_ReadID(FEB_TPS_Handle_t handle, uint16_t *id) {
    if (handle == NULL || id == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    if (!dev->initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    HAL_StatusTypeDef hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                                     FEB_TPS_REG_ID, id);

    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    return (hal_status == HAL_OK) ? FEB_TPS_OK : FEB_TPS_ERR_I2C;
}

/**
 * Reconfigure a registered device's shunt resistor and maximum current and apply the updated calibration.
 *
 * Updates the device's stored r_shunt_ohms and i_max_amps, recomputes calibration values, and writes the new
 * calibration register to the device over I2C.
 *
 * @param handle Pointer to the device handle previously returned by FEB_TPS_DeviceRegister.
 * @param r_shunt_ohms Shunt resistor value in ohms (must be > 0).
 * @param i_max_amps Maximum expected current in amperes (must be > 0).
 *
 * @returns FEB_TPS_OK if the calibration was updated and written to the device successfully,
 *          FEB_TPS_ERR_INVALID_ARG if any argument is invalid,
 *          FEB_TPS_ERR_NOT_INIT if the device is not initialized/registered,
 *          FEB_TPS_ERR_I2C if the I2C write of the calibration register failed.
 */
FEB_TPS_Status_t FEB_TPS_Reconfigure(FEB_TPS_Handle_t handle,
                                      float r_shunt_ohms,
                                      float i_max_amps) {
    if (handle == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    if (r_shunt_ohms <= 0.0f || i_max_amps <= 0.0f) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    if (!dev->initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    /* Update configuration */
    dev->r_shunt_ohms = r_shunt_ohms;
    dev->i_max_amps = i_max_amps;
    feb_tps_compute_calibration(dev);

    /* Write new calibration to device */
    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    HAL_StatusTypeDef hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                                      FEB_TPS_REG_CAL, dev->cal_reg);

    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    return (hal_status == HAL_OK) ? FEB_TPS_OK : FEB_TPS_ERR_I2C;
}

/**
 * Convert a FEB_TPS_Status_t value to a human-readable string.
 * @param status Status value to convert.
 * @returns A null-terminated string describing the status (e.g. "OK", "Invalid argument", "I2C error", "Not initialized", "Config mismatch", "Max devices exceeded", "Timeout", or "Unknown").
 */

const char *FEB_TPS_StatusToString(FEB_TPS_Status_t status) {
    switch (status) {
        case FEB_TPS_OK:                return "OK";
        case FEB_TPS_ERR_INVALID_ARG:   return "Invalid argument";
        case FEB_TPS_ERR_I2C:           return "I2C error";
        case FEB_TPS_ERR_NOT_INIT:      return "Not initialized";
        case FEB_TPS_ERR_CONFIG_MISMATCH: return "Config mismatch";
        case FEB_TPS_ERR_MAX_DEVICES:   return "Max devices exceeded";
        case FEB_TPS_ERR_TIMEOUT:       return "Timeout";
        default:                        return "Unknown";
    }
}

/**
 * Get the human-readable name for a TPS device handle.
 *
 * @param handle Device handle returned by FEB_TPS_DeviceRegister; may be NULL.
 * @returns Pointer to the device name string, or "Unknown" if the handle is NULL or the device name is not set.
 */
const char *FEB_TPS_GetDeviceName(FEB_TPS_Handle_t handle) {
    if (handle == NULL) {
        return "Unknown";
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    return (dev->name != NULL) ? dev->name : "Unknown";
}
