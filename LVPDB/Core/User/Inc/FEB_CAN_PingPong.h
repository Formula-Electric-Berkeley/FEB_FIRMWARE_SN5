/**
 ******************************************************************************
 * @file           : FEB_CAN_PingPong.h
 * @brief          : CAN Ping/Pong Test Module Header
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_PINGPONG_H
#define FEB_CAN_PINGPONG_H

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * Channel Definitions
 * ============================================================================ */

#define FEB_PINGPONG_NUM_CHANNELS 4

/* Frame IDs from SN4 CAN library */
#define FEB_PINGPONG_FRAME_ID_1 0xE0
#define FEB_PINGPONG_FRAME_ID_2 0xE1
#define FEB_PINGPONG_FRAME_ID_3 0xE2
#define FEB_PINGPONG_FRAME_ID_4 0xE3

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

typedef enum
{
  PINGPONG_MODE_OFF = 0, /* Channel disabled */
  PINGPONG_MODE_PING,    /* TX on ID, increment counter each transmission */
  PINGPONG_MODE_PONG,    /* Listen on ID, respond with counter+1 */
} FEB_PingPong_Mode_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize the ping/pong module
 * @note Must be called after FEB_CAN_Init()
 */
void FEB_CAN_PingPong_Init(void);

/**
 * @brief Set the mode for a channel
 * @param channel Channel number (1-4)
 * @param mode PINGPONG_MODE_OFF, PINGPONG_MODE_PING, or PINGPONG_MODE_PONG
 */
void FEB_CAN_PingPong_SetMode(uint8_t channel, FEB_PingPong_Mode_t mode);

/**
 * @brief Get the current mode of a channel
 * @param channel Channel number (1-4)
 * @return Current mode
 */
FEB_PingPong_Mode_t FEB_CAN_PingPong_GetMode(uint8_t channel);

/**
 * @brief Process ping transmissions (call from timer, e.g., every 100ms)
 */
void FEB_CAN_PingPong_Tick(void);

/**
 * @brief Get TX count for a channel
 * @param channel Channel number (1-4)
 * @return Number of messages transmitted
 */
uint32_t FEB_CAN_PingPong_GetTxCount(uint8_t channel);

/**
 * @brief Get RX count for a channel
 * @param channel Channel number (1-4)
 * @return Number of messages received
 */
uint32_t FEB_CAN_PingPong_GetRxCount(uint8_t channel);

/**
 * @brief Get last received counter value
 * @param channel Channel number (1-4)
 * @return Last counter value received, or 0 if none
 */
int32_t FEB_CAN_PingPong_GetLastCounter(uint8_t channel);

/**
 * @brief Reset all counters and turn off all channels
 */
void FEB_CAN_PingPong_Reset(void);

#endif /* FEB_CAN_PINGPONG_H */
