/**
 * @file FEB_CAN_IVT.c
 * @brief IVT (Isabellenhutte) Current/Voltage Sensor CAN Interface
 * @author Formula Electric @ Berkeley
 *
 * Implements CAN reception for IVT-S sensor using feb_can_lib.
 * The IVT sensor sends raw 32-bit signed integers in big-endian format.
 */

#include "FEB_CAN_IVT.h"
#include "FEB_Const.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "stm32f4xx_hal.h"
#include "cmsis_compiler.h"
#include <stddef.h>

/* Note: Critical sections removed - float reads are atomic on ARM Cortex-M4 */

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
 * @brief IVT CAN RX callback
 *
 * The IVT-S frames are defined in the shared CAN library
 * (common/FEB_CAN_Library_SN4, message defs IVTCurrent / IVTVoltage1-3 /
 * IVTTemperature). Decoding is delegated to the generated unpack functions;
 * the raw int32 value is then scaled into engineering units below.
 */
static void FEB_CAN_IVT_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)user_data;

  switch (can_id)
  {
  case FEB_CAN_IVT_CURRENT_FRAME_ID:
  {
    struct feb_can_ivt_current_t msg;
    if (feb_can_ivt_current_unpack(&msg, data, length) < 0)
    {
      return;
    }
    /* Current in mA, negate for reversed direction */
    ivt_data.current_mA = (float)msg.current * (-0.001f) * 1000.0f;
    __DMB(); /* Memory barrier to ensure data write completes before timestamp */
    ivt_data.last_rx_tick = HAL_GetTick();
    break;
  }

  case FEB_CAN_IVT_VOLTAGE1_FRAME_ID:
  {
    struct feb_can_ivt_voltage1_t msg;
    if (feb_can_ivt_voltage1_unpack(&msg, data, length) < 0)
    {
      return;
    }
    /* Voltage 1 (pack voltage) in mV */
    ivt_data.voltage_1_mV = (float)msg.voltage1;
    __DMB();
    ivt_data.last_rx_tick = HAL_GetTick();
    break;
  }

  case FEB_CAN_IVT_VOLTAGE2_FRAME_ID:
  {
    struct feb_can_ivt_voltage2_t msg;
    if (feb_can_ivt_voltage2_unpack(&msg, data, length) < 0)
    {
      return;
    }
    ivt_data.voltage_2_mV = (float)msg.voltage2;
    __DMB();
    ivt_data.last_rx_tick = HAL_GetTick();
    break;
  }

  case FEB_CAN_IVT_VOLTAGE3_FRAME_ID:
  {
    struct feb_can_ivt_voltage3_t msg;
    if (feb_can_ivt_voltage3_unpack(&msg, data, length) < 0)
    {
      return;
    }
    ivt_data.voltage_3_mV = (float)msg.voltage3;
    __DMB();
    ivt_data.last_rx_tick = HAL_GetTick();
    break;
  }

  case FEB_CAN_IVT_TEMPERATURE_FRAME_ID:
  {
    struct feb_can_ivt_temperature_t msg;
    if (feb_can_ivt_temperature_unpack(&msg, data, length) < 0)
    {
      return;
    }
    /* Temperature in 0.1 degrees C */
    ivt_data.temperature_C = (float)msg.temperature * 0.1f;
    __DMB();
    ivt_data.last_rx_tick = HAL_GetTick();
    break;
  }

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

  /* Single MASK registration covering 0x520-0x527 (IVT frames are
   * 0x521-0x525). One hardware filter bank instead of five — CAN1 only has
   * 14 banks. The callback switches on can_id and ignores anything else in
   * the range. */
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = 0x520,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_MASK,
      .mask = 0x7F8, /* match 0x520-0x527 */
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_IVT_Callback,
      .user_data = NULL,
  };
  FEB_CAN_RX_Register(&rx_params);
}

float FEB_CAN_IVT_GetVoltage(void)
{
  /* Check for stale data */
  if (!FEB_CAN_IVT_IsDataFresh(IVT_DATA_TIMEOUT_MS))
  {
    return 0.0f;
  }

  /* Convert mV to V. Channel is selected at compile time by the board wiring
   * (FEB_IVT_PACK_VOLTAGE_CHANNEL in FEB_Const.h). */
#if FEB_IVT_PACK_VOLTAGE_CHANNEL == 1
  return ivt_data.voltage_1_mV * 0.001f;
#elif FEB_IVT_PACK_VOLTAGE_CHANNEL == 2
  return ivt_data.voltage_2_mV * 0.001f;
#elif FEB_IVT_PACK_VOLTAGE_CHANNEL == 3
  return ivt_data.voltage_3_mV * 0.001f;
#else
#error "FEB_IVT_PACK_VOLTAGE_CHANNEL must be 1, 2, or 3"
#endif
}

float FEB_CAN_IVT_GetCurrent(void)
{
  /* Convert mA to A */
  return ivt_data.current_mA * 0.001f;
}

float FEB_CAN_IVT_GetTemperature(void)
{
  return ivt_data.temperature_C;
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
