#include "FEB_P_Sense.h"
#include "main.h"
#include "feb_log.h"
#include "feb_can_lib.h"

extern osMessageQueueId_t canTxQueueHandle;
extern osMutexId_t canTxMutexHandle;
extern osSemaphoreId_t canTxMailboxSemHandle;

extern osMessageQueueId_t StateHandle;

static DSB_Status status = OFF;

DSB_Status get_status()
{
  return status;
}

void set_status(DSB_Status s)
{
  status = s;
}

void START_P_SENSE(void const *argument)
{

  LOG_I(TAG_P_SENSE, "%s", "P_Sense Task Started Successfully");

  for (;;)
  {
    osDelay(1);
  }
}