/**
 ******************************************************************************
 * @file           : PCU_Torque.cpp
 * @brief          : Every FreeRTOS task body on DASH
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "PCU_ADC.hpp"
#include "PCU_CAN.h"
#include "PCU_Commands.h"

#include "cmsis_os2.h"
#include "feb_can_tasks.hpp"
#include "feb_uart.h"
#include "stm32f4xx_hal.h"

void PCU_Tick()
{
  // uint32_t now = HAL_GetTick(); // gets the time in ms since boot
  // //uint32_t last_sample_ms = 0;
  // if ((now - last_sample_ms) < PCU_SAMPLE_PERIOD_MS)
  // {
  // return;
  // }
  // last_sample_ms = now;

  // bool apps_plausible = validate_torque(apps1, apps2) && apps1.in_range() && apps2.in_range();
  // if (apps_plausible)
  // {
  // last_plausible_ms = now;
  // }
  // //apps_fault = (now - last_plausible_ms) > APPS_IMPLAUSIBILITY_TIME_MS;

  // torque_command = 0;
  // if (PCU_CAN_IsR2D() && !apps_fault)
  // {
  // torque_command = apps1.get_torque();
  // }
}
