/**
 ******************************************************************************
 * @file           : FEB_Task_TPS.c
 * @brief          : TPS2482 power monitoring task
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_Task_TPS.h"
#include "TPS2482.h"
#include "i2c.h"
#include "cmsis_os.h"
#include "feb_uart_log.h"
#include <stdbool.h>

#define TAG_TPS "[TPS]"

/* TPS2482 configuration */
#define BMS_TPS_ADDR TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_GND)
#define BMS_TPS_R_SHUNT 0.002 /* 2 mOhm shunt resistor (WSR52L000FEA) */
#define BMS_TPS_I_MAX 5.0     /* 5A fuse max */
#define BMS_TPS_CURRENT_LSB TPS2482_CURRENT_LSB_EQ(BMS_TPS_I_MAX)
#define BMS_TPS_CAL TPS2482_CAL_EQ(BMS_TPS_CURRENT_LSB, BMS_TPS_R_SHUNT)

/* ===== StartTPSTask =====
   Low-priority task for TPS2482 power monitoring
   - Monitors LV bus voltage and current
   - Single TPS2482 with A0=GND, A1=GND */
void StartTPSTask(void *argument)
{
  (void)argument;

  uint8_t addr = BMS_TPS_ADDR;
  TPS2482_Configuration config = {.config = TPS2482_CONFIG_DEFAULT, .cal = BMS_TPS_CAL, .mask = 0, .alert_lim = 0};
  uint16_t id = 0;
  bool init_result = false;

  LOG_I(TAG_TPS, "Initializing TPS2482 at address 0x%02X", addr);

  TPS2482_Init(&hi2c1, &addr, &config, &id, &init_result, 1);

  if (!init_result)
  {
    LOG_W(TAG_TPS, "TPS2482 initialization failed");
  }
  else
  {
    LOG_I(TAG_TPS, "TPS2482 initialized, ID: 0x%04X", id);
  }

  for (;;)
  {
    uint16_t current_raw = 0;
    uint16_t voltage_raw = 0;

    TPS2482_Poll_Current(&hi2c1, &addr, &current_raw, 1);
    TPS2482_Poll_Bus_Voltage(&hi2c1, &addr, &voltage_raw, 1);

    /* Convert to physical units */
    /* Current: raw * current_LSB (in Amps) */
    /* Voltage: raw * 1.25mV/LSB = raw * 0.00125 V */
    (void)((float)((int16_t)current_raw) * BMS_TPS_CURRENT_LSB);
    (void)((float)voltage_raw * TPS2482_CONV_VBUS);
    // LOG_D(TAG_TPS, "V=%.2fV I=%.3fA", voltage_V, current_A);

    osDelay(pdMS_TO_TICKS(1000)); /* 1 Hz polling */
  }
}
