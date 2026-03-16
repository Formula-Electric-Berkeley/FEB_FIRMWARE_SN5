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

/**
 * Assemble and transmit TPS voltage and current measurements over CAN.
 *
 * Packs raw TPS bus voltages and currents into three CAN frames and sends them using the FEB CAN transmit API:
 * - Voltages frame (ID: FEB_CAN_LVPDB_LV_24V_BUS_AND_12V_BUS_VOLTAGES_FRAME_ID): 4-byte payload containing
 *   tps_bus_voltage_raw[0] and tps_bus_voltage_raw[3] as 16-bit values.
 * - LV/SH/LT/BM_L currents frame (ID: FEB_CAN_LVPDB_LV_SH_LT_BM_L_CURRENTS_FRAME_ID): 8-byte payload containing
 *   tps_current_raw[0..3] as four 16-bit values.
 * - SM/AF1_AF2/CP_RF currents frame (ID: FEB_CAN_LVPDB_SM_AF1_AF2_CP_RF_CURRENTS_FRAME_ID): 8-byte payload containing
 *   tps_current_raw[4..6] as three 16-bit values (fourth slot left zero).
 *
 * @param tps_current_raw Array of raw current measurements in the order: LV, SH, LT, BM_L, SM, AF1_AF2, CP_RF.
 *                        Values are treated as 16-bit signed integers and placed into CAN payloads as little-endian
 * 16-bit words.
 * @param tps_bus_voltage_raw Array of raw bus voltage measurements; expected to contain at least elements 0 and 3
 *                            which are placed into the voltages CAN frame as 16-bit words.
 * @param length Number of elements available in the provided arrays. This parameter is currently not used by the
 * implementation.
 */

void FEB_CAN_TPS_Tick(int16_t *tps_current_raw, uint16_t *tps_bus_voltage_raw, size_t length)
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
  memcpy(&tx_data[0], &tps_current_raw[4], sizeof(uint16_t));
  memcpy(&tx_data[2], &tps_current_raw[5], sizeof(uint16_t));
  memcpy(&tx_data[4], &tps_current_raw[6], sizeof(uint16_t));
  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_LVPDB_SM_AF1_AF2_CP_RF_CURRENTS_FRAME_ID, FEB_CAN_ID_STD, tx_data,
                  8); // Currents for SM, AF1_AF2, CP_RF
}

// CAN Transaction Data
//
// byte             byte             byte             byte
// 1010101010101010 1010101010101010 1010101010101010 1010101010101010
// 1111111111111111 1111111111111111 0000000000000000 0000000000000000
// uint16_t                          uint16_t
