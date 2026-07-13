/**
 * @file FEB_CAN_IVT.c
 * @brief IVT (Isabellenhutte) Current/Voltage sensor CAN reception for the PCU
 * @author Formula Electric @ Berkeley
 *
 * Decodes the IVT-S broadcast on CAN1 using the generated feb_can_ivt_*_unpack()
 * functions, exactly like the other PCU CAN packages. The measured pack voltage
 * and current feed the RMS torque/power limiter (FEB_RMS.c).
 */

#include "FEB_CAN_IVT.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "stm32f4xx_hal.h"
#include "cmsis_compiler.h"
#include <stddef.h>

/* Note: float reads are atomic on ARM Cortex-M4, so no critical section needed. */

typedef struct
{
  volatile float current_mA;
  volatile float voltage_1_mV;
  volatile float voltage_2_mV;
  volatile float voltage_3_mV;
  volatile uint32_t last_rx_tick; /* 0 = never received */
} IVT_Data_t;

static IVT_Data_t ivt_data = {0};

/* ISR-context callback — no logging or blocking work. */
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
    /* Current in mA, negate for reversed direction (matches BMS convention) */
    ivt_data.current_mA = (float)msg.current * (-0.001f) * 1000.0f;
    __DMB();
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

  default:
    break;
  }
}

void FEB_CAN_IVT_Init(void)
{
  ivt_data.current_mA = 0.0f;
  ivt_data.voltage_1_mV = 0.0f;
  ivt_data.voltage_2_mV = 0.0f;
  ivt_data.voltage_3_mV = 0.0f;
  ivt_data.last_rx_tick = 0;

  /* Single MASK registration covering 0x520-0x527 (IVT frames are 0x521-0x525).
   * One hardware filter bank; the callback ignores anything else in range. */
  FEB_CAN_RX_Params_t rx_params = {
      .instance = FEB_CAN_INSTANCE_1,
      .can_id = 0x520,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_MASK,
      .mask = 0x7F8,
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_IVT_Callback,
      .user_data = NULL,
  };
  FEB_CAN_RX_Register(&rx_params);
}

float FEB_CAN_IVT_GetVoltage(void)
{
  if (!FEB_CAN_IVT_IsDataFresh(FEB_CAN_IVT_DATA_TIMEOUT_MS))
  {
    return 0.0f;
  }

#if FEB_CAN_IVT_PACK_VOLTAGE_CHANNEL == 1
  return ivt_data.voltage_1_mV * 0.001f;
#elif FEB_CAN_IVT_PACK_VOLTAGE_CHANNEL == 2
  return ivt_data.voltage_2_mV * 0.001f;
#elif FEB_CAN_IVT_PACK_VOLTAGE_CHANNEL == 3
  return ivt_data.voltage_3_mV * 0.001f;
#else
#error "FEB_CAN_IVT_PACK_VOLTAGE_CHANNEL must be 1, 2, or 3"
#endif
}

float FEB_CAN_IVT_GetCurrent(void)
{
  return ivt_data.current_mA * 0.001f;
}

bool FEB_CAN_IVT_IsDataFresh(uint32_t timeout_ms)
{
  if (ivt_data.last_rx_tick == 0)
  {
    return false;
  }
  return (HAL_GetTick() - ivt_data.last_rx_tick) < timeout_ms;
}
