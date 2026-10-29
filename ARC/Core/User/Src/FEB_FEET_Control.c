#include "FEB_FEET_Control.h"
#include "FEB_ARC_State.h"
#include "main.h"
#include "cmsis_os.h"
#include "feb_log.h"
#include <stdint.h>

// Access Y_GATE and B_GATE pins from main.h
// Usage: HAL_GPIO_WritePin(GPIOA, B_GATE_Pin | Y_GATE_Pin, GPIO_PIN_RESET);

static uint16_t blink_time = 0;
static const int dt = 10; // 100 hz check, can change if this is too frequent
static const int T = 500; // blink every 500 ms

void START_FEET_CONTROL(void const *argument)
{

  LOG_I(TAG_FEET, "%s", "FEET Task Started Successfully");

  for (;;)
  {
    switch (get_state_at(DRIVERLESS_STATUS))
    {
    case FEET_STATE_OFF:
    case FEET_STATE_READY:
    case FEET_STATE_FINISHED:
      break;        // No changes for these states
    case FEET_STATE_DRIVING: // Blink if needed and reset timer
      if (blink_time >= T)
      {
        toggle_yellow();
        blink_time = 0;
      }
      break;
    case FEET_STATE_EMERGENCY: // Blink if needed and reset timer
      if (blink_time >= T)
      {
        toggle_blue();
        blink_time = 0;
      }
      break;
    default:
      LOG_I(TAG_FEET, "%s", "No DS state Received yet!!!");
    }
    osDelay(dt);
    blink_time += dt;
  }
}

void update_feet_state()
{
  blink_time = 0;
  switch (get_state_at(DRIVERLESS_STATUS))
  {
  case FEET_STATE_OFF:
    lights_off();
    break;
  case FEET_STATE_READY:
  case FEET_STATE_DRIVING:
    lights_off();
    set_yellow();
    break;
  case FEET_STATE_FINISHED:
  case FEET_STATE_EMERGENCY:
    lights_off();
    set_blue();
    break;
  default:
    LOG_E(TAG_FEET, "%s", "No DS state Received yet!!!");
  }
}

// Helpers:

void toggle_blue()
{
  HAL_GPIO_TogglePin(GPIOA, B_GATE_Pin);
}

void toggle_yellow()
{
  HAL_GPIO_TogglePin(GPIOA, Y_GATE_Pin);
}

void set_blue()
{
  HAL_GPIO_WritePin(GPIOA, B_GATE_Pin, GPIO_PIN_SET);
}

void set_yellow()
{
  HAL_GPIO_WritePin(GPIOA, Y_GATE_Pin, GPIO_PIN_SET);
}

void lights_off()
{
  HAL_GPIO_WritePin(GPIOA, B_GATE_Pin | Y_GATE_Pin, GPIO_PIN_RESET);
}