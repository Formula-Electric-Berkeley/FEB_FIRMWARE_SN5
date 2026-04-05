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

  // CAN_DASH_STATE_FRAME Bits
  // 0: button1
  // 1: button2
  // 2: button3
  // 3: button4
  // 4: switch4
  // 5: switch5
  // 6: switch6
  // 7: switch7
  // 8: buzzer on/off
  // 9: ready_to_drive

  uint8_t tx_data[2];
  memset(tx_data, 0x00, sizeof(tx_data));

  IO_States_t states = FEB_IO_GetLastIOStates();
  bool rtd = FEB_State_GetLastRTD();

  tx_data[0] = (states.button_rtd << 0) + (states.switch_coolant_pump_radiator_fan << 4) +
               (states.switch_accumulator_fans << 5) + (states.switch_logging << 6);
  tx_data[1] = (states.buzzer_enabled << 0) + (rtd << 1);

  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_DASH_STATE_FRAME_ID, FEB_CAN_ID_STD, tx_data, FEB_CAN_DASH_STATE_LENGTH);
}
