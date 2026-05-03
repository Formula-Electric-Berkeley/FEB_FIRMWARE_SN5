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
#include "feb_log.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <string.h>

/* ============================================================================
 * Internal State
 * ============================================================================ */

typedef struct
{
  volatile int16_t torque;
  volatile uint8_t direction;
  volatile uint8_t enabled;
  volatile uint32_t last_rx_tick;
} RMS_State_t;

static RMS_State_t rms_state = {.torque = 0x0000, .direction = 0xFF, .enabled = 0xFF, .last_rx_tick = 0};

typedef struct
{
  volatile uint16_t brake_position;
  volatile uint32_t last_rx_tick;
} Brake_State_t;

static Brake_State_t brake_state = {.brake_position = 0, .last_rx_tick = 0};

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
  int16_t torque;
  memcpy(&torque, &data[0], sizeof(int16_t));
  rms_state.torque = torque;
  rms_state.direction = data[4];
  rms_state.enabled = data[5];
  __DMB();
  rms_state.last_rx_tick = HAL_GetTick();
}

// FEB_CAN_RMS_COMMAND_FRAME_ID:
// Byte 0-1: Brake Position Centi-percent (int16_t)
// ...

static void rx_callback_brake_position(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                       const uint8_t *data, uint8_t length, void *user_data)
{
  uint16_t position;
  memcpy(&position, &data[0], sizeof(uint16_t));
  brake_state.brake_position = position;
  __DMB();
  brake_state.last_rx_tick = HAL_GetTick();
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_PCU_Init(void)
{
  // PCU Torque Commands for the RMS
  FEB_CAN_RX_Params_t rx_params_torque = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_M192_COMMAND_MESSAGE_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0x7FF,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback_torque,
  };
  FEB_CAN_RX_Register(&rx_params_torque);

  // PCU Brake Position
  FEB_CAN_RX_Params_t rx_params_brake_position = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_BRAKE_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0x7FF,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback_brake_position,
  };
  FEB_CAN_RX_Register(&rx_params_brake_position);
}

int16_t FEB_CAN_PCU_GetLastTorque(void)
{
  return rms_state.torque;
}

int8_t FEB_CAN_PCU_GetLastDirection(void)
{
  return rms_state.direction;
}

uint8_t FEB_CAN_PCU_GetLastRMSEnabled(void)
{
  return rms_state.enabled;
}

uint16_t FEB_CAN_PCU_GetLastBrakePosition(void)
{
  return brake_state.brake_position;
}

bool FEB_CAN_PCU_IsRMSDataFresh(uint32_t timeout_ms)
{
  uint32_t last = rms_state.last_rx_tick;
  if (last == 0)
    return false;
  return (HAL_GetTick() - last) < timeout_ms;
}

bool FEB_CAN_PCU_IsBrakeDataFresh(uint32_t timeout_ms)
{
  uint32_t last = brake_state.last_rx_tick;
  if (last == 0)
    return false;
  return (HAL_GetTick() - last) < timeout_ms;
}
