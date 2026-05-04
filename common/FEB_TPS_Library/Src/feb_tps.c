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

    /* Retry loop for transient I2C errors */
    for (uint8_t retry = 0; retry < FEB_TPS_I2C_MAX_RETRIES; retry++) {
        status = HAL_I2C_Mem_Read(hi2c, (uint16_t)(i2c_addr << 1), reg,
                                   I2C_MEMADD_SIZE_8BIT, buf, 2,
                                   feb_tps_ctx.i2c_timeout_ms);

        if (status == HAL_OK) {
            /* TPS2482 sends MSB first */
            *value = ((uint16_t)buf[0] << 8) | buf[1];
            return status;
        }

        /* Brief delay before retry (only if not last attempt) */
        if (retry < FEB_TPS_I2C_MAX_RETRIES - 1) {
            FEB_TPS_DELAY_MS(1);
        }
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
    HAL_StatusTypeDef status;

    /* TPS2482 expects MSB first */
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);

    /* DEBUG: gate diagnostic logging to first 10 write_reg calls so the
     * 2100-attempt failure storm cannot saturate the UART. */
    static uint32_t call_count = 0;
    bool log_this = (call_count < 10);
    call_count++;

    if (log_this) {
        GPIO_PinState scl = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8);
        GPIO_PinState sda = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9);
        TPS_LOG_I("WR#%lu addr7=0x%02X reg=0x%02X val=0x%04X "
                  "preState=0x%02X preErr=0x%08lX SCL=%d SDA=%d",
                  (unsigned long)call_count, i2c_addr, reg, value,
                  (unsigned)hi2c->State, (unsigned long)hi2c->ErrorCode,
                  (int)scl, (int)sda);
        HAL_StatusTypeDef rdy = HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(i2c_addr << 1), 1, 50);
        TPS_LOG_I("WR#%lu IsDeviceReady=%d (0=OK 1=ERR 2=BUSY 3=TIMEOUT) "
                  "postRdyState=0x%02X postRdyErr=0x%08lX",
                  (unsigned long)call_count, (int)rdy,
                  (unsigned)hi2c->State, (unsigned long)hi2c->ErrorCode);
    }

    /* Retry loop for transient I2C errors */
    for (uint8_t retry = 0; retry < FEB_TPS_I2C_MAX_RETRIES; retry++) {
        status = HAL_I2C_Mem_Write(hi2c, (uint16_t)(i2c_addr << 1), reg,
                                    I2C_MEMADD_SIZE_8BIT, buf, 2,
                                    feb_tps_ctx.i2c_timeout_ms);

        if (log_this && retry == 0) {
            TPS_LOG_E("WR#%lu post status=%d State=0x%02X ErrorCode=0x%08lX "
                      "[BERR=%d ARLO=%d AF=%d OVR=%d TIMEOUT=%d SIZE=%d]",
                      (unsigned long)call_count, (int)status,
                      (unsigned)hi2c->State, (unsigned long)hi2c->ErrorCode,
                      !!(hi2c->ErrorCode & HAL_I2C_ERROR_BERR),
                      !!(hi2c->ErrorCode & HAL_I2C_ERROR_ARLO),
                      !!(hi2c->ErrorCode & HAL_I2C_ERROR_AF),
                      !!(hi2c->ErrorCode & HAL_I2C_ERROR_OVR),
                      !!(hi2c->ErrorCode & HAL_I2C_ERROR_TIMEOUT),
                      !!(hi2c->ErrorCode & HAL_I2C_ERROR_SIZE));
        }

        if (status == HAL_OK) {
            return status;
        }

        /* Brief delay before retry (only if not last attempt) */
        if (retry < FEB_TPS_I2C_MAX_RETRIES - 1) {
            FEB_TPS_DELAY_MS(1);
        }
    }

    return status;
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

    /* Calculate calibration with range validation to prevent truncation issues */
    float cal_float = 0.00512f / (dev->current_lsb * dev->r_shunt_ohms);

    if (cal_float > 65535.0f) {
        TPS_LOG_W("CAL overflow (%.0f > 65535) for %s, clamping to 0xFFFF",
                  cal_float, dev->name ? dev->name : "?");
        dev->cal_reg = 0xFFFF;
    } else if (cal_float < 1.0f) {
        TPS_LOG_W("CAL underflow (%.6f < 1) for %s, clamping to 0x0001",
                  cal_float, dev->name ? dev->name : "?");
        dev->cal_reg = 0x0001;
    } else {
        dev->cal_reg = (uint16_t)cal_float;
    }
}

