/**
 * @file FEB_Task_ADBMS.c
 * @brief ADBMS6830B monitoring task
 * @author Formula Electric @ Berkeley
 *
 * FreeRTOS task for battery monitoring, temperature sensing, and cell balancing.
 */

#include "FEB_Task_ADBMS.h"
#include "main.h"
#include "FEB_ADBMS_App.h"
#include "FEB_SM.h"
#include "BMS_HW_Config.h"
#include "cmsis_os.h"
#include "feb_log.h"
#include <stdbool.h>

#define TAG_ADBMS "[ADBMS]"

/* External mutex from freertos.c */
extern osMutexId_t ADBMSMutexHandle;

/* Timing state */
static uint32_t s_last_voltage_tick = 0;
static uint32_t s_last_temp_tick = 0;
static uint32_t s_last_balance_tick = 0;

/* Previous state for edge detection */
static BMS_State_t s_prev_state = BMS_STATE_BOOT;

/**
 * @brief Handle error from BMS application layer
 * @param err Error code
 */
static void _handle_error(BMS_AppError_t err)
{
  switch (err)
  {
  case BMS_APP_OK:
    /* No error */
    break;

  case BMS_APP_ERR_COMM:
    LOG_W(TAG_ADBMS, "Communication error (PEC)");
    /* Communication errors are tracked - consecutive errors will fault */
    break;

  case BMS_APP_ERR_VOLTAGE_UV:
    LOG_E(TAG_ADBMS, "Undervoltage fault: Bank %d IC %d Cell %d", g_bms_pack.error_bank, g_bms_pack.error_ic,
          g_bms_pack.error_cell);
    FEB_SM_Fault(BMS_STATE_FAULT_BMS);
    break;

  case BMS_APP_ERR_VOLTAGE_OV:
    LOG_E(TAG_ADBMS, "Overvoltage fault: Bank %d IC %d Cell %d", g_bms_pack.error_bank, g_bms_pack.error_ic,
          g_bms_pack.error_cell);
    FEB_SM_Fault(BMS_STATE_FAULT_BMS);
    break;

  case BMS_APP_ERR_TEMP_HIGH:
    LOG_E(TAG_ADBMS, "Overtemperature fault: %.1fC", g_bms_pack.pack_max_temp_C);
    FEB_SM_Fault(BMS_STATE_FAULT_BMS);
    break;

  case BMS_APP_ERR_TEMP_LOW:
    LOG_E(TAG_ADBMS, "Undertemperature fault: %.1fC", g_bms_pack.pack_min_temp_C);
    FEB_SM_Fault(BMS_STATE_FAULT_BMS);
    break;

  case BMS_APP_ERR_INIT:
    LOG_E(TAG_ADBMS, "Initialization error");
    FEB_SM_Fault(BMS_STATE_FAULT_BMS);
    break;

  case BMS_APP_ERR_REDUNDANCY:
    LOG_W(TAG_ADBMS, "C-ADC vs S-ADC redundancy mismatch");
    /* Log but don't fault immediately */
    break;

  case BMS_APP_ERR_SENSOR:
    LOG_W(TAG_ADBMS, "Sensor fault detected");
    break;

  default:
    LOG_W(TAG_ADBMS, "Unknown error: %d", err);
    break;
  }
}

/* ===== StartADBMSTask =====
   High-priority task for ADBMS6830B monitoring and control
   - Monitors cell voltages every 100ms
   - Monitors cell temperatures every 500ms
   - Performs cell balancing when in BALANCE state */
