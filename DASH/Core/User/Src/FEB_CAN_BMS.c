/**
 ******************************************************************************
 * @file           : FEB_CAN_BMS.c
 * @brief          : CAN BMS Receiving Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_BMS.h"
#include "feb_can.h"
#include "feb_can_lib.h"
#include "feb_uart.h"
#include "feb_uart_log.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Internal State
 * ============================================================================
 */

typedef struct BMS_MESSAGE_TYPE
{
  volatile uint16_t temperature;       // Updated in ISR, read in main loop
  volatile uint16_t voltage;           // Updated in ISR, read in main loop (in 0.1V units)
  volatile FEB_SM_ST_t state;          // Updated in ISR, read in main loop
  volatile FEB_HB_t ping_ack;          // Updated in ISR, read in main loop
  volatile float max_temperature;      // Max accumulator temperature in C
  volatile float accumulator_voltage;  // Accumulator voltage in V
  volatile uint32_t last_rx_timestamp; // 0 = never received, else HAL_GetTick() when last RX
} BMS_Message_t;

static BMS_State_t state = 0;
static BMS_Message_t message = {};
uint16_t BMS_Temperature = -1; // negative value indicates uninitialized state lol

/* ============================================================================
 * RX Callback Handlers
 * ============================================================================
 */

// FEB_CAN_BMS_STATE_FRAME_ID:
// Byte 0 (first 5 bits): BMS_State_t (enum)

static void rx_callback_bms_state(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                  const uint8_t *data, uint8_t length, void *user_data)
{
  state = data[0] >> 3;
}

/* ============================================================================
 * API Implementation
 * ============================================================================
 */

void FEB_CAN_BMS_Init(void)
{
  FEB_CAN_RX_Params_t rx_params_bms_state = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_BMS_STATE_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0x7FF,
      .fifo = FEB_CAN_FIFO_0,
      .callback = rx_callback_bms_state,
      .user_data = NULL,
  };

  FEB_CAN_RX_Register(&rx_params_bms_state);
}

BMS_State_t FEB_CAN_BMS_GetLastState(void)
{
  return state;
}

uint16_t FEB_CAN_BMS_getTemp(void)
{
  return message.temperature;
}
