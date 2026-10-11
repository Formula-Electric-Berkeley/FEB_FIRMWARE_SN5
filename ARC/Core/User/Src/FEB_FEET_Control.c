#include "FEB_FEET_CONTROL.h"
#include "main.h"
#include "feb_log.h"

extern osMessageQueueId_t StateHandle;

// Access Y_GATE and B_GATE pins from main.h
// Usage: HAL_GPIO_WritePin(GPIOA, B_GATE_Pin | Y_GATE_Pin, GPIO_PIN_RESET);

static FEET_Status status = OFF;

FEET_Status get_feet_status()
{
  return status;
}

void set_feet_status(FEET_Status s)
{
  status = s;
}

void START_FEET_CONTROL(void const *argument)
{

  LOG_I(TAG_FEET, "%s", "FEET Task Started Successfully");

  for (;;)
  {
    osDelay(1);
  }
}