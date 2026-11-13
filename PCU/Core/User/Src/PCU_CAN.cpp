/**
 ******************************************************************************
 * @file           : PCU_CAN.cpp
 * @brief          : PCU CAN bring-up and the RX / TX tasks
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "PCU_CAN.hpp"
#include "feb_bms_state.h"
#include "feb_can_subscriber.hpp"
#include "main.h"

extern CAN_HandleTypeDef hcan1;
namespace fc = feb::can;
namespace fm = feb::can::msg;

void PCU_CAN_Init()
{
  const FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
      .hcan2 = nullptr,
      .get_tick_ms = HAL_GetTick,
  };

  if (FEB_CAN_Init(&cfg) != FEB_CAN_OK)
  {
    Error_Handler();
  }
  fc::RxRegistry::attach_all();
}

bool PCU_CAN_IsR2D()
{
  const auto &bms = fc::rx<fm::BmsState>;
  return bms.fresh() && bms.v().bms_state == BMS_STATE_DRIVE;
}
