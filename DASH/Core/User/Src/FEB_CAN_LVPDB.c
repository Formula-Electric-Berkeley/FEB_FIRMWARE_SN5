/**
 ******************************************************************************
 * @file           : FEB_CAN_LVPDB.c
 * @brief          : CAN LVPDB Receiving Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_LVPDB.h"
#include "feb_can.h"
#include "feb_can_lib.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

typedef struct
{
  volatile uint16_t lv_24v_voltage;
  volatile uint16_t lv_12v_voltage;
  volatile uint32_t last_rx_tick;
} LVPDB_State_t;

static LVPDB_State_t lvpdb_state = {.lv_24v_voltage = 0, .lv_12v_voltage = 0, .last_rx_tick = 0};

/* ============================================================================
 * RX Callback Handlers
 * ============================================================================ */

// FEB_CAN_LVPDB_LV_24V_BUS_AND_12V_BUS_VOLTAGES_FRAME_ID (0x16):
// Byte 0-1: lv_24v_voltage
// Byte 2-3: lv_12v_voltage

static void rx_callback_lv_voltages(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                    const uint8_t *data, uint8_t length, void *user_data)
{
  struct feb_can_lvpdb_lv_24v_bus_and_12v_bus_voltages_t msg;
  if (feb_can_lvpdb_lv_24v_bus_and_12v_bus_voltages_unpack(&msg, data, length) == 0)
  {
    lvpdb_state.lv_24v_voltage = msg.lv_24v_voltage;
    lvpdb_state.lv_12v_voltage = msg.lv_12v_voltage;
    __DMB();
    lvpdb_state.last_rx_tick = HAL_GetTick();
  }
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_LVPDB_Init(void)
{
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_LVPDB_LV_24V_BUS_AND_12V_BUS_VOLTAGES_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback_lv_voltages,
      .user_data = NULL,
  };

  (void)FEB_CAN_RX_Register(&rx_params);
}

uint16_t FEB_CAN_LVPDB_GetLast24VVoltage(void)
{
  return lvpdb_state.lv_24v_voltage;
}

uint16_t FEB_CAN_LVPDB_GetLast12VVoltage(void)
{
  return lvpdb_state.lv_12v_voltage;
}

bool FEB_CAN_LVPDB_IsDataFresh(uint32_t timeout_ms)
{
  uint32_t last = lvpdb_state.last_rx_tick;
  if (last == 0)
    return false;
  return (HAL_GetTick() - last) < timeout_ms;
}
