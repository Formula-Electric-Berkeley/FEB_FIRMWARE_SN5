#include "FEB_K3_CONTROL.h"
#include "main.h"
#include "feb_log.h"

extern osMessageQueueId_t StateHandle;

// Access K3 control pin from main.h
// Usage: HAL_GPIO_WritePin(Driverless_Systems_Relay_GPIO_Port, Driverless_Systems_Relay_Pin, GPIO_PIN_RESET);

static K3_Status status = OPEN;

void START_K3_CONTROL(void const *argument)
{

  LOG_I(TAG_K3, "%s", "K3 Task Started Successfully");

  for (;;)
  {
    osDelay(1);
  }
}