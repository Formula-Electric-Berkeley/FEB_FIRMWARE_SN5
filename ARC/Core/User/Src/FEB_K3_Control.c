#include "FEB_K3_Control.h"
#include "main.h"
#include "FEB_ARC_State.h"
#include "cmsis_os.h"
#include "feb_log.h"

// Access K3 control pin from main.h
// Usage: HAL_GPIO_WritePin(Driverless_Systems_Relay_GPIO_Port, Driverless_Systems_Relay_Pin, GPIO_PIN_RESET);

static K3_Status status = OPEN;

void START_K3_CONTROL(void const *argument)
{

  LOG_I(TAG_K3, "%s", "K3 Task Started Successfully");

  // Initially open/no current
  HAL_GPIO_WritePin(Driverless_Systems_Relay_GPIO_Port, Driverless_Systems_Relay_Pin, GPIO_PIN_RESET);

  for (;;)
  {
    if (get_state_at(MISSION) == ARC_MISSION_MANUAL)
    {
      // Manual Mission Checks
      if (get_state_at(DSB_STATE) == ARC_OFF)
      {
        K3_ON();
      }
      else
      {
        K3_OFF();
      }
    }
    else
    {
      // Auto Mission Checks (This check might be wrong, the spec just says
      // when "brakes are applied", but also "sufficient brake pressure is built up")
      if (get_state_at(MISSION) == ARC_MISSION_AUTO &&
          get_state_at(DSMS_ON) == ARC_ON &&
          get_state_at(STARTUP_CHECK) == ARC_DONE)
      {
        K3_ON();
      }
      else
      {
        K3_OFF();
      }
    }
    osDelay(10); // 100Hz
  }
}

void K3_ON(void)
{
  if (status == OPEN)
  {
    HAL_GPIO_WritePin(Driverless_Systems_Relay_GPIO_Port, Driverless_Systems_Relay_Pin, GPIO_PIN_SET);
    LOG_I(TAG_K3, "%s", "K3 Closed!!!");
    status = CLOSED;
  }
}

void K3_OFF(void)
{
  if (status == CLOSED)
  {
    HAL_GPIO_WritePin(Driverless_Systems_Relay_GPIO_Port, Driverless_Systems_Relay_Pin, GPIO_PIN_RESET);
    LOG_I(TAG_K3, "%s", "K3 Opened!!!");
    status = OPEN;
  }
}