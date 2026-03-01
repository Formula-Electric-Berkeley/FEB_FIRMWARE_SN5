/**
 * @file FEB_CAN_IVT.c
 * @brief IVT (Isabellenhutte) Current/Voltage Sensor CAN Interface
 * @author Formula Electric @ Berkeley
 *
 * Implements CAN reception for IVT-S sensor using feb_can_lib.
 * The IVT sensor sends raw 32-bit signed integers in big-endian format.
 */

#include "FEB_CAN_IVT.h"
#include "feb_can_lib.h"
#include "stm32f4xx_hal.h"
#include "cmsis_compiler.h"
#include <stddef.h>

/* Critical section macros for ISR/task shared data */
#define IVT_ENTER_CRITICAL()                                                                                           \
  uint32_t _primask = __get_PRIMASK();                                                                                 \
  __disable_irq()
#define IVT_EXIT_CRITICAL() __set_PRIMASK(_primask)

/* ============================================================================
 * Constants
 * ============================================================================ */

/* IVT data timeout in milliseconds */
#define IVT_DATA_TIMEOUT_MS 1000

/* ============================================================================
 * Internal State
 * ============================================================================ */

/* IVT data storage */
static FEB_CAN_IVT_Data_t ivt_data = {0};

/* ============================================================================
 * CAN Callback
 * ============================================================================ */

/**
 * @brief Parse IVT CAN message data
 * @param data CAN message data (6 bytes for IVT messages)
 * @return Signed 32-bit value from bytes 2-5 (big-endian)
 *
 * IVT message format:
 * - Bytes 0-1: Message counter/ID
 * - Bytes 2-5: Signed 32-bit value (big-endian)
 */
static int32_t parse_ivt_value(const uint8_t *data)
{
  /* Big-endian to native conversion */
  return ((int32_t)data[2] << 24) | ((int32_t)data[3] << 16) | ((int32_t)data[4] << 8) | ((int32_t)data[5]);
}

/**
 * @brief IVT CAN RX callback
 */
static void FEB_CAN_IVT_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)user_data;

  if (length < 6)
  {
    return; /* IVT messages are 6+ bytes */
  }

  int32_t raw_value = parse_ivt_value(data);

  switch (can_id)
  {
  case FEB_CAN_ID_IVT_CURRENT:
    /* Current in mA, negate for reversed direction */
    ivt_data.current_mA = (float)raw_value * (-0.001f) * 1000.0f;
    __DMB(); /* Memory barrier to ensure data write completes before timestamp */
    ivt_data.last_rx_tick = HAL_GetTick();
    break;

  case FEB_CAN_ID_IVT_VOLTAGE_1:
    /* Voltage 1 (pack voltage) in mV */
    ivt_data.voltage_1_mV = (float)raw_value;
    __DMB();
    ivt_data.last_rx_tick = HAL_GetTick();
    break;

  case FEB_CAN_ID_IVT_VOLTAGE_2:
    ivt_data.voltage_2_mV = (float)raw_value;
    __DMB();
    ivt_data.last_rx_tick = HAL_GetTick();
    break;

  case FEB_CAN_ID_IVT_VOLTAGE_3:
    ivt_data.voltage_3_mV = (float)raw_value;
    __DMB();
    ivt_data.last_rx_tick = HAL_GetTick();
    break;

  case FEB_CAN_ID_IVT_TEMPERATURE:
    /* Temperature in 0.1 degrees C */
    ivt_data.temperature_C = (float)raw_value * 0.1f;
    __DMB();
    ivt_data.last_rx_tick = HAL_GetTick();
    break;

  default:
    break;
  }
}

/* ============================================================================
 * Public Interface
 * ============================================================================ */

void FEB_CAN_IVT_Init(void)
{
  /* Clear IVT data structure */
  ivt_data.current_mA = 0.0f;
  ivt_data.voltage_1_mV = 0.0f;
  ivt_data.voltage_2_mV = 0.0f;
  ivt_data.voltage_3_mV = 0.0f;
  ivt_data.temperature_C = 0.0f;
  ivt_data.last_rx_tick = 0;

  /* Register for IVT current message */
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = FEB_CAN_ID_IVT_CURRENT,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .mask = 0,
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_IVT_Callback,
      .user_data = NULL,
  };
  FEB_CAN_RX_Register(&rx_params);

  /* Register for IVT voltage 1 message */
  rx_params.can_id = FEB_CAN_ID_IVT_VOLTAGE_1;
  FEB_CAN_RX_Register(&rx_params);

  /* Register for IVT voltage 2 message */
  rx_params.can_id = FEB_CAN_ID_IVT_VOLTAGE_2;
  FEB_CAN_RX_Register(&rx_params);

  /* Register for IVT voltage 3 message */
  rx_params.can_id = FEB_CAN_ID_IVT_VOLTAGE_3;
  FEB_CAN_RX_Register(&rx_params);

  /* Register for IVT temperature message */
  rx_params.can_id = FEB_CAN_ID_IVT_TEMPERATURE;
  FEB_CAN_RX_Register(&rx_params);
}

float FEB_CAN_IVT_GetVoltage(void)
{
  /* Check for stale data */
  if (!FEB_CAN_IVT_IsDataFresh(IVT_DATA_TIMEOUT_MS))
  {
    return 0.0f;
  }

  /* Read voltage atomically to prevent torn reads */
  IVT_ENTER_CRITICAL();
  float voltage_mV = ivt_data.voltage_1_mV;
  IVT_EXIT_CRITICAL();

  /* Convert mV to V */
  return voltage_mV * 0.001f;
}

float FEB_CAN_IVT_GetCurrent(void)
{
  /* Read current atomically to prevent torn reads */
  IVT_ENTER_CRITICAL();
  float current_mA = ivt_data.current_mA;
  IVT_EXIT_CRITICAL();

  /* Convert mA to A */
  return current_mA * 0.001f;
}

float FEB_CAN_IVT_GetTemperature(void)
{
  /* Read temperature atomically to prevent torn reads */
  IVT_ENTER_CRITICAL();
  float temp = ivt_data.temperature_C;
  IVT_EXIT_CRITICAL();

  return temp;
}

bool FEB_CAN_IVT_IsDataFresh(uint32_t timeout_ms)
{
  if (ivt_data.last_rx_tick == 0)
  {
    return false; /* Never received */
  }

  uint32_t elapsed = HAL_GetTick() - ivt_data.last_rx_tick;
  return (elapsed < timeout_ms);
}

const FEB_CAN_IVT_Data_t *FEB_CAN_IVT_GetData(void)
{
  return &ivt_data;
}
