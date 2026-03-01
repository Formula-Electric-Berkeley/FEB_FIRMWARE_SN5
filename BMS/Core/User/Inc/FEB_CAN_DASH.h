/**
 * @file FEB_CAN_DASH.h
 * @brief DASH CAN message reception for BMS (Ready-to-Drive signal)
 *
 * FreeRTOS Safety:
 * - All shared data uses volatile for ISR/task visibility
 * - Reads are atomic (single word access)
 * - Timestamp checked before using R2D value
 */

#ifndef FEB_CAN_DASH_H
#define FEB_CAN_DASH_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief DASH IO signal storage
 * @note Updated from CAN RX callback (ISR context)
 * @note Read from main task context
 */
typedef struct
{
  volatile bool ready_to_drive;   // b1_ready_to_drive signal
  volatile bool data_logging;     // b2_data_logging signal
  volatile bool coolant_pump;     // s1_coolant_pump signal
  volatile bool radiator_fan;     // s2_radiator_fan signal
  volatile bool accumulator_fan;  // s3_accumulator_fan signal
  volatile uint32_t last_rx_tick; // HAL_GetTick() when last message received
} FEB_DASH_IO_t;

/* Global instance - defined in FEB_CAN_DASH.c */
extern FEB_DASH_IO_t DASH_IO;

/**
 * @brief Initialize DASH CAN message reception
 * @note Call after FEB_CAN_Init() in StartBMSTaskRx
 */
void FEB_CAN_DASH_Init(void);

/**
 * @brief Check if R2D signal is active and message is fresh
 * @param timeout_ms Maximum age of message in milliseconds
 * @return true if R2D is active AND message received within timeout
 *
 * @note FreeRTOS-safe: reads volatile variables atomically
 */
bool FEB_CAN_DASH_IsReadyToDrive(uint32_t timeout_ms);

/**
 * @brief Get raw R2D state (without timeout check)
 * @return Current R2D signal value (may be stale)
 *
 * @note Prefer FEB_CAN_DASH_IsReadyToDrive() for safety-critical decisions
 */
bool FEB_CAN_DASH_GetR2DRaw(void);

/**
 * @brief Get timestamp of last DASH IO message
 * @return HAL_GetTick() value when last message was received, 0 if never
 */
uint32_t FEB_CAN_DASH_GetLastRxTick(void);

#endif /* FEB_CAN_DASH_H */
