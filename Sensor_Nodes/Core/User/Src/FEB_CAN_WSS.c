/**
 ******************************************************************************
 * @file           : FEB_CAN_WSS.c
 * @brief          : CAN WSS (Wheel Speed Sensor) Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_WSS.h"
#include "feb_can_lib.h"
#include <stddef.h>
#include <string.h>
#include "feb_can.h"
#include "FEB_WSS.h"

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_WSS_Init(void) {}

/**
 * Assemble and transmit WSS wheel speed measurements over CAN.
 *
 * Packs wheel speed data into one 2-byte CAN frame:
 * - WSS frame (ID: FEB_CAN_WSS_DATA_FRAME_ID):
 *   - left_rpm  → byte 0 (uint8_t, LSB = 2 RPM, max = 510 RPM)
 *   - right_rpm → byte 1 (uint8_t, LSB = 2 RPM, max = 510 RPM)
 */

/* Error counter for throttled error reporting */
static uint32_t can_tx_error_count = 0;

void FEB_CAN_WSS_Tick(void)
{
  uint8_t tx_data[2] = {0};
  memcpy(&tx_data[0], &left_rpm, sizeof(uint8_t));
  memcpy(&tx_data[1], &right_rpm, sizeof(uint8_t));

  FEB_CAN_Status_t status =
      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_WSS_DATA_FRAME_ID, FEB_CAN_ID_STD, tx_data, FEB_CAN_WSS_DATA_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }

  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}
