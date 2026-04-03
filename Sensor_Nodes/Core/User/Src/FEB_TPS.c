#include "FEB_TPS.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include "FEB_Main.h"

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

static uint8_t tps_address = FEB_TPS_ADDR;
static uint16_t tps_id = 0;
static bool tps_init_res = false;

static uint16_t current_raw = 0;
static uint16_t bus_voltage_raw = 0;
static uint16_t shunt_voltage_raw = 0;

float tps_bus_voltage = 0.0f;
float tps_shunt_voltage = 0.0f;
float tps_current = 0.0f;

static TPS2482_Configuration tps_config = {
    .config = TPS2482_CONFIG_DEFAULT, .cal = FEB_TPS_CAL_VAL, .mask = TPS2482_MASK_SOL, .alert_lim = FEB_TPS_ALERT_LIM};

void i2c1_scan(void)
{
  printf("Scanning I2C1...\r\n");
  for (uint8_t addr = 0x00; addr < 0x80; addr++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK)
    {
      printf("  Device found at 0x%02X\r\n", addr);
    }
  }
  printf("Scan complete.\r\n");
}

void tps_init(void)
{
  TPS2482_Init(&hi2c1, &tps_address, &tps_config, &tps_id, &tps_init_res, 1);
  if (tps_init_res)
  {
    // LOGV("[TPS] Init OK  ID: 0x%04X  ADDR: 0x%02X", tps_id, tps_address);
  }
  else
  {
    // LOGC("[TPS] Init FAIL  ID: 0x%04X  ADDR: 0x%02X", tps_id, tps_address);
  }
}

void read_TPS(void)
{
  TPS2482_Poll_Current(&hi2c1, &tps_address, &current_raw, 1);
  TPS2482_Poll_Bus_Voltage(&hi2c1, &tps_address, &bus_voltage_raw, 1);
  TPS2482_Poll_Shunt_Voltage(&hi2c1, &tps_address, &shunt_voltage_raw, 1);

  // Convert raw to real units
  tps_bus_voltage = bus_voltage_raw * TPS2482_CONV_VBUS;
  tps_shunt_voltage = SIGN_MAGNITUDE(shunt_voltage_raw) * TPS2482_CONV_VSHUNT;
  tps_current = SIGN_MAGNITUDE(current_raw) * FEB_TPS_CURRENT_LSB;

  // LOGV("TPS: Vbus=%4.2fV  Vshunt=%4.3fmV  I=%4.3fA\r\n", tps_bus_voltage, tps_shunt_voltage, tps_current);
}
