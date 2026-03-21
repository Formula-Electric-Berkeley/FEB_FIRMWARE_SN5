/**
 ******************************************************************************
 * @file           : FEB_CAN_PCU.c
 * @brief          : CAN PCU Receiving Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_PCU.h"
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

RMS_State_t rms_state = {.torque = 0x0000, .direction = 0xFF, .enabled = 0xFF};

typedef struct
{
  uint16_t break_position;
} Break_State_t;

Break_State_t break_state = {.break_position = 0};

/* ============================================================================
 * RX Callback Handlers
 * ============================================================================ */

// FEB_CAN_RMS_COMMAND_FRAME_ID:
// Byte 0-1: Torque (int16_t)
// Byte 4:   Direction
// Byte 5:   Enabled

static void rx_callback_torque(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                               const uint8_t *data, uint8_t length, void *user_data)
{
  memcpy(&rms_state.torque, &data[0], sizeof(int16_t));
  rms_state.direction = data[4];
  rms_state.enabled = data[5];
}

// FEB_CAN_RMS_COMMAND_FRAME_ID:
// Byte 0-1: Break Position Centi-percent (int16_t)
// ...

static void rx_callback_break_position(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                       const uint8_t *data, uint8_t length, void *user_data)
{
  memcpy(&break_state.break_position, &data[0], sizeof(int16_t));
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_PCU_Init(void)
{
  // PCU Torque Commands for the RMS
  FEB_CAN_RX_Params_t rx_params_torque = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_RMS_COMMAND_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0x7FF,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback_torque,
  };
  FEB_CAN_RX_Register(&rx_params_torque);

  // PCU Break Position
  FEB_CAN_RX_Params_t rx_params_break_position = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_BRAKE_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0x7FF,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback_break_position,
  };
  FEB_CAN_RX_Register(&rx_params_break_position);
}

int16_t FEB_CAN_PCU_GetLastTorque(void)
{
  return rms_state.torque;
}

int8_t FEB_CAN_PCU_GetLastDirection(void)
{
  return rms_state.direction;
}

int8_t FEB_CAN_PCU_GetLastRMSEnabled(void)
{
  return rms_state.enabled;
}

uint16_t FEB_CAN_PCU_GetLastBreakPosition(void)
{
  return break_state.break_position;
}
