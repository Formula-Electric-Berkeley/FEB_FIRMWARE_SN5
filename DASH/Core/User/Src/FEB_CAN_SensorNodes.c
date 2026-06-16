/**
 ******************************************************************************
 * @file           : FEB_CAN_SensorNodes.c
 * @brief          : CAN SensorNodes Receiving Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_LVPDB.h"
#include "feb_can.h"
#include "feb_can_lib.h"
#include "feb_log.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

static uint16_t rear_speed_mph = 0; /* averaged rear wheel ground speed, whole mph */
static uint32_t last_rx_tick;

/* ============================================================================
 * RX Callback Handlers
 * ============================================================================ */

static void rx_callback_rear_wss(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data)
{
  struct feb_can_wss_rear_data_t msg;
  if (feb_can_wss_rear_data_unpack(&msg, data, length) == 0)
  {
    /* Signals are 0.01 mph/LSB; average the two wheels and rescale to whole mph
     * (sum / 2 / 100 = sum / 200). */
    rear_speed_mph = (uint16_t)((msg.wss_left_rear + msg.wss_right_rear) / 200u);
    __DMB();
    last_rx_tick = HAL_GetTick();
  }
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_SensorNodes_Init(void)
{
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_2,
      .can_id = FEB_CAN_WSS_REAR_DATA_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback_rear_wss,
      .user_data = NULL,
  };

  FEB_CAN_RX_Register(&rx_params);
}

uint16_t FEB_CAN_SensorNodes_GetLastRearWheelSpeed(void)
{
  return rear_speed_mph;
}

bool FEB_CAN_SensorNodes_IsDataFresh(uint32_t timeout_ms)
{
  uint32_t last = last_rx_tick;
  if (last == 0)
    return false;
  return (HAL_GetTick() - last) < timeout_ms;
}
