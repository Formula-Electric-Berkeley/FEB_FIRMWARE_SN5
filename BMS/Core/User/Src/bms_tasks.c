#include "bms_tasks.h"
#include "main.h"
#include "can.h"
#include "stm32f4xx_hal.h"
#include "FEB_ADBMS6830B.h"
#include "FEB_HW.h"
#include "FEB_SM.h"
#include "FEB_Const.h"
#include "cmsis_os.h"
#include <stdio.h>

/* ===== StartADBMSTask =====
   High-priority task for ADBMS6830B monitoring and control
   - Initializes isoSPI redundancy system and ADBMS chips
   - Monitors cell voltages every 100ms
   - Monitors cell temperatures every 500ms
   - Performs cell balancing when in BALANCE state */
void StartADBMSTask(void *argument) {
  const uint8_t MAX_INIT_RETRIES = 5;
  uint8_t init_attempts = 0;
  bool init_success = false;

  /* === Initialization Phase === */
  while (init_attempts < MAX_INIT_RETRIES && !init_success) {
    init_attempts++;

    /* Initialize redundant isoSPI system (if in redundant mode) */
    #if (ISOSPI_MODE == ISOSPI_MODE_REDUNDANT)
      FEB_spi_init_redundancy();
    #endif

    /* Initialize ADBMS6830B chips */
    FEB_ADBMS_Init();

    /* TODO: Add validation check here if FEB_ADBMS_Init returns status */
    /* For now, assume initialization succeeded */
    init_success = true;

    if (!init_success && init_attempts < MAX_INIT_RETRIES) {
      osDelay(100);  /* Wait 100ms before retry */
    }
  }

  /* Handle initialization failure */
  if (!init_success) {
    /* Transition to fault state and halt task */
    FEB_SM_Transition(FEB_SM_ST_FAULT_BMS);

    /* Halt task execution */
    for(;;) {
      osDelay(1000);
    }
  }

  /* === Main Task Loop === */
  uint32_t voltage_tick = osKernelGetTickCount();
  uint32_t temp_tick = osKernelGetTickCount();

  for(;;) {
    uint32_t now = osKernelGetTickCount();

    /* Voltage measurement every 100ms (10 Hz) */
    if (now - voltage_tick >= 100) {
      osMutexAcquire(ADBMSMutexHandle, osWaitForever);
      FEB_ADBMS_Voltage_Process();
      osMutexRelease(ADBMSMutexHandle);
      voltage_tick = now;
    }

    /* Temperature measurement every 500ms (2 Hz) */
    if (now - temp_tick >= 500) {
      osMutexAcquire(ADBMSMutexHandle, osWaitForever);
      FEB_ADBMS_Temperature_Process();
      osMutexRelease(ADBMSMutexHandle);
      temp_tick = now;
    }

    /* Cell balancing (only in BALANCE state) */
    FEB_SM_State_t current_state = FEB_SM_Get_Current_State();
    if (current_state == FEB_SM_ST_BALANCING) {
      osMutexAcquire(ADBMSMutexHandle, osWaitForever);
      FEB_Cell_Balance_Process();
      osMutexRelease(ADBMSMutexHandle);
    }

    /* Task runs at 10ms period (100 Hz) */
    osDelay(10);
  }
}