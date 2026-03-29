#include "FEB_RTD.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_PCU.h"
#include "FEB_IO.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>

static uint32_t rtd_button_press_start_tick = 0;
static bool previous_rtd_button = false;

void FEB_State_Update_RTD(void)
{
  // uint8_t brake_pressure = FEB_CAN_PCU_GetLastBreakPosition();
  // uint8_t inv_enabled = FEB_CAN_PCU_GetLastRMSEnabled();
  BMS_State_t bms_state = FEB_CAN_BMS_GetLastState();
  BMS_State_t previous_bms_state = FEB_CAN_BMS_GetLastState();

  IO_Switch_States_t states = FEB_IO_GetLastIOStates();

  if (previous_bms_state == BMS_STATE_ENERGIZED && bms_state == BMS_STATE_DRIVE)
  {
    FEB_IO_Play_Buzzer(BUZZER_DURATION_RTD_ENTER);
  }
  else if (previous_bms_state == BMS_STATE_DRIVE && bms_state == BMS_STATE_ENERGIZED)
  {
    FEB_IO_Play_Buzzer(BUZZER_DURATION_RTD_EXIT);
  }

  if (previous_rtd_button == false && states.button_rtd)
  {
    rtd_button_press_start_tick = HAL_GetTick();
  }

  if (states.button_rtd && rtd_button_press_start_tick + RTD_BUTTON_HOLD_DURATION < HAL_GetTick())
  {
    FEB_IO_Play_Buzzer(BUZZER_DURATION_RTD_ENTER);
  }

  previous_rtd_button = states.button_rtd;

  if (states.button_rtd)
  {
    printf("Button pressed");
  }
}
