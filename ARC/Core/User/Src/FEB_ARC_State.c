#include "FEB_ARC_State.h"
#include "FEB_P_Sense.h"
#include "FEB_FEET_Control.h"
#include "FEB_Main.h"
#include "main.h"
#include "feb_log.h"
#include "feb_can_lib.h"
#include "feb_can_db.h"
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
state[DRIVERLESS_STATUS] = OFF;

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

  ARC_CAN_Init();

  // TODO: Define heartbeat here

  // TODO: Define can listeners here

  // DSB_STATE (Get from P_sense or CAN) this one is prob CAN
  // EBS_ACTIVATED (Get from P_sense or CAN) this one is prob p_sense
  // STARTUP_CHECK (Get from P_sense?)

  // READY_TO_DRIVE (Get from DASH probably)
  FEB_CAN_RX_Params_t dash_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_DASH_STATE_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_DASH_Callback,
      .user_data = NULL,
  };
  FEB_CAN_RX_Register(&dash_params);

  // BRAKES_ENGAGED (CAN probably?)
  // STANDSTILL (Wheelspeed Callback?)
  // SDC_OPEN_RSS (No idea)
  // MISSION (Prob from DASH, check the CAN messages)

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

    // Update DRIVERLESS_STATUS below
    if (get_state_at(EBS_ACTIVATED) == YES)
    {
    }
    else
    {
    }

    osDelay(10); // Every 10 ms -> 100Hz
  }
}

void set_driverless_state(FEET_Status s)
{
  if (s != get_state_at(DRIVERLESS_STATUS))
  {
    LOG_I(TAG_ARC_STATE, "%s %d", "Driverless Status Updated To:", s);
    set_state_at(DRIVERLESS_STATUS, s)
  }
}

// Define CAN callbacks here

// Callback for READY_TO_DRIVE
static void FEB_CAN_DASH_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                  const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;

  if (length < FEB_CAN_DASH_STATE_LENGTH)
  {
    return;
  }

  struct feb_can_dash_state_t msg;
  feb_can_dash_state_unpack(&msg, data, length);

  set_state_at(READY_TO_DRIVE, (uint16_t)msg.ready_to_drive);
}

// End CAN callbacks