/**
 ******************************************************************************
 * @file           : FEB_Task_TPS.c
 * @brief          : TPS2482 power monitoring task
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Task_TPS.h"
#include "i2c.h" /* Must be before feb_tps.h for HAL types */
#include "feb_tps.h"
#include "cmsis_os.h"
#include "feb_uart_log.h"
#include <stdbool.h>

#define TAG_TPS "[TPS]"

/* TPS2482 configuration */
#define BMS_TPS_R_SHUNT 0.002f /* 2 mOhm shunt resistor (WSR52L000FEA) */
#define BMS_TPS_I_MAX 5.0f     /* 5A fuse max */

/* Device handle */
static FEB_TPS_Handle_t bms_tps_handle = NULL;

/* ===== StartTPSTask =====
   Low-priority task for TPS2482 power monitoring
   - Monitors LV bus voltage and current
   - Single TPS2482 with A0=GND, A1=GND */
void StartTPSTask(void *argument)
{
  (void)argument;

  /* Initialize TPS library */
  FEB_TPS_Init(NULL);

  /* Configure and register the TPS2482 device */
  FEB_TPS_DeviceConfig_t cfg = {
      .hi2c = &hi2c1,
      .i2c_addr = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND),
      .r_shunt_ohms = BMS_TPS_R_SHUNT,
      .i_max_amps = BMS_TPS_I_MAX,
      .config_reg = FEB_TPS_CONFIG_DEFAULT,
      .name = "BMS",
  };

  LOG_I(TAG_TPS, "Initializing TPS2482 at address 0x%02X", cfg.i2c_addr);

  FEB_TPS_Status_t status = FEB_TPS_DeviceRegister(&cfg, &bms_tps_handle);

  if (status != FEB_TPS_OK)
  {
    LOG_W(TAG_TPS, "TPS2482 initialization failed: %s", FEB_TPS_StatusToString(status));
  }
  else
  {
    uint16_t id = 0;
    FEB_TPS_ReadID(bms_tps_handle, &id);
    LOG_I(TAG_TPS, "TPS2482 initialized, ID: 0x%04X", id);
  }

  /* Polling loop */
  FEB_TPS_Measurement_t meas;

  for (;;)
  {
    if (bms_tps_handle != NULL)
    {
      status = FEB_TPS_Poll(bms_tps_handle, &meas);

      if (status == FEB_TPS_OK)
      {
        /* Measurement data is now available in meas struct:
         * - meas.bus_voltage_v (float, in Volts)
         * - meas.current_a (float, in Amps, correctly sign-converted)
         * - meas.shunt_voltage_mv (float, in millivolts)
         * - meas.power_w (float, in Watts)
         */
        // LOG_D(TAG_TPS, "V=%.2fV I=%.3fA", meas.bus_voltage_v, meas.current_a);
      }
    }

    osDelay(pdMS_TO_TICKS(1000)); /* 1 Hz polling */
  }
}
