/**
 * @file FEB_Task_ADBMS.c
 * @brief ADBMS acquisition task (SPI owner)
 *
 * Runs the ADBMS register acquisition scheduler. This task is the sole
 * owner of the SPI bus; all control-path writes (balancing, config,
 * mode changes) are staged by other tasks into g_adbms and picked up
 * via the pending-writes bitmask by BMS_Acq_ServiceScheduler().
 *
 * Fault handling and data interpretation live in the BMS processing
 * task - this task only performs raw I/O.
 */

#include "FEB_Task_ADBMS.h"
#include "FEB_BMS_Acquisition.h"
#include "FEB_ADBMS_App.h"
#include "FEB_SM.h"
#include "BMS_HW_Config.h"
#include "cmsis_os.h"
#include "feb_log.h"
#include <stdbool.h>

#define TAG_ADBMS "[ADBMS]"

/* Legacy mutex retained for the one-shot init sequence (BMS_App_Init
 * runs synchronous SPI traffic and must serialize against nothing else
 * here, but keeping the acquire/release preserves compatibility with any
 * external boot hook that wants to gate on readiness). */
extern osMutexId_t ADBMSMutexHandle;

/* Cadence at which we wake to service the scheduler. The scheduler runs
 * jobs only when their own period has elapsed, so this only has to be
 * fast enough to honour the tightest cadence (voltage @ 10 Hz). */
#define ADBMS_TASK_TICK_MS 5

void StartADBMSTask(void *argument)
{
  (void)argument;

  LOG_I(TAG_ADBMS, "ADBMS acquisition task starting");
  osDelay(pdMS_TO_TICKS(100));

  BMS_Acq_Init();

  /* One-shot driver init + config. Done under the mutex so nothing else
   * attempts to interact before ADBMS_Init() / ADBMS_WriteConfig() land. */
  BMS_AppError_t init_err = BMS_APP_ERR_INIT;
  for (uint8_t retry = 0; retry < BMS_INIT_RETRY_COUNT; retry++)
  {
    if (osMutexAcquire(ADBMSMutexHandle, osWaitForever) == osOK)
    {
      init_err = BMS_App_Init();
      osMutexRelease(ADBMSMutexHandle);

      if (init_err == BMS_APP_OK)
      {
        LOG_I(TAG_ADBMS, "Initialization successful on attempt %d", retry + 1);
        break;
      }
    }
    LOG_W(TAG_ADBMS, "Init attempt %d failed, retrying...", retry + 1);
    osDelay(pdMS_TO_TICKS(BMS_INIT_RETRY_DELAY_MS));
  }

  if (init_err != BMS_APP_OK)
  {
    LOG_E(TAG_ADBMS, "Initialization failed after %d attempts", BMS_INIT_RETRY_COUNT);
    FEB_SM_Fault(BMS_STATE_FAULT_BMS);
    for (;;)
      osDelay(pdMS_TO_TICKS(1000));
  }

  /* Prime the register mirror with one synchronous pass of each
   * essential job, so the processing task has data on its first frame. */
  LOG_I(TAG_ADBMS, "Priming register mirror...");
  (void)BMS_Acq_RunJobNow(BMS_ACQ_JOB_CELL_VOLTAGES);
  (void)BMS_Acq_RunJobNow(BMS_ACQ_JOB_AUX_SCAN);
  (void)BMS_Acq_RunJobNow(BMS_ACQ_JOB_STATUS);
  LOG_I(TAG_ADBMS, "Prime complete - entering scheduler loop");

  for (;;)
  {
    BMS_Acq_ServiceScheduler();

    /* Consecutive-PEC surveillance is a hard-fault signal; surface it to
     * the state machine. Fault semantics remain in processing task for
     * logical faults (UV/OV/temp); this one catches total bus failure. */
    if (BMS_Acq_GetConsecutivePECErrors() >= BMS_PEC_ERROR_THRESHOLD)
    {
      LOG_E(TAG_ADBMS, "Consecutive PEC failures >= %d - declaring comm fault", BMS_PEC_ERROR_THRESHOLD);
      FEB_SM_Fault(BMS_STATE_FAULT_BMS);
      BMS_Acq_ResetConsecutivePECErrors();
    }

    osDelay(pdMS_TO_TICKS(ADBMS_TASK_TICK_MS));
  }
}
