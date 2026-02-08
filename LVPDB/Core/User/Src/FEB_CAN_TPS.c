/**
 ******************************************************************************
 * @file           : FEB_CAN_TPS.c
 * @brief          : CAN TPS Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_TPS.h"
#include "feb_can_lib.h"
#include <stddef.h>
#include <string.h>
#include "feb_can.h"

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_TPS_Init(void) {}

// Order: LV, SH, LT, BM_L, SM, AF1_AF2, CP_RF

void FEB_CAN_TPS_Tick(uint16_t *tps_current_raw, uint16_t *tps_bus_voltage_raw, size_t length)
{
  uint8_t tx_data[8] = {0};
  memcpy(&tx_data[0], &tps_bus_voltage_raw[0], sizeof(uint16_t));
  memcpy(&tx_data[2], &tps_bus_voltage_raw[3], sizeof(uint16_t));
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_LVPDB_LV_24V_BUS_AND_12V_BUS_VOLTAGES_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                  4); // Voltages for LV (24V), BM_L (12V)

  memset(tx_data, 0, sizeof(tx_data));
  memcpy(&tx_data[0], &tps_current_raw[0], sizeof(uint16_t));
  memcpy(&tx_data[2], &tps_current_raw[1], sizeof(uint16_t));
  memcpy(&tx_data[4], &tps_current_raw[2], sizeof(uint16_t));
  memcpy(&tx_data[6], &tps_current_raw[3], sizeof(uint16_t));
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_LVPDB_LV_SH_LT_BM_L_CURRENTS_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                  8); // Currents for LV, SH, LT, BM_L

  memset(tx_data, 0, sizeof(tx_data));
  memcpy(&tx_data[0], &tps_bus_voltage_raw[4], sizeof(uint16_t));
  memcpy(&tx_data[2], &tps_bus_voltage_raw[5], sizeof(uint16_t));
  memcpy(&tx_data[4], &tps_bus_voltage_raw[6], sizeof(uint16_t));
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_LVPDB_SM_AF1_AF2_CP_RF_CURRENTS_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                  8); // Currents for SM, AF1_AF2, CP_RF
}

// CAN Transaction Data
//
// byte             byte             byte             byte             byte             byte             byte byte
// 1010101010101010 1010101010101010 1010101010101010 1010101010101010 1010101010101010 1010101010101010
// 1010101010101010 1010101010101010 1111111111111111 1111111111111111 0000000000000000 0000000000000000 uint16_t
// uint16_t
