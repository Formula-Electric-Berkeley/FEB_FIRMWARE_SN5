#include "FEB_CAN_TPS.h"
#include "feb_uart_log.h"
#include <stdint.h>

/* TPS2482 Configuration */
#define TPS_MAX_CURRENT_A 4.0f         /* Maximum current in Amps (based on 4A fuse rating) */
#define TPS_SHUNT_RESISTOR_OHMS 0.012f /* 12 milliohm shunt resistor */

/* Global TPS message data */
TPS_MESSAGE_TYPE TPS_MESSAGE;

/* Device handle */
static FEB_TPS_Handle_t pcu_tps_handle = NULL;

/**
 * Route TPS library log messages to the system logger with matching severity.
 *
 * Maps the provided FEB_TPS_LogLevel_t to the corresponding system log call and emits the given message.
 *
 * @param level Log level from the TPS library indicating severity.
 * @param msg   Null-terminated message string to log.
 */
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

/**
 * Initialize TPS CAN subsystem state.
 *
 * Sets the global TPS_MESSAGE fields (bus_voltage_mv, current_ma, shunt_voltage_uv) to zero
 * and logs that the TPS CAN component has been initialized.
 */
void FEB_CAN_TPS_Init(void)
{
  /* Initialize TPS message structure */
  TPS_MESSAGE.bus_voltage_mv = 0;
  TPS_MESSAGE.current_ma = 0;
  TPS_MESSAGE.shunt_voltage_uv = 0;
  TPS_MESSAGE.status_flags = 0;
  LOG_I(TAG_TPS, "TPS CAN initialized");
}

void FEB_CAN_TPS_GetData(FEB_CAN_TPS_Data_t *data)
{
  data->bus_voltage_mv = TPS_MESSAGE.bus_voltage_mv;
  data->current_ma = TPS_MESSAGE.current_ma;
  data->shunt_voltage_uv = TPS_MESSAGE.shunt_voltage_uv;
}

/**
 * Initialize the TPS device on first call, poll scaled measurements, and update the global TPS_MESSAGE.
 *
 * If the TPS device has not been registered, this function registers it using the provided I2C handle
 * and the first address from `i2c_addresses`. It then polls the device for scaled measurements and
 * writes the measured `bus_voltage_mv`, `current_ma`, and `shunt_voltage_uv` into the global
 * `TPS_MESSAGE`. Errors during registration or polling are logged and cause an early return.
 *
 * @param hi2c Pointer to the I2C peripheral handle used to communicate with the TPS device.
 * @param i2c_addresses Array of candidate I2C addresses; the first element is used to register the device.
 * @param num_devices Number of entries in `i2c_addresses`.
 */
void FEB_CAN_TPS_Update(I2C_HandleTypeDef *hi2c, uint8_t *i2c_addresses, uint8_t num_devices)
{
  /* Initialize library and register device on first call */
  if (pcu_tps_handle == NULL)
  {
    FEB_TPS_LibConfig_t lib_cfg = {
        .log_func = tps_log_callback,
        .log_level = FEB_TPS_LOG_INFO,
    };
    FEB_TPS_Status_t init_status = FEB_TPS_Init(&lib_cfg);
    if (init_status != FEB_TPS_OK)
    {
      LOG_E(TAG_TPS, "TPS library init failed: %s", FEB_TPS_StatusToString(init_status));
      return;
    }

    if (num_devices == 0 || i2c_addresses == NULL)
    {
      LOG_E(TAG_TPS, "TPS init failed: no devices specified");
      return;
    }

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

  /* Update message structure with bounds checking */
  TPS_MESSAGE.status_flags = 0;

  /* Bus voltage: uint32 -> uint16 with overflow detection */
  if (scaled.bus_voltage_mv > UINT16_MAX)
  {
    TPS_MESSAGE.bus_voltage_mv = UINT16_MAX;
    TPS_MESSAGE.status_flags |= TPS_STATUS_VOLTAGE_OVERFLOW;
  }
  else
  {
    TPS_MESSAGE.bus_voltage_mv = (uint16_t)scaled.bus_voltage_mv;
  }

  /* Current: int32 -> int16 with overflow detection */
  if (scaled.current_ma > INT16_MAX)
  {
    TPS_MESSAGE.current_ma = INT16_MAX;
    TPS_MESSAGE.status_flags |= TPS_STATUS_CURRENT_OVERFLOW;
  }
  else if (scaled.current_ma < INT16_MIN)
  {
    TPS_MESSAGE.current_ma = INT16_MIN;
    TPS_MESSAGE.status_flags |= TPS_STATUS_CURRENT_OVERFLOW;
  }
  else
  {
    TPS_MESSAGE.current_ma = (int16_t)scaled.current_ma;
  }

  /* Track negative current */
  if (scaled.current_ma < 0)
  {
    TPS_MESSAGE.status_flags |= TPS_STATUS_CURRENT_NEGATIVE;
  }

  TPS_MESSAGE.shunt_voltage_uv = scaled.shunt_voltage_uv;

  LOG_D(TAG_TPS, "TPS update: Voltage=%d mV, Current=%d mA, Flags=0x%02X", TPS_MESSAGE.bus_voltage_mv,
        TPS_MESSAGE.current_ma, TPS_MESSAGE.status_flags);
}

/**
 * Pack the latest TPS bus voltage and current into a CAN payload and transmit it.
 *
 * The function places bus voltage (millivolts) into bytes 0-1 and current (milliamps) into bytes 2-3
 * of an 8-byte CAN payload, then sends a 4-byte CAN frame on the PCU TPS CAN ID.
 * On transmission failure an error is logged; on success a debug message is logged.
 */
void FEB_CAN_TPS_Transmit(void)
{
  uint8_t data[8] = {0};

  /* Pack voltage (bytes 0-1) and current (bytes 2-3)
   * Note: Uses host byte order (little-endian on ARM Cortex-M).
   * Receivers must expect little-endian format. */
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
