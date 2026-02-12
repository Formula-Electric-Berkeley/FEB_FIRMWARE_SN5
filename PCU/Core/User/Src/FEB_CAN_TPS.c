#include "FEB_CAN_TPS.h"
#include "feb_uart_log.h"

/* TPS2482 Configuration */
#define TPS_MAX_CURRENT_A 4.0         /* Maximum current in Amps (based on 4A fuse rating) */
#define TPS_SHUNT_RESISTOR_OHMS 0.012 /* 12 milliohm shunt resistor */

/* Global TPS message data */
TPS_MESSAGE_TYPE TPS_MESSAGE;

void FEB_CAN_TPS_Init(void)
{
  /* Initialize TPS message structure */
  TPS_MESSAGE.bus_voltage_mv = 0;
  TPS_MESSAGE.current_ma = 0;
  TPS_MESSAGE.shunt_voltage_uv = 0;
  LOG_I(TAG_TPS, "TPS CAN initialized");
}

void FEB_CAN_TPS_GetData(FEB_CAN_TPS_Data_t *data)
{
  data->bus_voltage_mv = TPS_MESSAGE.bus_voltage_mv;
  data->current_ma = TPS_MESSAGE.current_ma;
  data->shunt_voltage_uv = TPS_MESSAGE.shunt_voltage_uv;
}

void FEB_CAN_TPS_Update(I2C_HandleTypeDef *hi2c, uint8_t *i2c_addresses, uint8_t num_devices)
{
  uint16_t voltage_raw;
  uint16_t current_raw;

  /* Poll TPS2482 for voltage and current */
  TPS2482_Poll_Bus_Voltage(hi2c, i2c_addresses, &voltage_raw, num_devices);
  TPS2482_Poll_Current(hi2c, i2c_addresses, &current_raw, num_devices);

  /* ===== CONVERSION FACTOR DOCUMENTATION =====
   *
   * Voltage Conversion:
   *   - TPS2482_CONV_VBUS = 0.00125 V/LSB (1.25 mV per bit)
   *   - Reference: TPS2482 datasheet, Bus Voltage Register (00h)
   *   - Raw value is multiplied by conversion factor and converted to mV
   *
   * Current Conversion:
   *   - Current LSB depends on maximum expected current and shunt resistor
   *   - Formula: Current_LSB = I_max / 2^15
   *   - I_max = TPS_MAX_CURRENT_A (4A, based on fuse rating)
   *   - This value assumes a specific shunt resistor configuration
   *   - To change: Update TPS_MAX_CURRENT_A #define at top of file
   *   - Reference: TPS2482 datasheet, Calibration Register (05h)
   */
  double voltage_v = voltage_raw * TPS2482_CONV_VBUS;
  double voltage_mv = voltage_v * 1000.0; /* Convert V to mV */

  /* Saturate voltage to uint16_t range to prevent overflow */
  if (voltage_mv > 65535.0)
  {
    TPS_MESSAGE.bus_voltage_mv = 65535;
  }
  else if (voltage_mv < 0.0)
  {
    TPS_MESSAGE.bus_voltage_mv = 0;
  }
  else
  {
    TPS_MESSAGE.bus_voltage_mv = (uint16_t)voltage_mv;
  }

  double current_lsb = TPS2482_CURRENT_LSB_EQ(TPS_MAX_CURRENT_A);
  double current_a = SIGN_MAGNITUDE(current_raw) * current_lsb;
  double current_ma = current_a * 1000.0; /* Convert A to mA */

  /* Saturate current to int16_t range to prevent overflow */
  if (current_ma > 32767.0)
  {
    TPS_MESSAGE.current_ma = 32767;
  }
  else if (current_ma < -32768.0)
  {
    TPS_MESSAGE.current_ma = -32768;
  }
  else
  {
    TPS_MESSAGE.current_ma = (int16_t)current_ma;
  }

  /* Calculate shunt voltage in microvolts */
  TPS_MESSAGE.shunt_voltage_uv = (int32_t)(TPS_MESSAGE.current_ma * TPS_SHUNT_RESISTOR_OHMS * 1000.0);

  LOG_D(TAG_TPS, "TPS update: Voltage=%d mV (%.2fV), Current=%d mA (%.2fA) [raw: V=0x%04X, I=0x%04X]",
        TPS_MESSAGE.bus_voltage_mv, voltage_v, TPS_MESSAGE.current_ma, current_a, voltage_raw, current_raw);
}

void FEB_CAN_TPS_Transmit(void)
{
  uint8_t data[8] = {0};

  /* Pack voltage (bytes 0-1) and current (bytes 2-3) */
  memcpy(&data[0], &TPS_MESSAGE.bus_voltage_mv, sizeof(uint16_t));
  memcpy(&data[2], &TPS_MESSAGE.current_ma, sizeof(int16_t));

  /* Transmit CAN message */
  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_PCU_TPS_FRAME_ID, FEB_CAN_ID_STD, data, 4);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_TPS, "Failed to transmit TPS data: %s", FEB_CAN_StatusToString(status));
  }
  else
  {
    LOG_D(TAG_TPS, "TPS data transmitted: V=%d mV, I=%d mA", TPS_MESSAGE.bus_voltage_mv, TPS_MESSAGE.current_ma);
  }
}
