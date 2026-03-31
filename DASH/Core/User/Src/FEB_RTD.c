#include "FEB_RTD.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_PCU.h"
#include "FEB_IO.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>

static uint32_t rtd_button_press_start_tick = 0;
static bool previous_rtd_button = false;

static Buzzing_State_t buzzing_state =
    NOT_BUZZED; // Here to ensure FEB_IO_Play_Buzzer() is not spammed and only called once

void FEB_State_Update_RTD(void)
{
  // MARK: Start buzzer code
  // to react to BMS entering and exiting drive state
  BMS_State_t bms_state = FEB_CAN_BMS_GetLastState();
  BMS_State_t previous_bms_state = FEB_CAN_BMS_GetLastState();
  if (previous_bms_state == BMS_STATE_ENERGIZED && bms_state == BMS_STATE_DRIVE && buzzing_state != BUZZED_ENTER_RTD)
  {
    FEB_IO_Play_Buzzer(BUZZER_DURATION_RTD_ENTER);
    buzzing_state = BUZZED_ENTER_RTD;
  }
  else if (previous_bms_state == BMS_STATE_DRIVE && bms_state == BMS_STATE_ENERGIZED &&
           buzzing_state != BUZZED_EXIT_RTD)
  {
    FEB_IO_Play_Buzzer(BUZZER_DURATION_RTD_EXIT);
    buzzing_state = BUZZED_EXIT_RTD;
  }

  // MARK: Signal enter rtd code
  // Send the ready to drive message over CAN when all the conditions are met
  IO_Switch_States_t states = FEB_IO_GetLastIOStates();
  uint8_t brake_pressure = FEB_CAN_PCU_GetLastBreakPosition();
  uint8_t inv_enabled = FEB_CAN_PCU_GetLastRMSEnabled();

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
