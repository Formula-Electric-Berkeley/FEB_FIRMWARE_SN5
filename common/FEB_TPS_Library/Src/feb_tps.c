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
#include "main.h"
#endif

#include "feb_tps.h"
#include "feb_tps_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define TPS_LOG_BUFFER_SIZE 128

/* ============================================================================
 * Global context
 * ============================================================================ */

static FEB_TPS_Context_t feb_tps_ctx = {0};

/* ============================================================================
 * Internal logging
 * ============================================================================ */

void FEB_TPS_Log(uint8_t level, const char *fmt, ...) {
    if (feb_tps_ctx.log_func == NULL) {
        return;
    }
    if (level > feb_tps_ctx.log_level) {
        return;
    }

    char buffer[TPS_LOG_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    feb_tps_ctx.log_func((FEB_TPS_LogLevel_t)level, buffer);
}

/* ============================================================================
 * I2C helpers — single-shot HAL calls, no retry, no inter-op delay
 * ============================================================================ */

static HAL_StatusTypeDef feb_tps_read_reg(I2C_HandleTypeDef *hi2c,
                                          uint8_t i2c_addr,
                                          uint8_t reg,
                                          uint16_t *value) {
    uint8_t buf[2];
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(hi2c, (uint16_t)(i2c_addr << 1),
                                                reg, I2C_MEMADD_SIZE_8BIT,
                                                buf, 2, FEB_TPS_I2C_TIMEOUT_MS);
    if (status == HAL_OK) {
        /* TPS2482 returns MSB first */
        *value = ((uint16_t)buf[0] << 8) | buf[1];
    }
    return status;
}

static HAL_StatusTypeDef feb_tps_write_reg(I2C_HandleTypeDef *hi2c,
                                           uint8_t i2c_addr,
                                           uint8_t reg,
                                           uint16_t value) {
    uint8_t buf[2] = { (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
    return HAL_I2C_Mem_Write(hi2c, (uint16_t)(i2c_addr << 1),
                             reg, I2C_MEMADD_SIZE_8BIT,
                             buf, 2, FEB_TPS_I2C_TIMEOUT_MS);
}

/* ============================================================================
 * Calibration
 * ============================================================================ */

static void feb_tps_compute_calibration(FEB_TPS_Device_t *dev) {
    dev->current_lsb = FEB_TPS_CALC_CURRENT_LSB(dev->i_max_amps);
    dev->power_lsb = FEB_TPS_CALC_POWER_LSB(dev->current_lsb);
    dev->cal_reg = FEB_TPS_CALC_CAL(dev->current_lsb, dev->r_shunt_ohms);
}

/* ============================================================================
 * Library lifecycle
 * ============================================================================ */

FEB_TPS_Status_t FEB_TPS_Init(const FEB_TPS_LibConfig_t *config) {
    if (feb_tps_ctx.initialized) {
        return FEB_TPS_OK;
    }

    memset(&feb_tps_ctx, 0, sizeof(feb_tps_ctx));

    if (config != NULL) {
        feb_tps_ctx.log_func = config->log_func;
        feb_tps_ctx.log_level = (config->log_level != 0) ? config->log_level
                                                          : FEB_TPS_LOG_INFO;
    } else {
        feb_tps_ctx.log_level = FEB_TPS_LOG_INFO;
    }

#if FEB_TPS_USE_FREERTOS
    if (config == NULL || config->i2c_mutex == NULL) {
        TPS_LOG_E("i2c_mutex required in FreeRTOS build");
        return FEB_TPS_ERR_INVALID_ARG;
    }
    feb_tps_ctx.i2c_mutex = config->i2c_mutex;
#endif

    feb_tps_ctx.initialized = true;
    TPS_LOG_I("TPS library initialized");
    return FEB_TPS_OK;
}

bool FEB_TPS_IsInitialized(void) {
    return feb_tps_ctx.initialized;
}

/* ============================================================================
 * Device registration — write CONFIG + CAL, read both back, compare
 * ============================================================================ */

FEB_TPS_Status_t FEB_TPS_DeviceRegister(const FEB_TPS_DeviceConfig_t *config,
                                        FEB_TPS_Handle_t *handle) {
    if (!feb_tps_ctx.initialized) {
        return FEB_TPS_ERR_NOT_INIT;
    }
    if (config == NULL || handle == NULL || config->hi2c == NULL) {
        return FEB_TPS_ERR_INVALID_ARG;
    }
    if (config->r_shunt_ohms <= 0.0f || config->i_max_amps <= 0.0f) {
        return FEB_TPS_ERR_INVALID_ARG;
    }
    if (feb_tps_ctx.device_count >= FEB_TPS_MAX_DEVICES) {
        return FEB_TPS_ERR_MAX_DEVICES;
    }

    FEB_TPS_Device_t *dev = &feb_tps_ctx.devices[feb_tps_ctx.device_count];
    memset(dev, 0, sizeof(*dev));

    dev->hi2c = config->hi2c;
    dev->i2c_addr = config->i2c_addr;
    dev->r_shunt_ohms = config->r_shunt_ohms;
    dev->i_max_amps = config->i_max_amps;
    dev->name = config->name;
    dev->en_gpio_port = config->en_gpio_port;
    dev->en_gpio_pin = config->en_gpio_pin;
    dev->pg_gpio_port = config->pg_gpio_port;
    dev->pg_gpio_pin = config->pg_gpio_pin;

    feb_tps_compute_calibration(dev);

    const uint16_t config_val = (config->config_reg != 0) ? config->config_reg
                                                          : FEB_TPS_CONFIG_REG_DEFAULT;

    FEB_TPS_MUTEX_LOCK(feb_tps_ctx.i2c_mutex);

    HAL_StatusTypeDef hal_status;
    FEB_TPS_Status_t status = FEB_TPS_OK;

    hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                   FEB_TPS_REG_CONFIG, config_val);
    if (hal_status != HAL_OK) {
        TPS_LOG_E("CONFIG write failed for %s @ 0x%02X",
                  dev->name ? dev->name : "?", dev->i2c_addr);
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }

    hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                   FEB_TPS_REG_CAL, dev->cal_reg);
    if (hal_status != HAL_OK) {
        TPS_LOG_E("CAL write failed for %s @ 0x%02X",
                  dev->name ? dev->name : "?", dev->i2c_addr);
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }

    /* Optional MASK / ALERT_LIM (not readback-verified) */
    if (config->mask_reg != 0) {
        hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                       FEB_TPS_REG_MASK, config->mask_reg);
        if (hal_status != HAL_OK) {
            status = FEB_TPS_ERR_I2C;
            goto cleanup;
        }
    }
    if (config->alert_limit != 0) {
        hal_status = feb_tps_write_reg(dev->hi2c, dev->i2c_addr,
                                       FEB_TPS_REG_ALERT_LIM, config->alert_limit);
        if (hal_status != HAL_OK) {
            status = FEB_TPS_ERR_I2C;
            goto cleanup;
        }
    }

    /* Readback verification: write didn't ACK garbage; chip latched our values */
    uint16_t rb_config = 0;
    uint16_t rb_cal = 0;
    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                  FEB_TPS_REG_CONFIG, &rb_config);
    if (hal_status != HAL_OK) {
        TPS_LOG_E("CONFIG readback failed for %s", dev->name ? dev->name : "?");
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }
    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                  FEB_TPS_REG_CAL, &rb_cal);
    if (hal_status != HAL_OK) {
        TPS_LOG_E("CAL readback failed for %s", dev->name ? dev->name : "?");
        status = FEB_TPS_ERR_I2C;
        goto cleanup;
    }

    /* The CFG_RST bit auto-clears on read; ignore it in the comparison. */
    if ((rb_config & ~FEB_TPS_CFG_RST) != (config_val & ~FEB_TPS_CFG_RST)) {
        TPS_LOG_E("CONFIG mismatch %s: wrote 0x%04X read 0x%04X",
                  dev->name ? dev->name : "?", config_val, rb_config);
        status = FEB_TPS_ERR_CONFIG_MISMATCH;
        goto cleanup;
    }
    if (rb_cal != dev->cal_reg) {
        TPS_LOG_E("CAL mismatch %s: wrote 0x%04X read 0x%04X",
                  dev->name ? dev->name : "?", dev->cal_reg, rb_cal);
        status = FEB_TPS_ERR_CONFIG_MISMATCH;
        goto cleanup;
    }

    dev->initialized = true;
    feb_tps_ctx.device_count++;
    *handle = dev;

    TPS_LOG_I("Registered '%s' at 0x%02X (cal=0x%04X)",
              dev->name ? dev->name : "?", dev->i2c_addr, dev->cal_reg);