void StartADBMSTask(void *argument)
{
  (void)argument;

  LOG_I(TAG_ADBMS, "ADBMS Task starting");

  /* Wait for system to stabilize */
  osDelay(pdMS_TO_TICKS(100));

  /* Initialize with retries */
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
    /* Enter fault state */
    FEB_SM_Fault(BMS_STATE_FAULT_BMS);

    /* Stay in error loop */
    for (;;)
    {
      osDelay(pdMS_TO_TICKS(1000));
    }
  }

  /* Perform initial full read */
  if (osMutexAcquire(ADBMSMutexHandle, osWaitForever) == osOK)
  {
    LOG_I(TAG_ADBMS, "Performing initial voltage read...");
    BMS_App_ProcessVoltage();

    LOG_I(TAG_ADBMS, "Performing initial temperature scan...");
    BMS_App_ProcessTemperature();

    LOG_I(TAG_ADBMS, "Initial read complete:");
    LOG_I(TAG_ADBMS, "  Pack Voltage: %.2f V", g_bms_pack.pack_voltage_V);
    LOG_I(TAG_ADBMS, "  Cell Range: %.3f - %.3f V", g_bms_pack.pack_min_cell_V, g_bms_pack.pack_max_cell_V);
    LOG_I(TAG_ADBMS, "  Temp Range: %.1f - %.1f C", g_bms_pack.pack_min_temp_C, g_bms_pack.pack_max_temp_C);

    osMutexRelease(ADBMSMutexHandle);
  }

  /* Initialize timing */
  s_last_voltage_tick = osKernelGetTickCount();
  s_last_temp_tick = s_last_voltage_tick;
  s_last_balance_tick = s_last_voltage_tick;

  /* Main monitoring loop */
  for (;;)
  {
    uint32_t current_tick = osKernelGetTickCount();
    BMS_State_t current_state = FEB_SM_Get_Current_State();
    BMS_AppError_t err = BMS_APP_OK;

    /* Detect state transitions */
    if (current_state != s_prev_state)
    {
      LOG_I(TAG_ADBMS, "State changed: %d -> %d", s_prev_state, current_state);

      /* Handle exit from BALANCE state */
      if (s_prev_state == BMS_STATE_BALANCE && current_state != BMS_STATE_BALANCE)
      {
        if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(50)) == osOK)
        {
          BMS_App_StopBalancing();
          BMS_App_SetMode(BMS_MODE_NORMAL);
          osMutexRelease(ADBMSMutexHandle);
        }
      }

      /* Handle entry to BALANCE state */
      if (current_state == BMS_STATE_BALANCE)
      {
        BMS_App_SetMode(BMS_MODE_BALANCING);
        LOG_I(TAG_ADBMS, "Entering balancing mode");
      }

      s_prev_state = current_state;
    }

    /* Voltage monitoring at 10 Hz */
    if ((current_tick - s_last_voltage_tick) >= pdMS_TO_TICKS(BMS_VOLTAGE_INTERVAL_MS))
    {
      s_last_voltage_tick = current_tick;

      if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(50)) == osOK)
      {
        err = BMS_App_ProcessVoltage();
        osMutexRelease(ADBMSMutexHandle);

        if (err != BMS_APP_OK)
        {
          _handle_error(err);
        }
      }
    }

    /* Temperature monitoring at 2 Hz */
    if ((current_tick - s_last_temp_tick) >= pdMS_TO_TICKS(BMS_TEMP_INTERVAL_MS))
    {
      s_last_temp_tick = current_tick;

      if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(100)) == osOK)
      {
        err = BMS_App_ProcessTemperature();
        osMutexRelease(ADBMSMutexHandle);

        if (err != BMS_APP_OK)
        {
          _handle_error(err);
        }
      }
    }

    /* Balancing at 1 Hz (only in BALANCE state) */
    if (current_state == BMS_STATE_BALANCE)
    {
      if ((current_tick - s_last_balance_tick) >= pdMS_TO_TICKS(BMS_BALANCE_INTERVAL_MS))
      {
        s_last_balance_tick = current_tick;

        if (osMutexAcquire(ADBMSMutexHandle, pdMS_TO_TICKS(100)) == osOK)
        {
          err = BMS_App_ProcessBalancing();
          osMutexRelease(ADBMSMutexHandle);

          if (err != BMS_APP_OK)
          {
            _handle_error(err);
          }

          /* Check if balancing is complete */
          if (!BMS_App_IsBalancingNeeded())
          {
            LOG_I(TAG_ADBMS, "Balancing complete - cells within threshold");
            /* State machine will handle transition */
          }
        }
      }
    }

    /* Sleep until next iteration */
    osDelay(pdMS_TO_TICKS(10));
  }
}
