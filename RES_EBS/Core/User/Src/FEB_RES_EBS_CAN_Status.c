#include "FEB_RES_EBS_CAN_Status.h"

#include "FEB_RES_EBS_BMS.h"
#include "FEB_RES_EBS_Board.h"
#include "FEB_RES_EBS_Safety.h"
#include "FEB_CAN_Library_SN4/gen/feb_can.h"
#include "feb_can_lib.h"
#include "feb_uart_log.h"
#include "stm32f4xx_hal.h"

#define RES_EBS_CAN_STATUS_PERIOD_MS ((uint32_t)100U)

static struct feb_can_res_status_t res_status_msg;
static int32_t res_status_tx_handle = -1;
static uint32_t status_counter_last_ms = 0U;

int RES_EBS_CAN_Status_Init(void)
{
  FEB_CAN_TX_Params_t tx_params;

  if (feb_can_res_status_init(&res_status_msg) != 0)
  {
    LOG_E(TAG_CAN, "Failed to init RES status CAN payload");
    return -1;
  }

  tx_params.instance = FEB_CAN_INSTANCE_1;
  tx_params.can_id = FEB_CAN_RES_STATUS_FRAME_ID;
  tx_params.id_type = FEB_CAN_ID_STD;
  tx_params.data_ptr = &res_status_msg;
  tx_params.data_size = sizeof(res_status_msg);
  tx_params.period_ms = RES_EBS_CAN_STATUS_PERIOD_MS;
  tx_params.pack_func = (int (*)(uint8_t *, const void *, size_t))feb_can_res_status_pack;

  res_status_tx_handle = FEB_CAN_TX_Register(&tx_params);
  if (res_status_tx_handle < 0)
  {
    LOG_E(TAG_CAN, "Failed to register RES status TX slot: %ld", (long)res_status_tx_handle);
    return -1;
  }

  status_counter_last_ms = HAL_GetTick();
  LOG_I(TAG_CAN, "RES status TX registered on 0x%03X every %lu ms", FEB_CAN_RES_STATUS_FRAME_ID,
        (unsigned long)RES_EBS_CAN_STATUS_PERIOD_MS);

  return 0;
}

void RES_EBS_CAN_Status_Tick(void)
{
  RES_EBS_Safety_Status_t safety_status;
  uint32_t now_ms = HAL_GetTick();
  GPIO_PinState tps_power_good = GPIO_PIN_RESET;
  GPIO_PinState tps_alert = GPIO_PIN_RESET;

  RES_EBS_Safety_GetStatus(&safety_status);
  RES_EBS_TPS_GetPinStates(&tps_power_good, &tps_alert);

  res_status_msg.relay_state = RES_EBS_Relay_Get() ? 1U : 0U;
  res_status_msg.ts_activation = safety_status.ts_activation ? 1U : 0U;
  res_status_msg.bms_critical = RES_EBS_BMS_CriticalShutdownActive() ? 1U : 0U;
  res_status_msg.shutdown_open_fault = safety_status.shutdown_open_fault ? 1U : 0U;
  res_status_msg.emergency_latched = safety_status.emergency_latched ? 1U : 0U;
  res_status_msg.relay_enable_allowed =
      (RES_EBS_BMS_RelayEnableAllowed() && safety_status.relay_enable_allowed) ? 1U : 0U;
  res_status_msg.tps_power_good = (tps_power_good == GPIO_PIN_SET) ? 1U : 0U;
  res_status_msg.tps_alert = (tps_alert == GPIO_PIN_SET) ? 1U : 0U;

  while ((now_ms - status_counter_last_ms) >= RES_EBS_CAN_STATUS_PERIOD_MS)
  {
    res_status_msg.status_counter++;
    status_counter_last_ms += RES_EBS_CAN_STATUS_PERIOD_MS;
  }
}
