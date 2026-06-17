#include "FEB_RTD.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_PCU.h"
#include "FEB_IO.h"
#include "feb_can_latest.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>

static bool rtd = false;

static bool rtd_timer_armed = false;
static uint32_t rtd_trying_to_toggle_start_tick = 0;
static bool rtd_toggle_complete = false;

// Minimum brake required for RTD activation (safety interlock).
// brake_position is centi-percent (0-10000 = 0-100%); 1000 = 10% brake.
#define RTD_BRAKE_THRESHOLD 1000

// Any RTD input older than this is treated as missing — RTD will not arm on
// stale CAN data. Matches BMS_STATE_TIMEOUT_MS.
#define RTD_INPUT_FRESHNESS_MS 1000

void FEB_State_Update_RTD(void)
{
  // MARK: Start buzzer code
  // Chime only on the normal R2D enter/exit (ENERGIZED <-> DRIVE). A fault that
  // drops us out of DRIVE (DRIVE -> FAULT_*) must NOT buzz.
  static BMS_State_t previous_bms_state = BMS_STATE_BOOT;
  BMS_State_t bms_state = FEB_CAN_BMS_GetLastState();
  if (previous_bms_state == BMS_STATE_ENERGIZED && bms_state == BMS_STATE_DRIVE)
  {
    FEB_IO_Play_Buzzer(BUZZER_DURATION_RTD_ENTER);
  }
  else if (previous_bms_state == BMS_STATE_DRIVE && bms_state == BMS_STATE_ENERGIZED)
  {
    FEB_IO_Play_Buzzer(BUZZER_DURATION_RTD_EXIT);
  }
  previous_bms_state = bms_state;

  // MARK: Signal enter rtd code
  // Send the ready to drive message over CAN when all the conditions are met
  IO_States_t states = FEB_IO_GetLastIOStates();
  uint16_t brake_pressure = FEB_CAN_PCU_GetLastBrakePosition();

  bool inputs_fresh =
      FEB_CAN_PCU_IsBrakeDataFresh(RTD_INPUT_FRESHNESS_MS) && FEB_CAN_BMS_IsDataFresh(RTD_INPUT_FRESHNESS_MS);

  // R2D may only ever be asserted while ENERGIZED or DRIVE. Any other state
  // (boot, precharge, charging, fault, ...) forces R2D false so the car can
  // never slip (back) into drive without a fresh handshake. Faults are terminal:
  // clearing R2D here guarantees a fault can never lead to drive.
  if (bms_state != BMS_STATE_ENERGIZED && bms_state != BMS_STATE_DRIVE)
  {
    rtd = false;
    rtd_timer_armed = false;
    rtd_toggle_complete = false;
    return;
  }

  // Mirrored handshake: hold the RTD button + brake >10% for RTD_SAFETY_DURATION
  // to toggle R2D. Works both ways:
  //   ENERGIZED, R2D false -> toggles true  (enter drive)
  //   DRIVE,     R2D true  -> toggles false (exit drive)
  // rtd_toggle_complete latches one toggle per press: the arm condition must
  // drop (button or brake released) before another toggle can occur, so a single
  // continuous press can never bounce drive on and off.
  if (inputs_fresh && (brake_pressure > RTD_BRAKE_THRESHOLD) && states.button_rtd)
  {
    if (!rtd_timer_armed)
    {
      rtd_timer_armed = true;
      rtd_trying_to_toggle_start_tick = HAL_GetTick();
    }
  }
  else
  {
    rtd_timer_armed = false;
  }

  if (rtd_timer_armed && (HAL_GetTick() - rtd_trying_to_toggle_start_tick >= RTD_SAFETY_DURATION))
  {
    if (!rtd_toggle_complete)
    {
      rtd = !rtd;
      FEB_IO_Play_Buzzer(rtd ? BUZZER_DURATION_RTD_ENTER : BUZZER_DURATION_RTD_EXIT);
      rtd_toggle_complete = true;
    }
  }
  else
  {
    rtd_toggle_complete = false;
  }
}

bool FEB_State_GetLastRTD(void)
{
  return rtd;
}
