/**
 ******************************************************************************
 * @file           : DCU_CAN_Logging.c
 * @brief          : CAN Receiving Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "DCU_CAN_Logging.h"
#include "feb_can.h"
#include "feb_can_lib.h"
#include "feb_log.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

// typedef struct
// {
//   volatile uint16_t lv_24v_voltage;
//   volatile uint16_t lv_12v_voltage;
//   volatile uint32_t last_rx_tick;
// } LVPDB_State_t;

// static LVPDB_State_t lvpdb_state = {.lv_24v_voltage = 0, .lv_12v_voltage = 0, .last_rx_tick = 0};

/* ============================================================================
 * RX Callback Handlers
 * ============================================================================ */

static void rx_callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                    const uint8_t *data, uint8_t length, void *user_data)
{
  LOG_D("[CAN]", "[%u] %X %X %X %X %X %X %X %X", can_id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_Logging_Init(void)
{
  const FEB_CAN_Instance_t instances[] = {FEB_CAN_INSTANCE_1, FEB_CAN_INSTANCE_2};
  const FEB_CAN_ID_Type_t id_types[] = {FEB_CAN_ID_STD, FEB_CAN_ID_EXT};

  for (size_t i = 0; i < sizeof(instances) / sizeof(instances[0]); i++)
  {
    for (size_t j = 0; j < sizeof(id_types) / sizeof(id_types[0]); j++)
    {
      FEB_CAN_RX_Params_t rx_params = {
          .instance = instances[i],
          .id_type = id_types[j],
          .filter_type = FEB_CAN_FILTER_WILDCARD,
          .fifo = FEB_CAN_FIFO_0,
          .callback = rx_callback,
          .user_data = NULL,
      };
      FEB_CAN_RX_Register(&rx_params);
    }
  }
}
