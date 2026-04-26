/**
 ******************************************************************************
 * @file           : DCU_CAN.c
 * @brief          : CAN initialization for DCU with accept-all filter
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_CAN.h"
#include "can.h"
#include "feb_can_lib.h"
#include "feb_log.h"
#include <stdbool.h>

static bool g_can_initialized = false;

bool DCU_CAN_Init(void)
{
  /* Reset flag in case of re-initialization */
  g_can_initialized = false;

  /* Initialize FEB CAN library */
  FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
      .hcan2 = &hcan2,
      .get_tick_ms = HAL_GetTick,
  };

  FEB_CAN_Status_t status = FEB_CAN_Init(&cfg);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "CAN init failed: %s", FEB_CAN_StatusToString(status));
    return false;
  }

  /* Configure accept-all filter on CAN1 FIFO0 (filter bank 0) */
  status = FEB_CAN_Filter_AcceptAll(FEB_CAN_INSTANCE_1, 0, FEB_CAN_FIFO_0);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "CAN1 filter failed: %s", FEB_CAN_StatusToString(status));
    return false;
  }

  /* Configure accept-all filter on CAN2 FIFO0 (filter bank 14) */
  status = FEB_CAN_Filter_AcceptAll(FEB_CAN_INSTANCE_2, 14, FEB_CAN_FIFO_0);
  if (status != FEB_CAN_OK)
  {
    LOG_W(TAG_CAN, "CAN2 filter failed: %s", FEB_CAN_StatusToString(status));
    /* Continue - CAN2 failure is not fatal */
  }

  g_can_initialized = true;
  LOG_I(TAG_CAN, "CAN initialized (accept-all mode)");
  return true;
}

bool DCU_CAN_IsInitialized(void)
{
  return g_can_initialized;
}
