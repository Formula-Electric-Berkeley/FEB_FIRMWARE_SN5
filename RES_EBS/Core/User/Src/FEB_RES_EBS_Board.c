#include "FEB_RES_EBS_Board.h"

#include "i2c.h"
#include "main.h"
#include "feb_uart_log.h"

#include <string.h>

#define RES_EBS_TPS_I2C_ADDRESS ((uint8_t)0x40U)
#define RES_EBS_TPS_REG_CONFIG ((uint8_t)0x00U)
#define RES_EBS_TPS_REG_SHUNT_VOLT ((uint8_t)0x01U)
#define RES_EBS_TPS_REG_BUS_VOLT ((uint8_t)0x02U)
#define RES_EBS_TPS_REG_CURRENT ((uint8_t)0x04U)
#define RES_EBS_TPS_REG_CAL ((uint8_t)0x05U)
#define RES_EBS_TPS_REG_MASK ((uint8_t)0x06U)
#define RES_EBS_TPS_REG_ALERT_LIMIT ((uint8_t)0x07U)
#define RES_EBS_TPS_REG_ID ((uint8_t)0xFFU)

#define RES_EBS_TPS_CONFIG_DEFAULT ((uint16_t)0x4127U)
#define RES_EBS_TPS_CAL_VALUE ((uint16_t)3495U)
#define RES_EBS_TPS_MASK_VALUE ((uint16_t)0x0000U)
#define RES_EBS_TPS_ALERT_LIMIT_VALUE ((uint16_t)0x0000U)

#define RES_EBS_TPS_CONV_VSHUNT_MV ((float)0.0025f)
#define RES_EBS_TPS_CONV_VBUS_V ((float)0.00125f)
#define RES_EBS_TPS_MAX_CURRENT_A ((float)4.0f)
#define RES_EBS_TPS_I2C_TIMEOUT_MS ((uint32_t)100U)

#define RES_EBS_TPS_MASK_SOL ((uint16_t)(0x0001U << 15))
#define RES_EBS_TPS_MASK_SUL ((uint16_t)(0x0001U << 14))
#define RES_EBS_TPS_MASK_BOL ((uint16_t)(0x0001U << 13))
#define RES_EBS_TPS_MASK_BUL ((uint16_t)(0x0001U << 12))
#define RES_EBS_TPS_MASK_POL ((uint16_t)(0x0001U << 11))
#define RES_EBS_TPS_MASK_CNVR ((uint16_t)(0x0001U << 10))

#define RES_EBS_TPS_VALID_MASK_BITS                                                                       \
  ((uint16_t)(RES_EBS_TPS_MASK_SOL | RES_EBS_TPS_MASK_SUL | RES_EBS_TPS_MASK_BOL | RES_EBS_TPS_MASK_BUL | \
              RES_EBS_TPS_MASK_POL | RES_EBS_TPS_MASK_CNVR))

#define SIGN_MAGNITUDE_TO_INT16(n) ((int16_t)(((((n) >> 15) & 0x01U) == 1U) ? -((n) & 0x7FFFU) : ((n) & 0x7FFFU)))

typedef struct
{
  bool initialized;
  uint8_t i2c_address;
  uint16_t device_id;
} RES_EBS_TPS_Runtime_t;

static RES_EBS_TPS_Runtime_t tps_runtime = {
    .initialized = false,
    .i2c_address = RES_EBS_TPS_I2C_ADDRESS,
    .device_id = 0U,
};

static HAL_StatusTypeDef tps_write_register(uint8_t reg, uint16_t value)
{
  uint8_t tx[2];

  tx[0] = (uint8_t)((value >> 8) & 0xFFU);
  tx[1] = (uint8_t)(value & 0xFFU);

  return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(tps_runtime.i2c_address << 1), reg, I2C_MEMADD_SIZE_8BIT, tx,
                           sizeof(tx), RES_EBS_TPS_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef tps_read_register(uint8_t reg, uint16_t *value)
{
  uint8_t rx[2];
  HAL_StatusTypeDef status;

  if (value == NULL)
  {
    return HAL_ERROR;
  }

  status = HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(tps_runtime.i2c_address << 1), reg, I2C_MEMADD_SIZE_8BIT, rx,
                            sizeof(rx), RES_EBS_TPS_I2C_TIMEOUT_MS);
  if (status == HAL_OK)
  {
    *value = (uint16_t)(((uint16_t)rx[0] << 8) | (uint16_t)rx[1]);
  }

  return status;
}

static void tps_fill_gpio_status(RES_EBS_TPS_Status_t *status)
{
  status->power_good = HAL_GPIO_ReadPin(TPS_PG_GPIO_Port, TPS_PG_Pin);
  status->alert = HAL_GPIO_ReadPin(TPS_Alert_GPIO_Port, TPS_Alert_Pin);
}

void RES_EBS_TPS_GetPinStates(GPIO_PinState *power_good, GPIO_PinState *alert)
{
  if (power_good != NULL)
  {
    *power_good = HAL_GPIO_ReadPin(TPS_PG_GPIO_Port, TPS_PG_Pin);
  }

  if (alert != NULL)
  {
    *alert = HAL_GPIO_ReadPin(TPS_Alert_GPIO_Port, TPS_Alert_Pin);
  }
}

void RES_EBS_Board_Init(void)
{
  RES_EBS_Relay_Set(false);

  if (!RES_EBS_TPS_Init())
  {
    LOG_W(TAG_TPS, "TPS init failed; use tps|init to retry");
  }
}

