/**
 * @file FEB_CAN_State.c
 * @brief BMS CAN state publishing module
 */

#include "FEB_CAN_State.h"
#include "FEB_CAN_DASH.h"
#include "FEB_SM.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <string.h>

/* Note: Critical sections removed - current_state is volatile and 1 byte (atomic on ARM) */

/* R2D timeout for state transitions */
#define R2D_TIMEOUT_MS 500

/* CAN ready flag - prevents transmission before CAN is initialized */
static volatile bool can_ready = false;

/* Current BMS state - volatile for ISR/task access */
static volatile BMS_State_t current_state = BMS_STATE_BOOT;

/* BMS state message data */
static struct feb_can_bms_state_t bms_state_msg;

/* State name lookup table - must match BMS_State_t enum order */
static const char *state_names[] = {
    "BOOT",              // 0
    "LV_POWER",          // 1
    "BUS_HEALTH_CHECK",  // 2
    "PRECHARGE",         // 3
    "ENERGIZED",         // 4
    "DRIVE",             // 5
    "BATTERY_FREE",      // 6
    "CHARGER_PRECHARGE", // 7
    "CHARGING",          // 8
    "BALANCE",           // 9
    "FAULT_BMS",         // 10
    "FAULT_BSPD",        // 11
    "FAULT_IMD",         // 12
    "FAULT_CHARGING",    // 13
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

    /* Use authoritative state from FEB_SM so PCU always gets most recent state */
    bms_state_msg.bms_state = (uint8_t)FEB_SM_Get_Current_State();

    /* Pack and send */
    uint8_t tx_data[FEB_CAN_BMS_STATE_LENGTH];
    feb_can_bms_state_pack(tx_data, &bms_state_msg, sizeof(tx_data));

    FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BMS_STATE_FRAME_ID, FEB_CAN_ID_STD, tx_data, FEB_CAN_BMS_STATE_LENGTH);
  }
}

void FEB_CAN_State_ProcessTransitions(void)
{
  /* Don't process until CAN is initialized */
  if (!can_ready)
  {
    return;
  }

  /*
   * NOTE: This function now routes through FEB_SM_Transition() to ensure
   * proper relay control and state validation. Previously this function
   * directly modified current_state, bypassing the state machine's
   * relay control logic.
   *
   * The FEB_SM_Process() function (called from timer ISR) already handles
   * ENERGIZED <-> DRIVE transitions via EnergizedTransition() and
   * DriveTransition(). This function is kept for API compatibility.
   */
  BMS_State_t state = FEB_SM_Get_Current_State();

  /* ENERGIZED -> DRIVE: When R2D is active and fresh */
  if (state == BMS_STATE_ENERGIZED)
  {
    if (FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    {
      FEB_SM_Transition(BMS_STATE_DRIVE);
    }
  }
  /* DRIVE -> ENERGIZED: When R2D is inactive or stale */
  else if (state == BMS_STATE_DRIVE)
  {
    if (!FEB_CAN_DASH_IsReadyToDrive(R2D_TIMEOUT_MS))
    {
      FEB_SM_Transition(BMS_STATE_ENERGIZED);
    }
  }
}
