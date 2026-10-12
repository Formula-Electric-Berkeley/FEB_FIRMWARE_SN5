#include "FEB_ARC_State.h"
#include "main.h"
#include "feb_log.h"
#include "feb_can_lib.h"
#include <stdint.h>

extern osMessageQueueId_t StateHandle;

uint16_t state[16] = {0}; // 32 byte state

uint16_t *get_state_arr()
{
  return state;
}

uint16_t get_state_at(State_item i)
{
  return state[i];
}

void set_state_at(State_item i, uint16_t val)
{
  state[i] = val;
}

void START_STATE_READ(void const *argument)
{

  LOG_I(TAG_ARC_STATE, "%s", "State Task Started Successfully");

  // TODO: Define heartbeat here

  // TODO: Define can listeners here

  for (;;)
  {
    osDelay(1);
  }
}