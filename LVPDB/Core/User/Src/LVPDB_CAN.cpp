/**
 ******************************************************************************
 * @file           : LVPDB_CAN.cpp
 * @brief          : LVPDB CAN bring-up
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "LVPDB_CAN.h"
#include "LVPDB_PingPong.h"
#include "LVPDB_TPS.h"
#include "cmsis_os2.h"
#include "feb_can_subscriber.hpp"
#include "main.h"

namespace fc = feb::can;
namespace fm = feb::can::msg;

extern CAN_HandleTypeDef hcan1;

#if FEB_CAN_USE_FREERTOS
extern osMessageQueueId_t canTxQueueHandle;
extern osMessageQueueId_t canRxQueueHandle;
extern osMutexId_t canTxMutexHandle;
extern osMutexId_t canRxMutexHandle;
extern osSemaphoreId_t canTxMailboxSemHandle;
#endif

namespace
{
volatile bool can_ready = false;
} // namespace

bool LVPDB_CAN_IsReady(void)
{
  return can_ready;
}

void LVPDB_CAN_Init()
{
  const FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
      .hcan2 = nullptr,
      .get_tick_ms = HAL_GetTick,
#if FEB_CAN_USE_FREERTOS
      .tx_queue = canTxQueueHandle,
      .rx_queue = canRxQueueHandle,
      .tx_mutex = canTxMutexHandle,
      .rx_mutex = canRxMutexHandle,
      .tx_mailbox_sem = canTxMailboxSemHandle,
#endif
  };
  FEB_CAN_Init(&cfg);

  fc::RxRegistry::attach_all();
  FEB_CAN_PingPong_Init();

  can_ready = true;
}

void LVPDB_CAN_ApplyRxState(void)
{
  // Rails fall back to off when DASH goes quiet.
  const bool dash_fresh = fc::rx<fm::DashState>.fresh();
  const auto dash_state = fc::rx<fm::DashState>.snapshot();

  // Rail indices: LV, SH, LT, BM_L, SM, AF1_AF2, CP_RF
  LVPDB_TPS_SetRail(5, dash_fresh && dash_state.switch1); // AF1_AF2
  LVPDB_TPS_SetRail(6, dash_fresh && dash_state.switch2); // CP_RF

  // LVPDB_TPS_SetRail(3, true); // BM_L

  /* brake_position is centi-percent (0-10000); compare whole percent (0-100). */
  bool brake_on = fc::rx<fm::Brake>.fresh() && (fc::rx<fm::Brake>.v().brake_position > 1000u);
  HAL_GPIO_WritePin(BL_Switch_GPIO_Port, BL_Switch_Pin, brake_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
