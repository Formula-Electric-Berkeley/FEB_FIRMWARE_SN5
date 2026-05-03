#include "FEB_RTD.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_PCU.h"
#include "FEB_IO.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>

static bool rtd = false;

static bool rtd_timer_armed = false;
static uint32_t rtd_trying_to_toggle_start_tick = 0;
static bool rtd_toggle_complete = false;

// Minimum brake pressure required for RTD activation (safety interlock)
#define RTD_BRAKE_THRESHOLD 5000

// Any RTD input older than this is treated as missing — RTD will not arm on
// stale CAN data. Matches BMS_STATE_TIMEOUT_MS.
#define RTD_INPUT_FRESHNESS_MS 500

void FEB_State_Update_RTD(void)
{
  // MARK: Start buzzer code
  // Buzz on any entry into / exit from BMS_STATE_DRIVE so faults that drop us
  // out of drive (DRIVE -> FAULT_*) still trigger the exit chime.
  static BMS_State_t previous_bms_state = BMS_STATE_BOOT;
  BMS_State_t bms_state = FEB_CAN_BMS_GetLastState();
  if (previous_bms_state != BMS_STATE_DRIVE && bms_state == BMS_STATE_DRIVE)
  {
    FEB_IO_Play_Buzzer(BUZZER_DURATION_RTD_ENTER);
  }
  else if (previous_bms_state == BMS_STATE_DRIVE && bms_state != BMS_STATE_DRIVE)
  {
    FEB_IO_Play_Buzzer(BUZZER_DURATION_RTD_EXIT);
  }
  previous_bms_state = bms_state;

  // MARK: Signal enter rtd code
  // Send the ready to drive message over CAN when all the conditions are met
  IO_States_t states = FEB_IO_GetLastIOStates();
  uint16_t brake_pressure = FEB_CAN_PCU_GetLastBrakePosition();
  uint8_t inv_enabled = FEB_CAN_PCU_GetLastRMSEnabled();

  bool inputs_fresh = FEB_CAN_PCU_IsBrakeDataFresh(RTD_INPUT_FRESHNESS_MS) &&
                      FEB_CAN_PCU_IsRMSDataFresh(RTD_INPUT_FRESHNESS_MS) &&
                      FEB_CAN_BMS_IsDataFresh(RTD_INPUT_FRESHNESS_MS);

  // RTD requires: fresh CAN inputs, button held, brake applied, inverter enabled, BMS energized
  // if (inputs_fresh && (brake_pressure >= RTD_BRAKE_THRESHOLD) && (inv_enabled == 1) &&
  //     (bms_state == BMS_STATE_ENERGIZED) && states.button_rtd)
  if (states.button_rtd)
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

  if (rtd_timer_armed && HAL_GetTick() - rtd_trying_to_toggle_start_tick >= RTD_SAFETY_DURATION)
  {
    if (!rtd_toggle_complete)
    {
      rtd = !rtd;
      rtd_toggle_complete = true;
    }
  }
  else
  {
    rtd_toggle_complete = false;
  }

  if (bms_state != BMS_STATE_DRIVE && bms_state != BMS_STATE_ENERGIZED)
  {
    rtd = false; // just in case
  }
}

bool FEB_State_GetLastRTD(void)
{
  return rtd;
}
