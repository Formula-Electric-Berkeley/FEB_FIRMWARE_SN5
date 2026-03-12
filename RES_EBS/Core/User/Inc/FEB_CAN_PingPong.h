/**
 ******************************************************************************
 * @file           : FEB_CAN_PingPong.h
 * @brief          : CAN Ping/Pong Test Module Header
 ******************************************************************************
 */

#ifndef FEB_CAN_PINGPONG_H
#define FEB_CAN_PINGPONG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FEB_PINGPONG_NUM_CHANNELS 4U

#define FEB_PINGPONG_FRAME_ID_1 0xE0U
#define FEB_PINGPONG_FRAME_ID_2 0xE1U
#define FEB_PINGPONG_FRAME_ID_3 0xE2U
#define FEB_PINGPONG_FRAME_ID_4 0xE3U

typedef enum
{
  PINGPONG_MODE_OFF = 0,
  PINGPONG_MODE_PING,
  PINGPONG_MODE_PONG,
} FEB_PingPong_Mode_t;

void FEB_CAN_PingPong_Init(void);
void FEB_CAN_PingPong_SetMode(uint8_t channel, FEB_PingPong_Mode_t mode);
FEB_PingPong_Mode_t FEB_CAN_PingPong_GetMode(uint8_t channel);
void FEB_CAN_PingPong_Tick(void);
uint32_t FEB_CAN_PingPong_GetTxCount(uint8_t channel);
uint32_t FEB_CAN_PingPong_GetRxCount(uint8_t channel);
int32_t FEB_CAN_PingPong_GetLastCounter(uint8_t channel);
void FEB_CAN_PingPong_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CAN_PINGPONG_H */
