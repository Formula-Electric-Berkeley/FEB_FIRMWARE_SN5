/**
 * @file FEB_Task_BMSProcessing.h
 * @brief BMS processing task entry point.
 * @author Formula Electric @ Berkeley
 */

#ifndef FEB_TASK_BMS_PROCESSING_H
#define FEB_TASK_BMS_PROCESSING_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief BMS processing task entry point.
   *
   * Consumes snapshots from g_adbms, updates g_bms_pack, and runs
   * fault detection + balancing logic. Registered with FreeRTOS as
   * StartBMSProcessingTask.
   */
  void StartBMSProcessingTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* FEB_TASK_BMS_PROCESSING_H */
