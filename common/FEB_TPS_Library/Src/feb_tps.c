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

/* ============================================================================
 * Global Context
 * ============================================================================ */

static FEB_TPS_Context_t feb_tps_ctx = {0};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Read a 16-bit register from TPS2482
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
 * @brief Write a 16-bit register to TPS2482
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
 * @brief Compute calibration values for a device
 */
static void feb_tps_compute_calibration(FEB_TPS_Device_t *dev) {
    dev->current_lsb = FEB_TPS_CALC_CURRENT_LSB(dev->i_max_amps);
    dev->power_lsb = FEB_TPS_CALC_POWER_LSB(dev->current_lsb);
    dev->cal_reg = FEB_TPS_CALC_CAL(dev->current_lsb, dev->r_shunt_ohms);
}

/* ============================================================================
 * Library Initialization
 * ============================================================================ */

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

    /* Create mutex for FreeRTOS */
    feb_tps_ctx.i2c_mutex = FEB_TPS_MUTEX_CREATE();

    feb_tps_ctx.initialized = true;

    return FEB_TPS_OK;
}

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

bool FEB_TPS_IsInitialized(void) {
    return feb_tps_ctx.initialized;
}

/* ============================================================================
 * Device Management
 * ============================================================================ */

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

    if (feb_tps_ctx.device_count >= FEB_TPS_MAX_DEVICES) {
        return FEB_TPS_ERR_MAX_DEVICES;
    }

    /* Get next available slot */
    FEB_TPS_Device_t *dev = &feb_tps_ctx.devices[feb_tps_ctx.device_count];

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
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }

    /* Write calibration register */
    hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                    FEB_TPS_REG_CAL, dev->cal_reg);
    if (hal_status != HAL_OK) {
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
    feb_tps_ctx.device_count++;
    *handle = dev;

cleanup:
    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    return status;
}

