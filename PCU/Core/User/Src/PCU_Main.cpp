/**
 ******************************************************************************
 * @file           : PCU_Main.cpp
 * @brief          : PCU Main Loop
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "main.h"
#include "PCU_Main.h"
#include "PCU_CAN.hpp"
#include "PCU_ADC.hpp"
#include "PCU_UART.hpp"
#include <cstdint>

static uint32_t last_sample_ms = 0;
static uint32_t last_plausible_ms = 0;
static bool apps_fault = false;
static uint16_t torque_command = 0;

/*
  Implausiblility:  sensors disagree by >10% pedal travel
                    >100 ms → motor controller cuts motor power

  PCU Specifics:    Brakes on + APPS >25% → cut motor power
                    Stays cut until APPS <5%, with or without brakes
*/

void PCU_Main_Setup(void)
{
  PCU_UART_Init();
  PCU_CAN_Init();
  PCU_ADC_Init();
}

void PCU_Main_Loop(void)
{
  PCU_UART_Process();

  uint32_t now = HAL_GetTick();
  if ((now - last_sample_ms) < PCU_SAMPLE_PERIOD_MS)
  {
    return;
  }
  last_sample_ms = now;

  bool apps_plausible = validate_torque(apps1, apps2) && apps1.in_range() && apps2.in_range();
  if (apps_plausible)
  {
    last_plausible_ms = now;
  }
  apps_fault = (now - last_plausible_ms) > APPS_IMPLAUSIBILITY_TIME_MS;

  torque_command = 0;
  if (PCU_CAN_IsR2D() && !apps_fault)
  {
    torque_command = apps1.get_torque();
  }
}
