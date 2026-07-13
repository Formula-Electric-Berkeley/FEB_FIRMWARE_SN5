/**
 * @file FEB_CAN_DASH.c
 * @brief DASH CAN message reception for BMS
 *
 * Receives the dash_state CAN message (FEB_CAN_DASH_STATE_FRAME_ID, 0x10)
 * from DASH and unpacks it into the shared DASH_IO struct.
 *
 * FreeRTOS Safety Notes:
 * - Callback executes via FEB_CAN_RX_Process() dispatch
 * - All shared fields are volatile
 * - Single-word reads/writes are atomic on Cortex-M4
 * - Timestamp is written last to ensure data consistency
 */

#include "FEB_CAN_DASH.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "stm32f4xx_hal.h"

DASH_IO_t DASH_IO = {0};

static void FEB_CAN_DASH_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                  const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;

  if (length < FEB_CAN_DASH_STATE_LENGTH)
  {
    return;
  }

  struct feb_can_dash_state_t msg;
  feb_can_dash_state_unpack(&msg, data, length);

  DASH_IO.ready_to_drive = (msg.ready_to_drive != 0);
  DASH_IO.data_logging = (msg.button2 != 0);
  DASH_IO.coolant_pump = (msg.switch1 != 0);
  DASH_IO.radiator_fan = (msg.switch2 != 0);
  DASH_IO.accumulator_fan = (msg.switch3 != 0);

  __DMB();

  DASH_IO.last_rx_tick = HAL_GetTick();
}

void FEB_CAN_DASH_Init(void)
{
  DASH_IO.ready_to_drive = false;
  DASH_IO.data_logging = false;
  DASH_IO.coolant_pump = false;
  DASH_IO.radiator_fan = false;
  DASH_IO.accumulator_fan = false;
  DASH_IO.last_rx_tick = 0;

  FEB_CAN_RX_Params_t params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_DASH_STATE_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_DASH_Callback,
      .user_data = NULL,
  };

  FEB_CAN_RX_Register(&params);
}

bool FEB_CAN_DASH_IsReadyToDrive(uint32_t timeout_ms)
{
  if (DASH_IO.last_rx_tick == 0)
  {
    return false;
  }

  uint32_t elapsed = HAL_GetTick() - DASH_IO.last_rx_tick;
  if (elapsed > timeout_ms)
  {
    return false;
  }

  return DASH_IO.ready_to_drive;
}

bool FEB_CAN_DASH_GetR2DRaw(void)
{
  return DASH_IO.ready_to_drive;
}

uint32_t FEB_CAN_DASH_GetLastRxTick(void)
{
  return DASH_IO.last_rx_tick;
}
