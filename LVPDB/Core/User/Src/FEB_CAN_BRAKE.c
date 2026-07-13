/**
 ******************************************************************************
 * @file           : FEB_CAN_BRAKE.c
 * @brief          : CAN BRAKE Receiving Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_BRAKE.h"
#include "feb_can.h"
#include "feb_can_lib.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

static BRAKE_State_t brake_state = {};

/* ============================================================================
 * RX Callback Handlers
 * ============================================================================ */

static void rx_callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data,
                        uint8_t length, void *user_data)
{
  struct feb_can_brake_t msg;
  if (feb_can_brake_unpack(&msg, data, length) == 0)
  {
    brake_state.brake_position = msg.brake_position;

    __DMB();
    brake_state.last_rx_tick = HAL_GetTick();
  }
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_BRAKE_Init(void)
{
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_BRAKE_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback,
      .user_data = NULL,
  };

  (void)FEB_CAN_RX_Register(&rx_params);
}

uint8_t FEB_CAN_BRAKE_GetPercent(void)
{
  /* brake_position is centi-percent (0-10000); return whole percent (0-100). */
  return (uint8_t)(brake_state.brake_position / 100u);
}

bool FEB_CAN_BRAKE_IsDataFresh(uint32_t timeout_ms)
{
  uint32_t last = brake_state.last_rx_tick;
  if (last == 0)
    return false;
  return (HAL_GetTick() - last) < timeout_ms;
}
