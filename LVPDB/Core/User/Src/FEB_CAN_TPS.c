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

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_TPS_Tick(uint16_t *tps_current_raw, uint16_t *tps_bus_voltage_raw, uint16_t *uint_shunt_voltage,
                      size_t length)
{
  uint8_t tx_data[8] = {0};

  // tx_data[0] = (uint8_t)(ch->tx_counter & 0xFF);
  // tx_data[1] = (uint8_t)((ch->tx_counter >> 8) & 0xFF);
  // tx_data[2] = (uint8_t)((ch->tx_counter >> 16) & 0xFF);
  // tx_data[3] = (uint8_t)((ch->tx_counter >> 24) & 0xFF);

  // send LVPDB TPS bus voltage (other TPS will report but we can assume all will be the same))
  // split currents over multiple messages (the other 3)

  FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, 0x01, FEB_CAN_ID_STD, tx_data, 8);
}
