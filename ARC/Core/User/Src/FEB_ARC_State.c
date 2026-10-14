#include "FEB_ARC_State.h"
#include "FEB_P_Sense.h"
#include "main.h"
#include "feb_log.h"
#include "feb_can_lib.h"
#include <stdint.h>

extern osMessageQueueId_t StateHandle;
// HAL_GPIO_ReadPin(FROM_TSActivation_GPIO_Port, FROM_TSActivation_Pin);
// HAL_GPIO_ReadPin(GPIOB, DSMS_ON_Pin);

uint16_t state[16] = {0}; // 32 byte state
state[TPS_STATUS] = NOT_OK;
state[DSMS_ON] = OFF;
state[TSACTIVATION] = OFF;
state[DSB_STATE] = OFF;
state[EBS_ACTIVATED] = OFF;
state[STARTUP_CHECK] = NOT_DONE;
state[READY_TO_DRIVE] = NO;
state[BRAKES_ENGAGED] = NO;
state[STANDSTILL] = YES;
state[SDC_OPEN_RSS] = YES;

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
    // Read DSMS_On and TSActivation
    GPIO_Pinstate ts = HAL_GPIO_ReadPin(FROM_TSActivation_GPIO_Port, FROM_TSActivation_Pin);
    if (ts == GPIO_PIN_SET)
    {
      set_state_at(TSACTIVATION, ON);
    }
    else
    {
      set_state_at(TSACTIVATION, OFF);
    }

    GPIO_Pinstate dsms = HAL_GPIO_ReadPin(GPIOB, DSMS_ON_Pin);
    if (dsms == GPIO_PIN_SET)
    {
      set_state_at(DSMS_ON, ON);
    }
    else
    {
      set_state_at(DSMS_ON, OFF);
    }

    osDelay(10); // Every 10 ms -> 100Hz
  }
}

// Define CAN callbacks here

// End CAN callbacks