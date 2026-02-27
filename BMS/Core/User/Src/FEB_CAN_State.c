/**
 * @file FEB_CAN_State.c
 * @brief BMS CAN state publishing module
 */

#include "FEB_CAN_State.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include <stdbool.h>
#include <string.h>

/* CAN ready flag - prevents transmission before CAN is initialized */
static volatile bool can_ready = false;

/* Current BMS state - volatile for ISR/task access */
static volatile BMS_State_t current_state = BMS_STATE_BOOT;

/* BMS state message data */
static struct feb_can_bms_state_t bms_state_msg;

/* State name lookup table */
static const char *state_names[] = {
    "BOOT",  "ORIGIN", "LV_POWER", "BUS_HEALTH_CHECK", "PRECHARGE", "ENERGIZED",
    "DRIVE", "FAULT",  "CHARGING", "BATTERY_FREE",     "BALANCE",
};

void FEB_CAN_State_Init(void)
{
  memset(&bms_state_msg, 0, sizeof(bms_state_msg));
  current_state = BMS_STATE_BOOT;
}

void FEB_CAN_State_SetReady(void)
{
  can_ready = true;
}

BMS_State_t FEB_CAN_State_GetState(void)
{
  return current_state;
}

int FEB_CAN_State_SetState(BMS_State_t state)
{
  if (state >= BMS_STATE_COUNT)
  {
    return -1;
  }
  current_state = state;
  return 0;
}

const char *FEB_CAN_State_GetStateName(BMS_State_t state)
{
  if (state >= BMS_STATE_COUNT)
  {
    return "UNKNOWN";
  }
  return state_names[state];
}

void FEB_CAN_State_Tick(void)
{
  /* Don't transmit until CAN is initialized */
  if (!can_ready)
  {
    return;
  }

  /* Divider for 100ms period (called every 1ms) */
  static uint16_t state_divider = 0;
  state_divider++;

  if (state_divider >= 100)
  {
    state_divider = 0;

    /* Update message with current state */
    bms_state_msg.bms_state = (uint8_t)current_state;

    /* Pack and send */
    uint8_t tx_data[FEB_CAN_BMS_STATE_LENGTH];
    feb_can_bms_state_pack(tx_data, &bms_state_msg, sizeof(tx_data));

    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_STATE_FRAME_ID, FEB_CAN_ID_STD, tx_data, FEB_CAN_BMS_STATE_LENGTH);
  }
}
