/**
 * @file FEB_CAN_State.c
 * @brief DASH CAN state publishing module
 */

#include "FEB_CAN_State.h"
#include "FEB_IO.h"
#include "FEB_RTD.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "feb_log.h"
#include <stdbool.h>
#include <string.h>
#include "FEB_IO.h"

#define TAG_STATE "[STATE]"

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

bool FEB_CAN_State_IsReady(void)
{
  return can_ready;
}

void FEB_CAN_State_Tick(void)
{
  /* Defense in depth: the TX task already gates on FEB_CAN_State_IsReady(),
     so this branch should never be taken in practice. Stay silent here so we
     don't spam the console at 1 kHz during the brief init window if a future
     caller forgets to gate. */
  if (!can_ready)
  {
    return;
  }

  uint8_t tx_data[FEB_CAN_DASH_TPS_LENGTH];
  memset(tx_data, 0x00, sizeof(tx_data));
  // feb_can_dash_tps_pack(tx_data, &((struct feb_can_dash_tps_t){.current = }), sizeof(tx_data));
  // FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_DASH_TPS_FRAME_ID, FEB_CAN_ID_STD, tx_data, FEB_CAN_DASH_TPS_LENGTH);

  /* Divider for 100ms period (called every 1ms) */
  static uint16_t heartbeat_divider = 0;
  heartbeat_divider++;
  if (heartbeat_divider >= 100)
  {
    heartbeat_divider = 0;

    dash_heartbeat_msg.io_expander_error = !FEB_IO_StatusOk();

    uint8_t tx_data[FEB_CAN_DASH_HEARTBEAT_LENGTH];
    memset(tx_data, 0x00, sizeof(tx_data));
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

    uint8_t tx_data[FEB_CAN_DASH_STATE_LENGTH];
    memset(tx_data, 0x00, sizeof(tx_data));

    IO_States_t states = FEB_IO_GetLastIOStates();

    if (feb_can_dash_state_pack(tx_data,
                                &((struct feb_can_dash_state_t){.buzzer = states.buzzer_enabled,
                                                                .button1 = states.button_rtd,
                                                                .button2 = states.button_2,
                                                                .button3 = states.button_3,
                                                                .button4 = states.button_4,
                                                                .switch1 = states.switch_accumulator_fans,
                                                                .switch2 = states.switch_coolant_pump_radiator_fan,
                                                                .switch3 = states.switch_logging,
                                                                .switch4 = states.switch_4,
                                                                .ready_to_drive = FEB_State_GetLastRTD()}),
                                sizeof(tx_data)) == FEB_CAN_DASH_STATE_LENGTH)
    {
      FEB_CAN_Status_t st = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_DASH_STATE_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                                            FEB_CAN_DASH_STATE_LENGTH);
      if (st == FEB_CAN_OK)
      {
        // LOG_D(TAG_STATE, "Sending Dash IO State Over CAN: %02X %02X", tx_data[0], tx_data[1]);
      }
      else
      {
        LOG_W(TAG_STATE, "DASH state TX dropped: %s", FEB_CAN_StatusToString(st));
      }
    }
  }
}