void FEB_TPS_DeviceUnregister(FEB_TPS_Handle_t handle) {
    if (handle == NULL) {
        return;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    dev->initialized = false;
}

FEB_TPS_Handle_t FEB_TPS_DeviceGetByIndex(uint8_t index) {
    if (index >= feb_tps_ctx.device_count) {
        return NULL;
    }

    if (!feb_tps_ctx.devices[index].initialized) {
        return NULL;
    }

    return &feb_tps_ctx.devices[index];
}

uint8_t FEB_TPS_DeviceGetCount(void) {
    return feb_tps_ctx.device_count;
}

/* ============================================================================
 * Measurement API
 * ============================================================================ */

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

FEB_TPS_Status_t FEB_TPS_PollScaled(FEB_TPS_Handle_t handle,
                                     FEB_TPS_MeasurementScaled_t *scaled) {
    FEB_TPS_Measurement_t meas;
    FEB_TPS_Status_t status = FEB_TPS_Poll(handle, &meas);

    if (status == FEB_TPS_OK && scaled != NULL) {
        scaled->bus_voltage_mv = (uint16_t)(meas.bus_voltage_v * 1000.0f);
        scaled->current_ma = (int16_t)(meas.current_a * 1000.0f);
        scaled->shunt_voltage_uv = (int32_t)(meas.shunt_voltage_mv * 1000.0f);
        scaled->power_mw = (uint16_t)(meas.power_w * 1000.0f);
    }

    return status;
}

FEB_TPS_Status_t FEB_TPS_PollBusVoltage(FEB_TPS_Handle_t handle, float *voltage_v) {
    if (!feb_tps_ctx.initialized || handle == NULL || voltage_v == NULL) {
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

FEB_TPS_Status_t FEB_TPS_PollCurrent(FEB_TPS_Handle_t handle, float *current_a) {
    if (!feb_tps_ctx.initialized || handle == NULL || current_a == NULL) {
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

FEB_TPS_Status_t FEB_TPS_PollRaw(FEB_TPS_Handle_t handle,
                                  uint16_t *bus_v_raw,
                                  int16_t *current_raw,
                                  int16_t *shunt_v_raw) {
    if (!feb_tps_ctx.initialized || handle == NULL) {
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

/* ============================================================================
 * Batch Operations
 * ============================================================================ */

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

uint8_t FEB_TPS_PollAllRaw(uint16_t *bus_v_raw, uint16_t *current_raw,
                            uint16_t *shunt_v_raw, uint8_t count) {
    if (!feb_tps_ctx.initialized) {
        return 0;
    }

    uint8_t actual_count = (count < feb_tps_ctx.device_count) ?
                           count : feb_tps_ctx.device_count;
    uint8_t success_count = 0;

    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    for (uint8_t i = 0; i < actual_count; i++) {
        FEB_TPS_Device_t *dev = &feb_tps_ctx.devices[i];
        if (!dev->initialized) {
            continue;
        }

        HAL_StatusTypeDef hal_status;
        uint16_t raw;
        bool success = true;

        if (bus_v_raw != NULL) {
            hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                           FEB_TPS_REG_BUS_VOLT, &raw);
            if (hal_status == HAL_OK) {
                bus_v_raw[i] = raw;
            } else {
                success = false;
            }
        }

        if (current_raw != NULL && success) {
            hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                           FEB_TPS_REG_CURRENT, &raw);
            if (hal_status == HAL_OK) {
                current_raw[i] = raw;
            } else {
                success = false;
            }
        }

        if (shunt_v_raw != NULL && success) {
            hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                           FEB_TPS_REG_SHUNT_VOLT, &raw);
            if (hal_status == HAL_OK) {
                shunt_v_raw[i] = raw;
            } else {
                success = false;
            }
        }

        if (success) {
            success_count++;
        }
    }

    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);

    return success_count;
}

/* ============================================================================
 * GPIO Control
 * ============================================================================ */

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

    return FEB_TPS_OK;
}

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

uint8_t FEB_TPS_EnableAll(bool enable) {
    uint8_t count = 0;

    for (uint8_t i = 0; i < feb_tps_ctx.device_count; i++) {
        FEB_TPS_Device_t *dev = &feb_tps_ctx.devices[i];
        if (dev->initialized && dev->en_gpio_port != NULL) {
            HAL_GPIO_WritePin((GPIO_TypeDef *)dev->en_gpio_port, dev->en_gpio_pin,
                              enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
            count++;
        }
    }

    return count;
}

uint8_t FEB_TPS_ReadAllPowerGood(bool *pg_states, uint8_t count) {
    if (pg_states == NULL) {
        return 0;
    }

    uint8_t actual_count = (count < feb_tps_ctx.device_count) ?
                           count : feb_tps_ctx.device_count;
    uint8_t success_count = 0;

    for (uint8_t i = 0; i < actual_count; i++) {
        FEB_TPS_Device_t *dev = &feb_tps_ctx.devices[i];
        if (dev->initialized && dev->pg_gpio_port != NULL) {
            GPIO_PinState state = HAL_GPIO_ReadPin((GPIO_TypeDef *)dev->pg_gpio_port,
                                                    dev->pg_gpio_pin);
            pg_states[i] = (state == GPIO_PIN_SET);
            success_count++;
        } else {
            pg_states[i] = false;
        }
    }

    return success_count;
}

/* ============================================================================
 * Configuration/Calibration
 * ============================================================================ */

float FEB_TPS_GetCurrentLSB(FEB_TPS_Handle_t handle) {
    if (handle == NULL) {
        return 0.0f;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    return dev->current_lsb;
}

uint16_t FEB_TPS_GetCalibration(FEB_TPS_Handle_t handle) {
    if (handle == NULL) {
        return 0;
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    return dev->cal_reg;
}

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

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

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

const char *FEB_TPS_GetDeviceName(FEB_TPS_Handle_t handle) {
    if (handle == NULL) {
        return "Unknown";
    }

    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    return (dev->name != NULL) ? dev->name : "Unknown";
}