cleanup:
    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);
    return status;
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

    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                  FEB_TPS_REG_BUS_VOLT, &raw);
    if (hal_status != HAL_OK) { status = FEB_TPS_ERR_I2C; goto cleanup; }
    measurement->bus_voltage_raw = raw;
    measurement->bus_voltage_v = (float)raw * FEB_TPS_CONV_VBUS_V_PER_LSB;

    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                  FEB_TPS_REG_CURRENT, &raw);
    if (hal_status != HAL_OK) { status = FEB_TPS_ERR_I2C; goto cleanup; }
    measurement->current_raw = FEB_TPS_SignMagnitude(raw);
    measurement->current_a = (float)measurement->current_raw * dev->current_lsb;

    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                  FEB_TPS_REG_SHUNT_VOLT, &raw);
    if (hal_status != HAL_OK) { status = FEB_TPS_ERR_I2C; goto cleanup; }
    measurement->shunt_voltage_raw = FEB_TPS_SignMagnitude(raw);
    measurement->shunt_voltage_mv = (float)measurement->shunt_voltage_raw *
                                    FEB_TPS_CONV_VSHUNT_MV_PER_LSB;

    hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                  FEB_TPS_REG_POWER, &raw);
    if (hal_status != HAL_OK) { status = FEB_TPS_ERR_I2C; goto cleanup; }
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
        scaled->bus_voltage_mv = (meas.bus_voltage_v >= 0.0f)
                                 ? (uint32_t)(meas.bus_voltage_v * 1000.0f) : 0;
        scaled->current_ma = (int32_t)(meas.current_a * 1000.0f);
        scaled->shunt_voltage_uv = (int32_t)(meas.shunt_voltage_mv * 1000.0f);
        scaled->power_mw = (meas.power_w >= 0.0f)
                           ? (uint32_t)(meas.power_w * 1000.0f) : 0;
    }
    return status;
}

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
        if (hal_status != HAL_OK) { status = FEB_TPS_ERR_I2C; goto cleanup; }
        *bus_v_raw = raw;
    }
    if (current_raw != NULL) {
        hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                      FEB_TPS_REG_CURRENT, &raw);
        if (hal_status != HAL_OK) { status = FEB_TPS_ERR_I2C; goto cleanup; }
        *current_raw = FEB_TPS_SignMagnitude(raw);
    }
    if (shunt_v_raw != NULL) {
        hal_status = feb_tps_read_reg(dev->hi2c, dev->i2c_addr,
                                      FEB_TPS_REG_SHUNT_VOLT, &raw);
        if (hal_status != HAL_OK) { status = FEB_TPS_ERR_I2C; goto cleanup; }
        *shunt_v_raw = FEB_TPS_SignMagnitude(raw);
    }

