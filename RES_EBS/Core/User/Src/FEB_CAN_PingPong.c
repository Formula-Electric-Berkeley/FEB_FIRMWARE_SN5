/**
 ******************************************************************************
 * @file           : FEB_CAN_PingPong.c
 * @brief          : CAN Ping/Pong Test Module Implementation
 ******************************************************************************
 */

#include "FEB_CAN_PingPong.h"

#include "feb_can_lib.h"
#include "feb_uart_log.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <string.h>

#define TAG_PING "[PING]"

typedef struct
{
  FEB_PingPong_Mode_t mode;
  int32_t tx_counter;
  int32_t last_rx_counter;
  uint32_t tx_count;
  uint32_t rx_count;
  int32_t rx_handle;
  bool pending_rx_log;
} PingPong_Channel_t;

static PingPong_Channel_t channels[FEB_PINGPONG_NUM_CHANNELS];

static const uint32_t frame_ids[FEB_PINGPONG_NUM_CHANNELS] = {
    FEB_PINGPONG_FRAME_ID_1,
    FEB_PINGPONG_FRAME_ID_2,
    FEB_PINGPONG_FRAME_ID_3,
    FEB_PINGPONG_FRAME_ID_4,
};

static void pingpong_rx_callback(uint8_t channel_idx, const uint8_t *data, uint8_t length)
{
  PingPong_Channel_t *ch;
  FEB_CAN_Status_t status;
  int32_t counter = 0;
  int32_t response;
  uint8_t tx_data[8] = {0};

  if (channel_idx >= FEB_PINGPONG_NUM_CHANNELS)
  {
    return;
  }

  ch = &channels[channel_idx];

  if (length >= 4U)
  {
    counter = (int32_t)((uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
                        ((uint32_t)data[3] << 24));
  }

  ch->last_rx_counter = counter;
  ch->rx_count++;
  ch->pending_rx_log = true;

  if (ch->mode != PINGPONG_MODE_PONG)
  {
    return;
  }

  response = counter + 1;
  tx_data[0] = (uint8_t)(response & 0xFF);
  tx_data[1] = (uint8_t)((response >> 8) & 0xFF);
  tx_data[2] = (uint8_t)((response >> 16) & 0xFF);
  tx_data[3] = (uint8_t)((response >> 24) & 0xFF);

  status =
      FEB_CAN_TX_SendFromISR(FEB_CAN_INSTANCE_1, frame_ids[channel_idx], FEB_CAN_ID_STD, tx_data, sizeof(tx_data));
  if (status == FEB_CAN_OK)
  {
    ch->tx_count++;
  }
  else
  {
    LOG_W(TAG_PING, "PONG TX ch%d failed: %s", channel_idx + 1, FEB_CAN_StatusToString(status));
  }
}

static void rx_callback_ch1(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                            const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;
  pingpong_rx_callback(0U, data, length);
}

static void rx_callback_ch2(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                            const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;
  pingpong_rx_callback(1U, data, length);
}

static void rx_callback_ch3(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                            const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;
  pingpong_rx_callback(2U, data, length);
}

static void rx_callback_ch4(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                            const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;
  pingpong_rx_callback(3U, data, length);
}

static FEB_CAN_RX_Callback_t rx_callbacks[FEB_PINGPONG_NUM_CHANNELS] = {
    rx_callback_ch1,
    rx_callback_ch2,
    rx_callback_ch3,
    rx_callback_ch4,
};

void FEB_CAN_PingPong_Init(void)
{
  uint8_t i;

  memset(channels, 0, sizeof(channels));

  for (i = 0U; i < FEB_PINGPONG_NUM_CHANNELS; i++)
  {
    channels[i].rx_handle = -1;
  }
}

void FEB_CAN_PingPong_SetMode(uint8_t channel, FEB_PingPong_Mode_t mode)
{
  uint8_t idx;
  PingPong_Channel_t *ch;

  if (channel < 1U || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return;
  }

  idx = (uint8_t)(channel - 1U);
  ch = &channels[idx];

  if (ch->mode == mode)
  {
    return;
  }

  if (ch->rx_handle >= 0)
  {
    FEB_CAN_RX_Unregister(ch->rx_handle);
    ch->rx_handle = -1;
  }

  if (mode == PINGPONG_MODE_PING || mode == PINGPONG_MODE_PONG)
  {
    FEB_CAN_RX_Params_t rx_params = {
        .instance = FEB_CAN_INSTANCE_1,
        .can_id = frame_ids[idx],
        .id_type = FEB_CAN_ID_STD,
        .filter_type = FEB_CAN_FILTER_EXACT,
        .mask = 0x7FFU,
        .fifo = FEB_CAN_FIFO_0,
        .callback = rx_callbacks[idx],
        .user_data = NULL,
    };
    ch->rx_handle = FEB_CAN_RX_Register(&rx_params);
    if (ch->rx_handle < 0)
    {
      LOG_W(TAG_PING, "RX register ch%d failed: %ld", channel, (long)ch->rx_handle);
    }
  }

  ch->tx_counter = 0;
  ch->tx_count = 0U;
  ch->rx_count = 0U;
  ch->last_rx_counter = 0;
  ch->pending_rx_log = false;

  if ((mode == PINGPONG_MODE_PING || mode == PINGPONG_MODE_PONG) && ch->rx_handle < 0)
  {
    ch->mode = PINGPONG_MODE_OFF;
    return;
  }

  ch->mode = mode;
}

FEB_PingPong_Mode_t FEB_CAN_PingPong_GetMode(uint8_t channel)
{
  if (channel < 1U || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return PINGPONG_MODE_OFF;
  }

  return channels[channel - 1U].mode;
}

void FEB_CAN_PingPong_Tick(void)
{
  uint8_t i;

  for (i = 0U; i < FEB_PINGPONG_NUM_CHANNELS; i++)
  {
    PingPong_Channel_t *ch = &channels[i];

    if (ch->pending_rx_log)
    {
      LOG_D(TAG_PING, "RX ch%d cnt:%ld total:%lu", i + 1, (long)ch->last_rx_counter, (unsigned long)ch->rx_count);
      ch->pending_rx_log = false;
    }

    if (ch->mode == PINGPONG_MODE_PING)
    {
      FEB_CAN_Status_t status;
      uint32_t retry_start;
      uint8_t tx_data[8] = {0};

      tx_data[0] = (uint8_t)(ch->tx_counter & 0xFF);
      tx_data[1] = (uint8_t)((ch->tx_counter >> 8) & 0xFF);
      tx_data[2] = (uint8_t)((ch->tx_counter >> 16) & 0xFF);
      tx_data[3] = (uint8_t)((ch->tx_counter >> 24) & 0xFF);

      retry_start = HAL_GetTick();
      do
      {
        status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_ids[i], FEB_CAN_ID_STD, tx_data, sizeof(tx_data));
        if (status == FEB_CAN_OK)
        {
          ch->tx_count++;
          LOG_D(TAG_PING, "TX ch%d ID:0x%02lX cnt:%ld", i + 1, (unsigned long)frame_ids[i], (long)ch->tx_counter);
          ch->tx_counter++;
          break;
        }
      } while ((HAL_GetTick() - retry_start) < 5U);

      if (status != FEB_CAN_OK)
      {
        LOG_W(TAG_PING, "PING TX ch%d failed: %s", i + 1, FEB_CAN_StatusToString(status));
      }
    }
  }
}

uint32_t FEB_CAN_PingPong_GetTxCount(uint8_t channel)
{
  if (channel < 1U || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return 0U;
  }

  return channels[channel - 1U].tx_count;
}

uint32_t FEB_CAN_PingPong_GetRxCount(uint8_t channel)
{
  if (channel < 1U || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return 0U;
  }

  return channels[channel - 1U].rx_count;
}

int32_t FEB_CAN_PingPong_GetLastCounter(uint8_t channel)
{
  if (channel < 1U || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return 0;
  }

  return channels[channel - 1U].last_rx_counter;
}

void FEB_CAN_PingPong_Reset(void)
{
  uint8_t i;

  for (i = 0U; i < FEB_PINGPONG_NUM_CHANNELS; i++)
  {
    FEB_CAN_PingPong_SetMode((uint8_t)(i + 1U), PINGPONG_MODE_OFF);
  }
}