/**
 * Initialize the FEB TPS2482 library with an optional configuration.
 *
 * If `config` is provided, its `i2c_timeout_ms` overrides the default I2C timeout,
 * and `log_func`/`log_level` are registered for library logging. The call initializes
 * global library state.
 *
 * @param config Optional pointer to library configuration; may be NULL in bare-metal mode.
 *               - If non-NULL and `i2c_timeout_ms > 0` that value is used;
 *                 otherwise the default timeout is applied.
 *               - If `log_func` is non-NULL it will be used for library logging;
 *                 `log_level` defaults to `FEB_TPS_LOG_INFO` when zero.
 *               - In FreeRTOS mode, `data_mutex` and `i2c_mutex` must be provided
 *                 (created externally in CubeMX/.ioc).
 *
 * @returns FEB_TPS_OK if the library is successfully initialized or was already initialized.
 * @returns FEB_TPS_ERR_INVALID_ARG if required mutexes are not provided (FreeRTOS build only).
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

#if FEB_TPS_USE_FREERTOS
    /* Validate and store user-provided mutexes (NOT created internally) */
    if (config == NULL || config->data_mutex == NULL || config->i2c_mutex == NULL) {
        TPS_LOG_E("Required mutexes not provided");
        return FEB_TPS_ERR_INVALID_ARG;
    }
    feb_tps_ctx.data_mutex = config->data_mutex;
    feb_tps_ctx.i2c_mutex = config->i2c_mutex;
    feb_tps_ctx.poll_interval_ms = (config->poll_interval_ms > 0) ?
                                    config->poll_interval_ms :
                                    FEB_TPS_DEFAULT_POLL_INTERVAL_MS;
    feb_tps_ctx.get_tick_ms = HAL_GetTick; /* Default to HAL_GetTick */
