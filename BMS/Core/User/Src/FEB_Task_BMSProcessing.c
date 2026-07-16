/**
 * @file FEB_Task_BMSProcessing.c
 * @brief BMS processing task.
 *
 * Runs at 20 Hz. Consumes raw register data from g_adbms (seqlock-based
 * lock-free reads), updates g_bms_pack, runs fault detection, and
 * handles mode transitions driven by the state machine. Staging of
 * balancing writes / config writes is deferred to the acquisition task
 * via the pending-writes bitmask.
 */

#include "FEB_Task_BMSProcessing.h"
#include "FEB_BMS_Processing.h"
#include "FEB_ADBMS_App.h"
#include "FEB_SM.h"
#include "BMS_HW_Config.h"
#include "cmsis_os.h"
#include "feb_log.h"
#include <stdbool.h>

#define TAG_PROC_TASK "[BMS_PROC_TASK]"

#define BMS_PROC_PERIOD_MS 50 /* 20 Hz - processes whatever is fresh */

static BMS_State_t s_prev_state = BMS_STATE_BOOT;

static void _handle_state_transitions(void)
{
  BMS_State_t current = FEB_SM_Get_Current_State();
  if (current == s_prev_state)
    return;

  LOG_I(TAG_PROC_TASK, "State changed: %d -> %d", s_prev_state, current);

  if (s_prev_state == BMS_STATE_BALANCE && current != BMS_STATE_BALANCE)
  {
    BMS_Proc_RequestStopBalancing();
    BMS_App_SetMode(BMS_MODE_NORMAL);
  }
  if (current == BMS_STATE_BALANCE)
  {
    BMS_App_SetMode(BMS_MODE_BALANCING);
    LOG_I(TAG_PROC_TASK, "Entering balancing mode");
  }

  s_prev_state = current;
}

static void _surface_faults_to_sm(void)
{
  uint32_t mask = BMS_App_GetActiveErrorMask();
  if (mask == 0)
    return;

  if (g_bms_pack.voltage_error == BMS_APP_ERR_VOLTAGE_UV || g_bms_pack.voltage_error == BMS_APP_ERR_VOLTAGE_OV)
  {
    FEB_SM_Fault(BMS_STATE_FAULT_BMS);
  }
  if (g_bms_pack.temp_error == BMS_APP_ERR_TEMP_HIGH || g_bms_pack.temp_error == BMS_APP_ERR_TEMP_LOW)
  {
    FEB_SM_Fault(BMS_STATE_FAULT_BMS);
  }
}

void StartBMSProcessingTask(void *argument)
{
  (void)argument;

  LOG_I(TAG_PROC_TASK, "Processing task starting");

  /* Wait for acquisition task to complete init + priming. */
  while (!BMS_App_IsInitialized())
  {
    osDelay(pdMS_TO_TICKS(50));
  }

  BMS_Proc_Init();

  LOG_I(TAG_PROC_TASK, "Processing task entering main loop");

  for (;;)
  {
    _handle_state_transitions();

    BMS_Proc_RunFrame();

    _surface_faults_to_sm();

    osDelay(pdMS_TO_TICKS(BMS_PROC_PERIOD_MS));
  }
}
