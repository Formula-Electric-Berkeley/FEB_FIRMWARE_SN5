/**
 ******************************************************************************
 * @file           : feb_tps_config.h
 * @brief          : Configuration defaults and FreeRTOS detection for TPS library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Configuration options for the FEB TPS Library. Users can override these
 * defaults by defining the macros before including feb_tps.h.
 *
 ******************************************************************************
 */

#ifndef FEB_TPS_CONFIG_H
#define FEB_TPS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FreeRTOS Auto-Detection
 * ============================================================================ */

/**
 * Automatically detect FreeRTOS presence based on common FreeRTOS macros.
 * This follows the same pattern as FEB_UART_Library.
 *
 * User can override by defining FEB_TPS_USE_FREERTOS before including this header.
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
 * Bare-Metal Safety Mode
 * ============================================================================
 *
 * When FreeRTOS is NOT detected and FORCE_BARE_METAL is NOT set:
 *   - Mutex operations are NO-OPs (safe default for single-threaded use)
 *
 * When FORCE_BARE_METAL is explicitly set to 1:
 *   - Mutex operations use __disable_irq() / __enable_irq() critical sections
 *   - Use this for bare-metal projects that need ISR protection
 */

#ifndef FEB_TPS_FORCE_BARE_METAL
#define FEB_TPS_FORCE_BARE_METAL 0
#endif

/* ============================================================================
 * Polling Configuration (FreeRTOS Mode)
 * ============================================================================ */

/**
 * @brief Default polling interval in milliseconds for auto-polling mode
 */
#ifndef FEB_TPS_DEFAULT_POLL_INTERVAL_MS
#define FEB_TPS_DEFAULT_POLL_INTERVAL_MS 100
#endif

/* ============================================================================
 * Device Limits
 * ============================================================================ */

/**
 * @brief Maximum number of TPS2482 devices that can be registered
 *
 * LVPDB uses 7 devices, so default is 8 to allow some margin.
 * Can be reduced to save memory if fewer devices are needed.
 */
#ifndef FEB_TPS_MAX_DEVICES
#define FEB_TPS_MAX_DEVICES 8
#endif

/* ============================================================================
 * I2C Configuration
 * ============================================================================ */

/**
 * @brief Default I2C timeout in milliseconds
 *
 * Using a finite timeout (500ms) allows I2C operations to fail gracefully
 * and enables FEB_TPS_BusRecovery to recover from stuck bus states.
 */
#ifndef FEB_TPS_DEFAULT_I2C_TIMEOUT_MS
#define FEB_TPS_DEFAULT_I2C_TIMEOUT_MS 500
#endif

/**
 * @brief Maximum number of I2C retries on communication failure
 */
#ifndef FEB_TPS_I2C_MAX_RETRIES
#define FEB_TPS_I2C_MAX_RETRIES 3
#endif

/* ============================================================================
 * Default TPS2482 Register Values
 * ============================================================================ */

/**
 * @brief Default CONFIG register value
 *
 * 0x4127 =
 *   - AVG: 1 sample
 *   - VBUS_CT: 1.1ms
 *   - VSH_CT: 1.1ms
 *   - MODE: Shunt and Bus, continuous
 */
#ifndef FEB_TPS_CONFIG_REG_DEFAULT
#define FEB_TPS_CONFIG_REG_DEFAULT 0x4127
#endif

/* ============================================================================
 * Debug/Logging Configuration
 * ============================================================================ */

/**
 * @brief Enable debug logging (requires FEB_UART_Library)
 */
#ifndef FEB_TPS_ENABLE_LOGGING
#define FEB_TPS_ENABLE_LOGGING 0
#endif

#ifdef __cplusplus
}
#endif

#endif /* FEB_TPS_CONFIG_H */