#else
    /* Bare-metal: Initialize local mutex storage */
    feb_tps_ctx.i2c_mutex = 0;
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

    /*
     * NOTE: We do NOT delete user-provided mutexes.
     * The user created them in CubeMX/.ioc and owns their lifecycle.
     * We only clear our references.
     *
     * The memset below clears all device slots (including initialized and
     * in_use flags) regardless of array sparsity, so no explicit loop is needed.
     */

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

    /* Warn if I2C address is outside TPS2482 typical range (0x40-0x4F) */
    if (config->i2c_addr < 0x40 || config->i2c_addr > 0x4F) {
        TPS_LOG_W("I2C address 0x%02X outside TPS2482 range (0x40-0x4F)",
                  config->i2c_addr);
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
 * Marks the device slot as unused and clears its data. The slot is NOT compacted
 * (no array relocation), which ensures that other device handles remain valid.
 * The device count is decremented. The call is a no-op if `handle` is NULL or
 * the device is not found.
 *
 * @param handle Handle returned by FEB_TPS_DeviceRegister identifying the device to remove.
 *
 * @warning After unregistering, the handle becomes invalid. Using a stale handle
 *          after unregister results in undefined behavior. If the slot is reused
 *          by a subsequent DeviceRegister call, the old handle may incorrectly
 *          appear to work but will reference the wrong device. Callers must
 *          set their handle variables to NULL after unregistering.
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

    /* Lock mutex to prevent concurrent access during unregister */
    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    /* Clear slot data (memset zeros in_use and initialized flags) */
    memset(dev, 0, sizeof(FEB_TPS_Device_t));
    feb_tps_ctx.device_count--;

    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

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
        /* Log diagnostic warning if clamping negative values */
        if (meas.bus_voltage_v < 0.0f) {
            TPS_LOG_W("Clamped negative bus_voltage: %.3f V", meas.bus_voltage_v);
        }
        if (meas.power_w < 0.0f) {
            TPS_LOG_W("Clamped negative power: %.3f W", meas.power_w);
        }

        /* Clamp unsigned values to prevent undefined behavior from negative floats */
        scaled->bus_voltage_mv = (meas.bus_voltage_v >= 0.0f) ?
                                  (uint32_t)(meas.bus_voltage_v * 1000.0f) : 0;
        scaled->current_ma = (int32_t)(meas.current_a * 1000.0f);
        scaled->shunt_voltage_uv = (int32_t)(meas.shunt_voltage_mv * 1000.0f);
        scaled->power_mw = (meas.power_w >= 0.0f) ?
                           (uint32_t)(meas.power_w * 1000.0f) : 0;
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

    /* Zero-initialize output to prevent stale data on partial failure */
    memset(measurements, 0, actual_count * sizeof(FEB_TPS_Measurement_t));

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

    /* Zero-initialize output to prevent stale data on partial failure */
    memset(scaled, 0, actual_count * sizeof(FEB_TPS_MeasurementScaled_t));

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
 * @param bus_v_raw    Buffer to receive BUS_VOLT raw values indexed by device; may be NULL.
 * @param current_raw  Buffer to receive SIGN-MAGNITUDE CURRENT values indexed by device; may be NULL.
 * @param shunt_v_raw  Buffer to receive SIGN-MAGNITUDE SHUNT_VOLT values indexed by device; may be NULL.
 * @param count        Maximum number of devices to poll (size of the provided buffers).
 * @param success_mask Output bitmask indicating which device indices succeeded
 *                     (bit N set = device N polled successfully). May be NULL.
 * @returns Number of devices for which all requested fields were successfully read; returns 0 if the library is not initialized.
 */
uint8_t FEB_TPS_PollAllRaw(uint16_t *bus_v_raw, int16_t *current_raw,
                            int16_t *shunt_v_raw, uint8_t count,
                            uint8_t *success_mask) {
    if (!feb_tps_ctx.initialized) {
        return 0;
    }

    uint8_t actual_count = (count < feb_tps_ctx.device_count) ?
                           count : feb_tps_ctx.device_count;
    uint8_t success_count = 0;

    /* Initialize success_mask if provided */
    if (success_mask != NULL) {
        *success_mask = 0;
    }

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

        /* Use temporary variables to avoid partial data on failure */
        uint16_t temp_bus_v = 0;
        int16_t temp_current = 0;
        int16_t temp_shunt_v = 0;

        if (bus_v_raw != NULL) {
            hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                           FEB_TPS_REG_BUS_VOLT, &raw);
            if (hal_status == HAL_OK) {
                temp_bus_v = raw;
            } else {
                success = false;
            }
        }

        if (current_raw != NULL && success) {
            hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                           FEB_TPS_REG_CURRENT, &raw);
            if (hal_status == HAL_OK) {
                temp_current = feb_tps_sign_magnitude(raw);
            } else {
                success = false;
            }
        }

        if (shunt_v_raw != NULL && success) {
            hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                           FEB_TPS_REG_SHUNT_VOLT, &raw);
            if (hal_status == HAL_OK) {
                temp_shunt_v = feb_tps_sign_magnitude(raw);
            } else {
                success = false;
            }
        }

        /*
         * Always write to output arrays to maintain index alignment with
         * FEB_TPS_DeviceGetByIndex(). Use zero values for failed devices.
         * Callers can use success_mask to identify which indices succeeded.
         */
        if (bus_v_raw != NULL) {
            bus_v_raw[logical_index] = success ? temp_bus_v : 0;
        }
        if (current_raw != NULL) {
            current_raw[logical_index] = success ? temp_current : 0;
        }
        if (shunt_v_raw != NULL) {
            shunt_v_raw[logical_index] = success ? temp_shunt_v : 0;
        }
        if (success) {
            if (success_mask != NULL) {
                *success_mask |= (1u << logical_index);
            }
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
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    if (handle == NULL || pg_state == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    if (!dev->initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

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
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    if (handle == NULL || alert_active == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    if (!dev->initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

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
    if (!feb_tps_ctx.initialized) {
        return 0;
    }

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
    if (!feb_tps_ctx.initialized) {
        return 0;
    }

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
    if (!feb_tps_ctx.initialized || handle == NULL) {
        return 0.0f;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    if (!dev->initialized || !dev->in_use) {
        return 0.0f;
    }

    return dev->current_lsb;
}

/**
 * Get the stored calibration register value for a TPS device.
 * @param handle Device handle returned by FEB_TPS_DeviceRegister.
 * @returns The device's calibration register value (uint16_t); `0` if `handle` is NULL.
 */
uint16_t FEB_TPS_GetCalibration(FEB_TPS_Handle_t handle) {
    if (!feb_tps_ctx.initialized || handle == NULL) {
        return 0;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    if (!dev->initialized || !dev->in_use) {
        return 0;
    }

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
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

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
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

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

    /*
     * Compute calibration values to temporaries first.
     * Only update the device struct after I2C write succeeds to avoid
     * cache-hardware mismatch on I2C failure.
     */
    float new_current_lsb = FEB_TPS_CALC_CURRENT_LSB(i_max_amps);
    float new_power_lsb = FEB_TPS_CALC_POWER_LSB(new_current_lsb);
    uint16_t new_cal_reg = FEB_TPS_CALC_CAL(new_current_lsb, r_shunt_ohms);

    /* Write new calibration to device */
    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    HAL_StatusTypeDef hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                                      FEB_TPS_REG_CAL, new_cal_reg);

    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    if (hal_status != HAL_OK) {
        return FEB_TPS_ERR_I2C;
    }

    /* I2C write succeeded - update device cache */
    dev->r_shunt_ohms = r_shunt_ohms;
    dev->i_max_amps = i_max_amps;
    dev->current_lsb = new_current_lsb;
    dev->power_lsb = new_power_lsb;
    dev->cal_reg = new_cal_reg;

    return FEB_TPS_OK;
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
 * Attempt to recover all I2C buses used by registered TPS devices.
 *
 * This function disables and re-enables each unique I2C peripheral to clear any
 * stuck bus conditions that may occur when I2C transactions are interrupted
 * (e.g., by timer interrupts with blocking operations).
 *
 * @returns FEB_TPS_OK if all bus recovery attempts succeeded,
 *          FEB_TPS_ERR_NOT_INIT if the library is not initialized or no devices are registered,
 *          FEB_TPS_ERR_I2C if one or more bus recovery attempts failed.
 */
FEB_TPS_Status_t FEB_TPS_BusRecovery(void) {
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    /* Collect unique I2C handles from all registered devices */
    I2C_HandleTypeDef *unique_hi2c[FEB_TPS_MAX_DEVICES];
    uint8_t unique_count = 0;

    for (uint8_t i = 0; i < FEB_TPS_MAX_DEVICES; i++) {
        if (!feb_tps_ctx.devices[i].in_use || feb_tps_ctx.devices[i].hi2c == NULL) {
            continue;
        }

        I2C_HandleTypeDef *hi2c = feb_tps_ctx.devices[i].hi2c;

        /* Check if already in unique list */
        bool found = false;
        for (uint8_t j = 0; j < unique_count; j++) {
            if (unique_hi2c[j] == hi2c) {
                found = true;
                break;
            }
        }

        if (!found) {
            unique_hi2c[unique_count++] = hi2c;
        }
    }

    if (unique_count == 0) {
        return FEB_TPS_ERR_NOT_INIT;
    }

    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    FEB_TPS_Status_t overall_status = FEB_TPS_OK;

    /* Recover each unique I2C bus */
    for (uint8_t i = 0; i < unique_count; i++) {
        HAL_StatusTypeDef hal_status;

        hal_status = HAL_I2C_DeInit(unique_hi2c[i]);
        if (hal_status != HAL_OK) {
            TPS_LOG_E("I2C DeInit failed for bus %d: %d", i, hal_status);
            overall_status = FEB_TPS_ERR_I2C;
            continue; /* Try to recover other buses */
        }

        hal_status = HAL_I2C_Init(unique_hi2c[i]);
        if (hal_status != HAL_OK) {
            TPS_LOG_E("I2C Init failed for bus %d: %d", i, hal_status);
            overall_status = FEB_TPS_ERR_I2C;
            continue;
        }

        TPS_LOG_I("I2C bus %d recovery succeeded", i);
    }

    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    return overall_status;
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

/* ============================================================================
 * Cached Data API (FreeRTOS Mode Only)
 * ============================================================================ */

#if FEB_TPS_USE_FREERTOS

float FEB_TPS_GetVoltage(FEB_TPS_Handle_t handle) {
    if (!feb_tps_ctx.initialized || handle == NULL) {
        return 0.0f;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    osMutexAcquire(feb_tps_ctx.data_mutex, osWaitForever);
    float val = dev->cached_valid ? dev->cached_voltage_v : 0.0f;
    osMutexRelease(feb_tps_ctx.data_mutex);

    return val;
}

float FEB_TPS_GetCurrent(FEB_TPS_Handle_t handle) {
    if (!feb_tps_ctx.initialized || handle == NULL) {
        return 0.0f;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    osMutexAcquire(feb_tps_ctx.data_mutex, osWaitForever);
    float val = dev->cached_valid ? dev->cached_current_a : 0.0f;
    osMutexRelease(feb_tps_ctx.data_mutex);

    return val;
}

float FEB_TPS_GetPower(FEB_TPS_Handle_t handle) {
    if (!feb_tps_ctx.initialized || handle == NULL) {
        return 0.0f;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    osMutexAcquire(feb_tps_ctx.data_mutex, osWaitForever);
    float val = dev->cached_valid ? dev->cached_power_w : 0.0f;
    osMutexRelease(feb_tps_ctx.data_mutex);

    return val;
}

uint32_t FEB_TPS_GetLastUpdateTime(FEB_TPS_Handle_t handle) {
    if (!feb_tps_ctx.initialized || handle == NULL) {
        return 0;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    osMutexAcquire(feb_tps_ctx.data_mutex, osWaitForever);
    uint32_t val = dev->cached_last_update_ms;
    osMutexRelease(feb_tps_ctx.data_mutex);

    return val;
}

bool FEB_TPS_IsCacheValid(FEB_TPS_Handle_t handle) {
    if (!feb_tps_ctx.initialized || handle == NULL) {
        return false;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    osMutexAcquire(feb_tps_ctx.data_mutex, osWaitForever);
    bool val = dev->cached_valid;
    osMutexRelease(feb_tps_ctx.data_mutex);

    return val;
}

FEB_TPS_Status_t FEB_TPS_GetCachedData(FEB_TPS_Handle_t handle, FEB_TPS_CachedData_t *data) {
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }
    if (handle == NULL || data == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;

    osMutexAcquire(feb_tps_ctx.data_mutex, osWaitForever);
    data->voltage_v = dev->cached_voltage_v;
    data->current_a = dev->cached_current_a;
    data->power_w = dev->cached_power_w;
    data->last_update_ms = dev->cached_last_update_ms;
    data->valid = dev->cached_valid;
    osMutexRelease(feb_tps_ctx.data_mutex);

    return FEB_TPS_OK;
}

void FEB_TPS_PollAllDevices(void) {
    if (!feb_tps_ctx.initialized) {
        return;
    }

    uint32_t now = feb_tps_ctx.get_tick_ms ? feb_tps_ctx.get_tick_ms() : 0;

    for (uint8_t i = 0; i < FEB_TPS_MAX_DEVICES; i++) {
        FEB_TPS_Device_t *dev = &feb_tps_ctx.devices[i];
        if (!dev->in_use || !dev->initialized) {
            continue;
        }

        /* Poll this device */
        FEB_TPS_Measurement_t meas;
        FEB_TPS_Status_t status = FEB_TPS_Poll(dev, &meas);

        /* Update cache under data_mutex */
        osMutexAcquire(feb_tps_ctx.data_mutex, osWaitForever);
        if (status == FEB_TPS_OK) {
            dev->cached_voltage_v = meas.bus_voltage_v;
            dev->cached_current_a = meas.current_a;
            dev->cached_power_w = meas.power_w;
            dev->cached_last_update_ms = now;
            dev->cached_valid = true;
        } else {
            dev->cached_valid = false;
        }
        osMutexRelease(feb_tps_ctx.data_mutex);
    }
}

/* ============================================================================
 * Weak Task Function Implementation (FreeRTOS Mode)
 * ============================================================================ */

/**
 * @brief Weak default polling task
 *
 * Polls all TPS devices periodically and updates cached data.
 * Override to customize polling behavior.
 *
 * @param argument Not used (pass NULL from CubeMX)
 */
__attribute__((weak)) void FEB_TPS_PollTaskFunc(void *argument) {
    (void)argument; /* Unused */

    for (;;) {
        FEB_TPS_PollAllDevices();
        /*
         * Use poll_interval_ms with fallback to default if zero.
         * This prevents osDelay(0) spinning if init hasn't completed.
         */
        uint32_t delay = feb_tps_ctx.poll_interval_ms;
        if (delay == 0) {
            delay = FEB_TPS_DEFAULT_POLL_INTERVAL_MS;
        }
        osDelay(delay);
    }
}

#endif /* FEB_TPS_USE_FREERTOS */
