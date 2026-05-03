/**
 ******************************************************************************
 * @file           : FEB_CAN_DASH.c
 * @brief          : CAN DASH Receiving Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_DASH.h"
#include "feb_can.h"
#include "feb_can_lib.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

static DASH_State_t dash_state = {};

/* ============================================================================
 * RX Callback Handlers
 * ============================================================================ */

static void rx_callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data,
                        uint8_t length, void *user_data)
{
  struct feb_can_dash_state_t msg;
  if (feb_can_dash_state_unpack(&msg, data, length) == 0)
  {
    dash_state.button1 = msg.button1;
    dash_state.button2 = msg.button2;
    dash_state.button3 = msg.button3;
    dash_state.button4 = msg.button4;
    dash_state.switch1 = msg.switch1;
    dash_state.switch2 = msg.switch2;
    dash_state.switch3 = msg.switch3;
    dash_state.switch4 = msg.switch4;

    __DMB();
    dash_state.last_rx_tick = HAL_GetTick();
  }
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_DASH_Init(void)
{
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_DASH_STATE_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback,
      .user_data = NULL,
  };

  (void)FEB_CAN_RX_Register(&rx_params);
}

DASH_State_t FEB_CAN_DASH_GetLastState()
{
  return dash_state;
}

bool FEB_CAN_DASH_IsDataFresh(uint32_t timeout_ms)
{
  uint32_t last = dash_state.last_rx_tick;
  if (last == 0)
    return false;
  return (HAL_GetTick() - last) < timeout_ms;
}
