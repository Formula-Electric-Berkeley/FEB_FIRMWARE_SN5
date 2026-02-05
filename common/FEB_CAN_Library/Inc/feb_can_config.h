/**
 ******************************************************************************
 * @file           : feb_can_config.h
 * @brief          : Configuration defaults for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_CONFIG_H
#define FEB_CAN_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* ============================================================================
   * Capacity Defaults
   * ============================================================================ */

#ifndef FEB_CAN_MAX_RX_HANDLES
#define FEB_CAN_MAX_RX_HANDLES 32
#endif

#ifndef FEB_CAN_MAX_TX_HANDLES
#define FEB_CAN_MAX_TX_HANDLES 16
#endif

#ifndef FEB_CAN_MAX_FILTERS_PER_INSTANCE
#define FEB_CAN_MAX_FILTERS_PER_INSTANCE 14
#endif

  /* ============================================================================
   * Queue Sizes (FreeRTOS only)
   * ============================================================================ */

#ifndef FEB_CAN_TX_QUEUE_SIZE
#define FEB_CAN_TX_QUEUE_SIZE 16
#endif

#ifndef FEB_CAN_RX_QUEUE_SIZE
#define FEB_CAN_RX_QUEUE_SIZE 32
#endif

  /* ============================================================================
   * Timeout Defaults
   * ============================================================================ */

#ifndef FEB_CAN_TX_TIMEOUT_MS
#define FEB_CAN_TX_TIMEOUT_MS 100
#endif

#ifndef FEB_CAN_TX_QUEUE_TIMEOUT_MS
#define FEB_CAN_TX_QUEUE_TIMEOUT_MS 10
#endif

  /* ============================================================================
   * FreeRTOS Detection
   * ============================================================================
   *
   * Auto-detect FreeRTOS by checking for common FreeRTOS config macros.
   * Can be overridden by defining FEB_CAN_USE_FREERTOS before including.
   */

#ifndef FEB_CAN_USE_FREERTOS
#if defined(INCLUDE_xSemaphoreGetMutexHolder) || defined(configUSE_MUTEXES) || defined(USE_FREERTOS)
#define FEB_CAN_USE_FREERTOS 1
#else
#define FEB_CAN_USE_FREERTOS 0
#endif
#endif

  /* ============================================================================
   * Periodic TX Configuration
   * ============================================================================ */

#ifndef FEB_CAN_ENABLE_PERIODIC_TX
#define FEB_CAN_ENABLE_PERIODIC_TX 1
#endif

#ifndef FEB_CAN_MAX_PERIODIC_TX_SLOTS
#define FEB_CAN_MAX_PERIODIC_TX_SLOTS 8
#endif

  /* ============================================================================
   * Number of CAN Instances
   * ============================================================================ */

#ifndef FEB_CAN_NUM_INSTANCES
#define FEB_CAN_NUM_INSTANCES 2
#endif

  /* ============================================================================
   * Filter Bank Configuration
   * ============================================================================
   *
   * STM32F4 CAN filter banks:
   *   - Banks 0-13 assigned to CAN1 by default
   *   - Banks 14-27 assigned to CAN2 by default
   *   - Slave start bank configurable via CAN_SlaveStartFilterBank
   */

#ifndef FEB_CAN_CAN1_FILTER_BANK_START
#define FEB_CAN_CAN1_FILTER_BANK_START 0
#endif

#ifndef FEB_CAN_CAN2_FILTER_BANK_START
#define FEB_CAN_CAN2_FILTER_BANK_START 14
#endif

#ifndef FEB_CAN_TOTAL_FILTER_BANKS
#define FEB_CAN_TOTAL_FILTER_BANKS 28
#endif

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_CONFIG_H */
