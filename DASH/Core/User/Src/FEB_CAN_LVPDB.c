/**
 ******************************************************************************
 * @file           : FEB_CAN_LVPDB.c
 * @brief          : CAN TPS Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_LVPDB.h"
#include "feb_can_lib.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "feb_can.h"

/* ============================================================================
 * API Implementation
 * ============================================================================ */

// Order: LV, SH, LT, BM_L, SM, AF1_AF2, CP_RF

static uint16_t lv_temperature = 0;

static void rx_callback_lv_temperature(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                       const uint8_t *data, uint8_t length, void *user_data)
{
  memcpy(&lv_temperature, &data[0], sizeof(uint16_t));
}

uint16_t FEB_CAN_LVPDB_GetLastLVTemperature(void)
{
  return lv_temperature;
}

// CAN Transaction Data
//
// byte             byte             byte             byte
// 1010101010101010 1010101010101010 1010101010101010 1010101010101010
// 1111111111111111 1111111111111111 0000000000000000 0000000000000000
// uint16_t                          uint16_t