bool RES_EBS_TPS_Init(void)
{
  uint16_t config_readback = 0U;
  uint16_t cal_readback = 0U;
  uint16_t mask_readback = 0U;
  uint16_t alert_limit_readback = 0U;
  uint16_t device_id = 0U;
  HAL_StatusTypeDef status = HAL_OK;

  tps_runtime.initialized = false;

  status = tps_write_register(RES_EBS_TPS_REG_CONFIG, RES_EBS_TPS_CONFIG_DEFAULT);
  if (status == HAL_OK)
  {
    status = tps_write_register(RES_EBS_TPS_REG_CAL, RES_EBS_TPS_CAL_VALUE);
  }
  if (status == HAL_OK)
  {
    status = tps_write_register(RES_EBS_TPS_REG_MASK, RES_EBS_TPS_MASK_VALUE);
  }
  if (status == HAL_OK)
  {
    status = tps_write_register(RES_EBS_TPS_REG_ALERT_LIMIT, RES_EBS_TPS_ALERT_LIMIT_VALUE);
  }

  if (status != HAL_OK)
  {
    LOG_E(TAG_TPS, "TPS register write failed: %d", (int)status);
    return false;
  }

  HAL_Delay(10U);

  status = tps_read_register(RES_EBS_TPS_REG_CONFIG, &config_readback);
  if (status == HAL_OK)
  {
    status = tps_read_register(RES_EBS_TPS_REG_CAL, &cal_readback);
  }
  if (status == HAL_OK)
  {
    status = tps_read_register(RES_EBS_TPS_REG_MASK, &mask_readback);
  }
  if (status == HAL_OK)
  {
    status = tps_read_register(RES_EBS_TPS_REG_ALERT_LIMIT, &alert_limit_readback);
  }
  if (status == HAL_OK)
  {
    status = tps_read_register(RES_EBS_TPS_REG_ID, &device_id);
  }

  if (status != HAL_OK)
  {
    LOG_E(TAG_TPS, "TPS readback failed: %d", (int)status);
    return false;
  }

  if (config_readback != RES_EBS_TPS_CONFIG_DEFAULT || cal_readback != RES_EBS_TPS_CAL_VALUE ||
      (mask_readback & RES_EBS_TPS_VALID_MASK_BITS) != RES_EBS_TPS_MASK_VALUE ||
      alert_limit_readback != RES_EBS_TPS_ALERT_LIMIT_VALUE)
  {
    LOG_E(TAG_TPS, "TPS readback mismatch cfg=0x%04X cal=0x%04X mask=0x%04X alert=0x%04X", config_readback,
          cal_readback, mask_readback, alert_limit_readback);
    return false;
  }

  tps_runtime.device_id = device_id;
  tps_runtime.initialized = true;

  LOG_I(TAG_TPS, "TPS ready addr=0x%02X id=0x%04X", tps_runtime.i2c_address, tps_runtime.device_id);

  return true;
}

bool RES_EBS_TPS_IsInitialized(void)
{
  return tps_runtime.initialized;
}

bool RES_EBS_TPS_Read(RES_EBS_TPS_Status_t *status)
{
  int16_t shunt_signed;
  int16_t current_signed;

  if (status == NULL || !tps_runtime.initialized)
  {
    return false;
  }

  memset(status, 0, sizeof(*status));
  status->initialized = true;
  status->i2c_address = tps_runtime.i2c_address;
  status->device_id = tps_runtime.device_id;
  status->config = RES_EBS_TPS_CONFIG_DEFAULT;
  status->cal = RES_EBS_TPS_CAL_VALUE;
  status->mask = RES_EBS_TPS_MASK_VALUE;
  status->alert_limit = RES_EBS_TPS_ALERT_LIMIT_VALUE;

  if (tps_read_register(RES_EBS_TPS_REG_SHUNT_VOLT, &status->shunt_voltage_raw) != HAL_OK ||
      tps_read_register(RES_EBS_TPS_REG_BUS_VOLT, &status->bus_voltage_raw) != HAL_OK ||
      tps_read_register(RES_EBS_TPS_REG_CURRENT, &status->current_raw) != HAL_OK)
  {
    LOG_W(TAG_TPS, "TPS measurement read failed");
    return false;
  }

  shunt_signed = SIGN_MAGNITUDE_TO_INT16(status->shunt_voltage_raw);
  current_signed = SIGN_MAGNITUDE_TO_INT16(status->current_raw);

  status->shunt_voltage_mv = (float)shunt_signed * RES_EBS_TPS_CONV_VSHUNT_MV;
  status->bus_voltage_v = (float)status->bus_voltage_raw * RES_EBS_TPS_CONV_VBUS_V;
  status->current_a = ((float)current_signed * RES_EBS_TPS_MAX_CURRENT_A) / 32767.0f;
  tps_fill_gpio_status(status);

  LOG_T(TAG_TPS, "TPS raw shunt=0x%04X bus=0x%04X current=0x%04X", status->shunt_voltage_raw, status->bus_voltage_raw,
        status->current_raw);

  return true;
}

void RES_EBS_Relay_Set(bool enabled)
{
  HAL_GPIO_WritePin(Driverless_System_Relay_GPIO_Port, Driverless_System_Relay_Pin, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
  LOG_T(TAG_GPIO, "Relay %s via %s", enabled ? "ON" : "OFF", RES_EBS_Relay_GetName());
}

bool RES_EBS_Relay_Get(void)
{
  return HAL_GPIO_ReadPin(Driverless_System_Relay_GPIO_Port, Driverless_System_Relay_Pin) == GPIO_PIN_SET;
}

const char *RES_EBS_Relay_GetName(void)
{
  return "Driverless_System_Relay";
}

bool RES_EBS_TSActivation_Get(void)
{
  return HAL_GPIO_ReadPin(TS_Activation_GPIO_Port, TS_Activation_Pin) == GPIO_PIN_SET;
}

const char *RES_EBS_TSActivation_GetName(void)
{
  return "TS_Activation";
}