cleanup:
    FEB_TPS_MUTEX_UNLOCK(feb_tps_ctx.i2c_mutex);
    return status;
}

/* ============================================================================
 * GPIO control
 * ============================================================================ */

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
    return FEB_TPS_OK;
}

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

    GPIO_PinState s = HAL_GPIO_ReadPin((GPIO_TypeDef *)dev->pg_gpio_port,
                                       dev->pg_gpio_pin);
    *pg_state = (s == GPIO_PIN_SET);
    return FEB_TPS_OK;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

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

float FEB_TPS_GetCurrentLSB(FEB_TPS_Handle_t handle) {
    if (!feb_tps_ctx.initialized || handle == NULL) {
        return 0.0f;
    }
    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    return dev->initialized ? dev->current_lsb : 0.0f;
}

const char *FEB_TPS_GetDeviceName(FEB_TPS_Handle_t handle) {
    if (handle == NULL) {
        return "Unknown";
    }
    FEB_TPS_Device_t *dev = (FEB_TPS_Device_t *)handle;
    return (dev->name != NULL) ? dev->name : "Unknown";
}

const char *FEB_TPS_StatusToString(FEB_TPS_Status_t status) {
    switch (status) {
        case FEB_TPS_OK:                   return "OK";
        case FEB_TPS_ERR_INVALID_ARG:      return "Invalid argument";
        case FEB_TPS_ERR_I2C:              return "I2C error";
        case FEB_TPS_ERR_NOT_INIT:         return "Not initialized";
        case FEB_TPS_ERR_CONFIG_MISMATCH:  return "Config mismatch";
        case FEB_TPS_ERR_MAX_DEVICES:      return "Max devices exceeded";
        default:                           return "Unknown";
    }
}
