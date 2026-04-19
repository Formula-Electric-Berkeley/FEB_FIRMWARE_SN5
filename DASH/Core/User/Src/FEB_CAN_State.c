/**
 * @file FEB_CAN_State.c
 * @brief DASH CAN state publishing module
 */

#include "FEB_CAN_State.h"
#include "FEB_IO.h"
#include "FEB_RTD.h"
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

  /* CAN_DASH_STATE_FRAME transmission - rate limited to 100ms
   * Bit layout:
   * Byte 0: [0] button_rtd, [4] switch_coolant_pump_radiator_fan,
   *         [5] switch_accumulator_fans, [6] switch_logging
   * Byte 1: [0] buzzer_enabled, [1] ready_to_drive
   */
  static uint16_t state_divider = 0;
  state_divider++;

  if (state_divider >= 100)
  {
    state_divider = 0;

    uint8_t tx_data[2];
    memset(tx_data, 0x00, sizeof(tx_data));

    IO_State_t states = FEB_IO_GetLastIOStates();
    bool rtd = FEB_State_GetLastRTD();

    tx_data[0] = (states.button_rtd << 0) + (states.switch_coolant_pump_radiator_fan << 4) +
                 (states.switch_accumulator_fans << 5) + (states.switch_logging << 6);
    tx_data[1] = (states.buzzer_enabled << 0) + (rtd << 1);

    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_DASH_STATE_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                    FEB_CAN_DASH_STATE_LENGTH);
  }
}
