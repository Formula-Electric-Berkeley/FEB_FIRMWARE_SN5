/**
 ******************************************************************************
 * @file           : FEB_CAN_PingPong.c
 * @brief          : CAN Ping/Pong Test Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_PingPong.h"
#include "feb_can_lib.h"
#include <string.h>

/* ============================================================================
 * Internal State
 * ============================================================================ */

typedef struct
{
  FEB_PingPong_Mode_t mode;
  int32_t tx_counter;
  int32_t last_rx_counter;
  uint32_t tx_count;
  uint32_t rx_count;
  int32_t rx_handle;
} PingPong_Channel_t;

static PingPong_Channel_t channels[FEB_PINGPONG_NUM_CHANNELS];

static const uint32_t frame_ids[FEB_PINGPONG_NUM_CHANNELS] = {
    FEB_PINGPONG_FRAME_ID_1,
    FEB_PINGPONG_FRAME_ID_2,
    FEB_PINGPONG_FRAME_ID_3,
    FEB_PINGPONG_FRAME_ID_4,
};

/* ============================================================================
 * RX Callback Handlers
 * ============================================================================ */

static void pingpong_rx_callback(uint8_t channel_idx, const uint8_t *data, uint8_t length)
{
  if (channel_idx >= FEB_PINGPONG_NUM_CHANNELS)
  {
    return;
  }

  PingPong_Channel_t *ch = &channels[channel_idx];

  /* Unpack the counter value (int32_t, little-endian) */
  int32_t counter = 0;
  if (length >= 4)
  {
    counter =
        (int32_t)((uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24));
  }

  ch->last_rx_counter = counter;
  ch->rx_count++;

  /* If in pong mode, send response with counter+1 */
  if (ch->mode == PINGPONG_MODE_PONG)
  {
    int32_t response = counter + 1;
    uint8_t tx_data[8] = {0};

    /* Pack response (int32_t, little-endian) */
    tx_data[0] = (uint8_t)(response & 0xFF);
    tx_data[1] = (uint8_t)((response >> 8) & 0xFF);
    tx_data[2] = (uint8_t)((response >> 16) & 0xFF);
    tx_data[3] = (uint8_t)((response >> 24) & 0xFF);

    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_ids[channel_idx], FEB_CAN_ID_STD, tx_data, 8);
    ch->tx_count++;
  }
}

static void rx_callback_ch1(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                            const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;
  pingpong_rx_callback(0, data, length);
}

static void rx_callback_ch2(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                            const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;
  pingpong_rx_callback(1, data, length);
}

static void rx_callback_ch3(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                            const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;
  pingpong_rx_callback(2, data, length);
}

static void rx_callback_ch4(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                            const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;
  pingpong_rx_callback(3, data, length);
}

static FEB_CAN_RX_Callback_t rx_callbacks[FEB_PINGPONG_NUM_CHANNELS] = {
    rx_callback_ch1,
    rx_callback_ch2,
    rx_callback_ch3,
    rx_callback_ch4,
};

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_PingPong_Init(void)
{
  memset(channels, 0, sizeof(channels));

  /* Initialize RX handles to invalid */
  for (uint8_t i = 0; i < FEB_PINGPONG_NUM_CHANNELS; i++)
  {
    channels[i].rx_handle = -1;
  }
}

void FEB_CAN_PingPong_SetMode(uint8_t channel, FEB_PingPong_Mode_t mode)
{
  if (channel < 1 || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return;
  }

  uint8_t idx = channel - 1;
  PingPong_Channel_t *ch = &channels[idx];

  /* If mode is changing, update RX registration */
  if (ch->mode != mode)
  {
    /* Unregister old handler if any */
    if (ch->rx_handle >= 0)
    {
      FEB_CAN_RX_Unregister(ch->rx_handle);
      ch->rx_handle = -1;
    }

    /* Register RX handler for pong mode (to receive pings) */
    /* Also register for ping mode (to receive pong responses) */
    if (mode == PINGPONG_MODE_PONG || mode == PINGPONG_MODE_PING)
    {
      FEB_CAN_RX_Params_t rx_params = {
          .instance = FEB_CAN_INSTANCE_1,
          .can_id = frame_ids[idx],
          .id_type = FEB_CAN_ID_STD,
          .filter_type = FEB_CAN_FILTER_EXACT,
          .mask = 0x7FF,
          .fifo = FEB_CAN_FIFO_0,
          .callback = rx_callbacks[idx],
      };
      ch->rx_handle = FEB_CAN_RX_Register(&rx_params);
    }

    /* Reset counters when mode changes */
    ch->tx_counter = 0;
    ch->tx_count = 0;
    ch->rx_count = 0;
    ch->last_rx_counter = 0;
    ch->mode = mode;
  }
}

FEB_PingPong_Mode_t FEB_CAN_PingPong_GetMode(uint8_t channel)
{
  if (channel < 1 || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return PINGPONG_MODE_OFF;
  }
  return channels[channel - 1].mode;
}

void FEB_CAN_PingPong_Tick(void)
{
  for (uint8_t i = 0; i < FEB_PINGPONG_NUM_CHANNELS; i++)
  {
    PingPong_Channel_t *ch = &channels[i];

    if (ch->mode == PINGPONG_MODE_PING)
    {
      /* Pack and send the counter */
      uint8_t tx_data[8] = {0};

      tx_data[0] = (uint8_t)(ch->tx_counter & 0xFF);
      tx_data[1] = (uint8_t)((ch->tx_counter >> 8) & 0xFF);
      tx_data[2] = (uint8_t)((ch->tx_counter >> 16) & 0xFF);
      tx_data[3] = (uint8_t)((ch->tx_counter >> 24) & 0xFF);

      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_ids[i], FEB_CAN_ID_STD, tx_data, 8);

      ch->tx_counter++;
      ch->tx_count++;
    }
  }
}

uint32_t FEB_CAN_PingPong_GetTxCount(uint8_t channel)
{
  if (channel < 1 || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return 0;
  }
  return channels[channel - 1].tx_count;
}

uint32_t FEB_CAN_PingPong_GetRxCount(uint8_t channel)
{
  if (channel < 1 || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return 0;
  }
  return channels[channel - 1].rx_count;
}

int32_t FEB_CAN_PingPong_GetLastCounter(uint8_t channel)
{
  if (channel < 1 || channel > FEB_PINGPONG_NUM_CHANNELS)
  {
    return 0;
  }
  return channels[channel - 1].last_rx_counter;
}

void FEB_CAN_PingPong_Reset(void)
{
  for (uint8_t i = 0; i < FEB_PINGPONG_NUM_CHANNELS; i++)
  {
    FEB_CAN_PingPong_SetMode(i + 1, PINGPONG_MODE_OFF);
  }
}
