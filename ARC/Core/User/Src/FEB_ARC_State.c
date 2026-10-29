#include "FEB_ARC_State.h"
#include "FEB_P_Sense.h"
#include "FEB_FEET_Control.h"
#include "FEB_Main.h"
#include "main.h"
#include "cmsis_os.h"
#include "feb_log.h"
#include "feb_can_lib.h"
#include "feb_can_db.h"
#include <stdint.h>

static void FEB_CAN_DASH_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                  const uint8_t *data, uint8_t length, void *user_data);
static void FEB_CAN_EBS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data);

static uint16_t state[STATE_ITEM_COUNT] = {
    [TPS_STATUS] = ARC_NOT_OK,
    [DSMS_ON] = ARC_OFF,
    [TSACTIVATION] = ARC_OFF,
    [DSB_STATE] = ARC_OFF,
    [EBS_ACTIVATED] = ARC_NO,
    [STARTUP_CHECK] = ARC_NOT_DONE,
    [READY_TO_DRIVE] = ARC_NO,
    [BRAKES_ENGAGED] = ARC_NO,
    [STANDSTILL] = ARC_NO,
    [SDC_OPEN_RSS] = ARC_NO,
    [MISSION] = ARC_MISSION_UNKNOWN,
    [DRIVERLESS_STATUS] = FEET_STATE_OFF,
};

uint16_t *get_state_arr(void)
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
  FEB_DSB_Init();
  // EBS_ACTIVATED (Get from P_sense or CAN) this one is prob p_sense
  FEB_EBS_Init();
  // STARTUP_CHECK (Get from P_sense?)
  FEB_Startup_Init();
  // READY_TO_DRIVE (Get from DASH probably)
  FEB_RToD_Init();
  // BRAKES_ENGAGED (CAN probably?)
  FEB_Brakes_Init();
  // STANDSTILL (Wheelspeed Callback?)
  FEB_Standstill_Init();
  // SDC_OPEN_RSS (No idea)
  FEB_SDC_Init();
  // MISSION (Prob from DASH, check the CAN messages)
  FEB_Mission_Init();

  for (;;)
  {
    // Read DSMS_On and TSActivation
    GPIO_PinState ts = HAL_GPIO_ReadPin(FROM_TSActivation_GPIO_Port, FROM_TSActivation_Pin);
    if (ts == GPIO_PIN_SET)
    {
      set_state_at(TSACTIVATION, ARC_ON);
    }
    else
    {
      set_state_at(TSACTIVATION, ARC_OFF);
    }

    GPIO_PinState dsms = HAL_GPIO_ReadPin(DSMS_ON_GPIO_Port, DSMS_ON_Pin);
    if (dsms == GPIO_PIN_SET)
    {
      set_state_at(DSMS_ON, ARC_ON);
    }
    else
    {
      set_state_at(DSMS_ON, ARC_OFF);
    }

    // Update DRIVERLESS_STATUS below
    if (get_state_at(EBS_ACTIVATED) == ARC_YES)
    {
    }
    else
    {
    }

    osDelay(10); // Every 10 ms -> 100Hz
  }
}

// Helpers

void FEB_DSB_Init(void) {}

void FEB_EBS_Init(void)
{
  FEB_CAN_RX_Params_t ebs_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_EBS_PRESSURE_STATUS_FRAME_ID,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_EBS_Callback,
      .user_data = NULL,
  };
  if (FEB_CAN_RX_Register(&ebs_params) < 0)
  {
    LOG_E(TAG_ARC_STATE, "%s", "Failed to register EBS CAN receiver");
  }
}

void FEB_Startup_Init(void) {}

void FEB_RToD_Init(void)
{
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
  if (FEB_CAN_RX_Register(&dash_params) < 0)
  {
    LOG_E(TAG_ARC_STATE, "%s", "Failed to register dashboard CAN receiver");
  }
}

void FEB_Brakes_Init(void) {}

void FEB_Standstill_Init(void) {}

void FEB_SDC_Init(void) {}

void FEB_Mission_Init(void) {}

void set_driverless_state(FEET_Status s)
{
  if ((uint16_t)s != get_state_at(DRIVERLESS_STATUS))
  {
    LOG_I(TAG_ARC_STATE, "%s %d", "Driverless Status Updated To:", (int)s);
    set_state_at(DRIVERLESS_STATUS, s);
    update_feet_state();
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

// EBS state Callback
static void FEB_CAN_EBS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)can_id;
  (void)id_type;
  (void)user_data;

  if (length < FEB_CAN_EBS_PRESSURE_STATUS_LENGTH)
  {
    return;
  }
  struct feb_can_ebs_pressure_status_t msg;
  feb_can_ebs_pressure_status_unpack(&msg, data, length);

  // Do whatever here with ebs status for state, still don't know

  // End
}

// End CAN callbacks