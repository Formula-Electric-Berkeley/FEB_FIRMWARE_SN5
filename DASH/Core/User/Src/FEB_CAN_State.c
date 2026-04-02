/**
 * @file FEB_CAN_State.c
 * @brief DASH CAN state publishing module
 */

#include "FEB_CAN_State.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include <stdbool.h>
#include <string.h>

/* CAN ready flag - prevents transmission before CAN is initialized */
static volatile bool can_ready = false;

/* DASH heartbeat message data */
static struct feb_can_dash_heartbeat_t dash_heartbeat_msg;

void FEB_CAN_State_Init(void)
{
  memset(&dash_heartbeat_msg, 0, sizeof(dash_heartbeat_msg));
}

void FEB_CAN_State_SetReady(void)
{
  can_ready = true;
}

void FEB_CAN_State_Tick(void)
{
  /* Don't transmit until CAN is initialized */
  if (!can_ready)
  {
    return;
  }

  /* Divider for 100ms period (called every 1ms) */
  static uint16_t heartbeat_divider = 0;
  heartbeat_divider++;

  if (heartbeat_divider >= 100)
  {
    heartbeat_divider = 0;

    /* TODO: Wire to actual DASH error state when implemented */
    /* dash_heartbeat_msg.error0 = error_flags_0; */
    /* dash_heartbeat_msg.error1 = error_flags_1; */
    /* ... etc ... */

    /* Pack and send */
    uint8_t tx_data[FEB_CAN_DASH_HEARTBEAT_LENGTH];
    feb_can_dash_heartbeat_pack(tx_data, &dash_heartbeat_msg, sizeof(tx_data));

    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_DASH_HEARTBEAT_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                    FEB_CAN_DASH_HEARTBEAT_LENGTH);
  }
}
