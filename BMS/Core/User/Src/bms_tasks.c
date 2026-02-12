#include "bms_tasks.h"
#include "main.h"
#include "projdefs.h"
#include "stm32f4xx_hal.h"
#include "FEB_ADBMS6830B.h"
#include "FEB_HW.h"
#include "FEB_SM.h"
#include "FEB_Const.h"
#include "TPS2482.h"
#include "i2c.h"
#include "cmsis_os.h"
#include <stdio.h>

/* ===== StartADBMSTask =====
   High-priority task for ADBMS6830B monitoring and control
   - Initializes isoSPI redundancy system and ADBMS chips
   - Monitors cell voltages every 100ms
   - Monitors cell temperatures every 500ms
   - Performs cell balancing when in BALANCE state */
void StartADBMSTask(void *argument)
{
  const uint8_t MAX_INIT_RETRIES = 5;
  uint8_t init_attempts = 0;
  bool init_success = false;

  printf("[ADBMS_TASK] Task Begun\r\n");

  /* === Initialization Phase === */
  while (init_attempts < MAX_INIT_RETRIES && !init_success)
  {
    init_attempts++;

/* Initialize redundant isoSPI system (if in redundant mode) */
#if (ISOSPI_MODE == ISOSPI_MODE_REDUNDANT)
    FEB_spi_init_redundancy();
#endif

    HAL_GPIO_WritePin(M1_GPIO_Port, M1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(M2_GPIO_Port, M2_Pin, GPIO_PIN_SET);

    /* Initialize ADBMS6830B chips */
    FEB_ADBMS_Init();

    /* TODO: Add validation check here if FEB_ADBMS_Init returns status */
    /* For now, assume initialization succeeded */
    init_success = true;

    if (!init_success && init_attempts < MAX_INIT_RETRIES)
    {
      osDelay(pdMS_TO_TICKS(100)); /* Wait 100ms before retry */
    }
  }

  /* Handle initialization failure */
  if (!init_success)
  {
    printf("[ADBMS_TASK] FATAL: Initialization failed after %d attempts\r\n", MAX_INIT_RETRIES);
    FEB_ADBMS_Update_Error_Type(ERROR_TYPE_INIT_FAILURE);

    /* Signal failure via LED blinking */
    for (;;)
    {
      HAL_GPIO_TogglePin(M2_GPIO_Port, M2_Pin);
      osDelay(pdMS_TO_TICKS(500));
    }
  }

  /* === Main Task Loop === */
  uint32_t voltage_tick = osKernelGetTickCount();
  uint32_t temp_tick = osKernelGetTickCount();
  uint32_t print_tick = osKernelGetTickCount();

  for (;;)
  {
    uint32_t now = osKernelGetTickCount();

    /* Voltage measurement every 100ms (10 Hz) */
    if (now - voltage_tick >= pdMS_TO_TICKS(100))
    {
      osMutexAcquire(ADBMSMutexHandle, osWaitForever);
      FEB_ADBMS_Voltage_Process();
      osMutexRelease(ADBMSMutexHandle);
      voltage_tick = now;
    }

    /* Temperature measurement every 500ms (2 Hz) */
    if (now - temp_tick >= pdMS_TO_TICKS(500))
    {
      osMutexAcquire(ADBMSMutexHandle, osWaitForever);
      FEB_ADBMS_Temperature_Process();
      osMutexRelease(ADBMSMutexHandle);
      temp_tick = now;
    }

    /* Print accumulator struct every 1000ms (1 Hz) */
    if (now - print_tick >= pdMS_TO_TICKS(1000))
    {
      FEB_ADBMS_Print_Accumulator();
      print_tick = now;
    }

    /* Cell balancing (only in BALANCE state) */
    // FEB_SM_State_t current_state = FEB_SM_Get_Current_State();
    // if (current_state == FEB_SM_ST_BALANCING)
    // {
    //   osMutexAcquire(ADBMSMutexHandle, osWaitForever);
    //   FEB_Cell_Balance_Process();
    //   osMutexRelease(ADBMSMutexHandle);
    // }

    /* Task runs at 10ms period (100 Hz) */

    osDelay(pdMS_TO_TICKS(10));
  }
}

/* ===== StartTPSTask =====
   Low-priority task for TPS2482 power monitoring
   - Monitors LV bus voltage and current
   - Single TPS2482 with A0=GND, A1=GND */
#define BMS_TPS_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_GND)
#define BMS_TPS_R_SHUNT 0.002 // 2 mOhm shunt resistor (WSR52L000FEA)
#define BMS_TPS_I_MAX 5.0     // 5A fuse max
#define BMS_TPS_CURRENT_LSB TPS2482_CURRENT_LSB_EQ(BMS_TPS_I_MAX)
#define BMS_TPS_CAL TPS2482_CAL_EQ(BMS_TPS_CURRENT_LSB, BMS_TPS_R_SHUNT)

void StartTPSTask(void *argument)
{
  uint8_t addr = BMS_TPS_ADDR;
  TPS2482_Configuration config = {.config = TPS2482_CONFIG_DEFAULT, .cal = BMS_TPS_CAL, .mask = 0, .alert_lim = 0};
  uint16_t id = 0;
  bool init_result = false;

  printf("[TPS_TASK] Initializing TPS2482 at address 0x%02X\r\n", addr);

  TPS2482_Init(&hi2c1, &addr, &config, &id, &init_result, 1);

  if (!init_result)
  {
    printf("[TPS_TASK] WARNING: TPS2482 initialization failed\r\n");
  }
  else
  {
    printf("[TPS_TASK] TPS2482 initialized, ID: 0x%04X\r\n", id);
  }

  for (;;)
  {
    uint16_t current_raw = 0;
    uint16_t voltage_raw = 0;

    TPS2482_Poll_Current(&hi2c1, &addr, &current_raw, 1);
    TPS2482_Poll_Bus_Voltage(&hi2c1, &addr, &voltage_raw, 1);

    // Convert to physical units
    // Current: raw * current_LSB (in Amps)
    // Voltage: raw * 1.25mV/LSB = raw * 0.00125 V
    float current_A = (float)((int16_t)current_raw) * BMS_TPS_CURRENT_LSB;
    float voltage_V = (float)voltage_raw * TPS2482_CONV_VBUS;

    printf("[TPS] V=%.2fV I=%.3fA\r\n", voltage_V, current_A);

    osDelay(pdMS_TO_TICKS(1000)); // 1 Hz polling
  }
}
