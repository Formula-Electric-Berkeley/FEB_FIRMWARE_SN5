/**
 * @file FEB_CAN_State.c
 * @brief BMS CAN state publishing module
 */

#include "FEB_CAN_State.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include <stdbool.h>
#include <string.h>

/* CAN ready flag - prevents transmission before CAN is initialized */
static volatile bool can_ready = false;

/* BMS state message data */
static struct feb_can_bms_state_t bms_state_msg;

void FEB_CAN_State_Init(void)
{
  memset(&bms_state_msg, 0, sizeof(bms_state_msg));
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
  static uint16_t state_divider = 0;
  state_divider++;

  if (state_divider >= 100)
  {
    state_divider = 0;

    /* TODO: Wire to actual BMS state when state machine is implemented */
    /* bms_state_msg.bms_state = current_state; */
    /* bms_state_msg.relay_state = relay_status; */
    /* bms_state_msg.gpio_sense = gpio_reading; */
    /* bms_state_msg.ping_lv_nodes = ping_value; */

    /* Pack and send */
    uint8_t tx_data[FEB_CAN_BMS_STATE_LENGTH];
    feb_can_bms_state_pack(tx_data, &bms_state_msg, sizeof(tx_data));

    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_STATE_FRAME_ID, FEB_CAN_ID_STD, tx_data, FEB_CAN_BMS_STATE_LENGTH);
  }
}
