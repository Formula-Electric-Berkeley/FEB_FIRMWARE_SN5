/**
 ******************************************************************************
 * @file           : FEB_CAN_RMS.c
 * @brief          : CAN RMS Recieving Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_can.h"
#include "feb_can_lib.h"
#include "feb_uart.h"
#include "feb_uart_log.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* ============================================================================
 * Internal State
 * ============================================================================ */

typedef struct
{
  int16_t torque;
  uint8_t direction;
  uint8_t enabled;
} RMS_State_t;

RMS_State_t state = {.torque = 0xFF, .direction = 0xFF, .enabled = 0xFF};

/* ============================================================================
 * RX Callback Handlers
 * ============================================================================ */

// clang-format: off
//
// FEB_CAN_RMS_COMMAND_FRAME_ID, Byte_0, Byte_1, Byte_2, Byte_3, Byte_4,   Byte_5, Byte_6, Byte_7
// ID                            Torque========                  Direction Enabled
//
// clang-format: on

static void rx_callback_ch1(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                            const uint8_t *data, uint8_t length, void *user_data)
{
  memcopy(&state.torque, &data[0], sizeof(int16_t));
  state.torque = data[4];
  state.torque = data[5];
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_RMS_Init(void)
{
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_RMS_COMMAND_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0x7FF,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback_ch1,
  };
  FEB_CAN_RX_Register(&rx_params);
}

int16_t FEB_CAN_RMS_GetLastTorque(void)
{
  return state.torque;
}

int8_t FEB_CAN_RMS_GetLastDirection(void)
{
  return state.direction;
}

int8_t FEB_CAN_RMS_GetLastEnabled(void)
{
  return state.direction;
}
