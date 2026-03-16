#include "FEB_CAN_TPS.h"
#include "feb_uart_log.h"

/* TPS2482 Configuration */
#define TPS_MAX_CURRENT_A 4.0f         /* Maximum current in Amps (based on 4A fuse rating) */
#define TPS_SHUNT_RESISTOR_OHMS 0.012f /* 12 milliohm shunt resistor */

/* Global TPS message data */
TPS_MESSAGE_TYPE TPS_MESSAGE;

/* Device handle */
static FEB_TPS_Handle_t pcu_tps_handle = NULL;

static void tps_log_callback(FEB_TPS_LogLevel_t level, const char *msg)
{
  switch (level)
  {
  case FEB_TPS_LOG_ERROR:
    LOG_E(TAG_TPS, "%s", msg);
    break;
  case FEB_TPS_LOG_WARN:
    LOG_W(TAG_TPS, "%s", msg);
    break;
  case FEB_TPS_LOG_INFO:
    LOG_I(TAG_TPS, "%s", msg);
    break;
  case FEB_TPS_LOG_DEBUG:
    LOG_D(TAG_TPS, "%s", msg);
    break;
  default:
    break;
  }
}

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
  /* Initialize library and register device on first call */
  if (pcu_tps_handle == NULL)
  {
    FEB_TPS_LibConfig_t lib_cfg = {
        .log_func = tps_log_callback,
        .log_level = FEB_TPS_LOG_INFO,
    };
    FEB_TPS_Init(&lib_cfg);

    FEB_TPS_DeviceConfig_t cfg = {
        .hi2c = hi2c,
        .i2c_addr = i2c_addresses[0],
        .r_shunt_ohms = TPS_SHUNT_RESISTOR_OHMS,
        .i_max_amps = TPS_MAX_CURRENT_A,
        .config_reg = FEB_TPS_CONFIG_DEFAULT,
        .name = "PCU",
    };

    FEB_TPS_Status_t status = FEB_TPS_DeviceRegister(&cfg, &pcu_tps_handle);
    if (status != FEB_TPS_OK)
    {
      LOG_E(TAG_TPS, "TPS init failed: %s", FEB_TPS_StatusToString(status));
      return;
    }
  }

  /* Poll using the new library */
  FEB_TPS_MeasurementScaled_t scaled;
  FEB_TPS_Status_t status = FEB_TPS_PollScaled(pcu_tps_handle, &scaled);

  if (status != FEB_TPS_OK)
  {
    LOG_E(TAG_TPS, "TPS poll failed: %s", FEB_TPS_StatusToString(status));
    return;
  }

  /* Update message structure */
  TPS_MESSAGE.bus_voltage_mv = scaled.bus_voltage_mv;
  TPS_MESSAGE.current_ma = scaled.current_ma;
  TPS_MESSAGE.shunt_voltage_uv = scaled.shunt_voltage_uv;

  LOG_D(TAG_TPS, "TPS update: Voltage=%d mV, Current=%d mA", TPS_MESSAGE.bus_voltage_mv, TPS_MESSAGE.current_ma);
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
