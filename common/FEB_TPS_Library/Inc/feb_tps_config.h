/**
 ******************************************************************************
 * @file           : feb_tps_config.h
 * @brief          : Compile-time options for the FEB TPS Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_TPS_CONFIG_H
#define FEB_TPS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FreeRTOS auto-detection
 * ============================================================================ */

/**
 * Detected automatically from common FreeRTOS macros. User can pre-define
 * FEB_TPS_USE_FREERTOS to override.
 */
#ifndef FEB_TPS_USE_FREERTOS
#if ((defined(INCLUDE_xSemaphoreGetMutexHolder) && (INCLUDE_xSemaphoreGetMutexHolder != 0)) || \
     (defined(configUSE_MUTEXES) && (configUSE_MUTEXES != 0)) || \
     (defined(USE_FREERTOS) && (USE_FREERTOS != 0)))
#define FEB_TPS_USE_FREERTOS 1
#else
#define FEB_TPS_USE_FREERTOS 0
#endif
#endif

/* ============================================================================
 * Device limits
 * ============================================================================ */

/**
 * Max simultaneously registered devices. LVPDB uses 7; default 8 leaves
 * headroom. Slots are packed (no Unregister API) so this is also the
 * total array size.
 */
#ifndef FEB_TPS_MAX_DEVICES
#define FEB_TPS_MAX_DEVICES 8
#endif

/* ============================================================================
 * I2C timeout
 * ============================================================================ */

/**
 * Timeout for every HAL_I2C_Mem_Read/Write call. Single-shot — no retries.
 * Matches the working raw-HAL register-dump path on LVPDB.
 */
#ifndef FEB_TPS_I2C_TIMEOUT_MS
#define FEB_TPS_I2C_TIMEOUT_MS 100
#endif

/* ============================================================================
 * Default CONFIG register
 * ============================================================================ */

/**
 * Default CONFIG written when DeviceConfig.config_reg == 0.
 * 0x4127 = AVG=1, VBUS_CT=1.1ms, VSH_CT=1.1ms, MODE=shunt+bus continuous.
 */
#ifndef FEB_TPS_CONFIG_REG_DEFAULT
#define FEB_TPS_CONFIG_REG_DEFAULT 0x4127
#endif

#ifdef __cplusplus
}
#endif

#endif /* FEB_TPS_CONFIG_H */
