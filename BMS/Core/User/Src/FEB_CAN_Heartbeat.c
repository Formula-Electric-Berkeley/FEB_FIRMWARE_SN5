/**
 * @file FEB_CAN_Heartbeat.c
 * @brief CAN-presence (heartbeat) tracking for the BMS
 * @author Formula Electric @ Berkeley
 *
 * Subscribes to the per-board heartbeat frames and records the last-seen tick
 * for each. Reads are lock-free (single-word volatile, atomic on Cortex-M4),
 * matching the FEB_CAN_DASH / FEB_CAN_IVT freshness pattern.
 */

#include "FEB_CAN_Heartbeat.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "stm32f4xx_hal.h"

static volatile uint32_t hb_last_rx_tick[FEB_HB_COUNT] = {0};

static int8_t frame_to_dev(uint32_t can_id)
{
  switch (can_id)
  {
  case FEB_CAN_PCU_HEARTBEAT_FRAME_ID:
    return FEB_HB_PCU;
  case FEB_CAN_DASH_HEARTBEAT_FRAME_ID:
    return FEB_HB_DASH;
  case FEB_CAN_LVPDB_HEARTBEAT_FRAME_ID:
    return FEB_HB_LVPDB;
  case FEB_CAN_DCU_HEARTBEAT_FRAME_ID:
    return FEB_HB_DCU;
  case FEB_CAN_FRONT_SENSOR_HEARTBEAT_MESSAGE_FRAME_ID:
    return FEB_HB_FSN;
  case FEB_CAN_REAR_SENSOR_HEARTBEAT_MESSAGE_FRAME_ID:
    return FEB_HB_RSN;
  default:
    return -1;
  }
}

static void FEB_CAN_Heartbeat_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                       const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)data;
  (void)length;
  (void)user_data;

  int8_t dev = frame_to_dev(can_id);
  if (dev < 0)
  {
    return;
  }
  hb_last_rx_tick[dev] = HAL_GetTick();
}

void FEB_CAN_Heartbeat_Init(void)
{
  for (int i = 0; i < FEB_HB_COUNT; i++)
  {
    hb_last_rx_tick[i] = 0;
  }

  /* Single MASK registration covering 0xD0-0xD7 (heartbeats are 0xD0-0xD5).
   * One hardware filter bank instead of six — CAN1 only has 14 banks and the
   * RX dispatcher matches (id & mask) before invoking the callback;
   * frame_to_dev() ignores IDs in the range that aren't heartbeats. */
  FEB_CAN_RX_Params_t params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_PCU_HEARTBEAT_FRAME_ID, /* 0xD0 */
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_MASK,
      .mask = 0x7F8, /* match 0xD0-0xD7 */
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_Heartbeat_Callback,
      .user_data = NULL,
  };
  FEB_CAN_RX_Register(&params);
}

bool FEB_CAN_Heartbeat_DevFresh(FEB_HB_Device_t dev, uint32_t timeout_ms)
{
  if (dev >= FEB_HB_COUNT)
  {
    return false;
  }
  uint32_t t = hb_last_rx_tick[dev];
  if (t == 0)
  {
    return false;
  }
  return ((HAL_GetTick() - t) < timeout_ms);
}

bool FEB_CAN_Heartbeat_OthersPresent(uint32_t timeout_ms)
{
  return FEB_CAN_Heartbeat_DevFresh(FEB_HB_DASH, timeout_ms) || FEB_CAN_Heartbeat_DevFresh(FEB_HB_PCU, timeout_ms);
}
