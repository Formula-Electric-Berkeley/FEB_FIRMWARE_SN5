/**
 ******************************************************************************
 * @file           : FEB_CAN_Magnetometer.c
 * @brief          : CAN Magnetometer Reporter Module Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_Magnetometer.h"
#include "feb_can_lib.h"
#include <stddef.h>
#include <string.h>
#include "feb_can.h"
#include "FEB_Magnetometer.h"

/* ============================================================================
 * API Implementation
 * ============================================================================ */

void FEB_CAN_Magnetometer_Init(void) {}

/**
 * Assemble and transmit Magnetometer measurements over CAN.
 *
 * Packs raw magnetometer data into two 6-byte CAN frames:
 * - Magnetometer frame (ID: FEB_CAN_MAGNETOMETER_DATA_FRAME_ID):
 *   - mag_raw[0] → bytes 0–1 (X-axis, int16_t)
 *   - mag_raw[1] → bytes 2–3 (Y-axis, int16_t)
 *   - mag_raw[2] → bytes 4–5 (Z-axis, int16_t)
 *
 *
 * @param mag_raw Pointer to data_raw_magnetometer[3] from FEB_Magnetometer.c
 */
/* Error counter for throttled error reporting */
static uint32_t can_tx_error_count = 0;

void FEB_CAN_Magnetometer_Tick(void)
{
  uint8_t tx_data[6] = {0};
  memcpy(&tx_data[0], &data_raw_magnetometer[0], sizeof(int16_t));
  memcpy(&tx_data[2], &data_raw_magnetometer[1], sizeof(int16_t));
  memcpy(&tx_data[4], &data_raw_magnetometer[2], sizeof(int16_t));

  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_MAGNETOMETER_DATA_FRAME_ID, FEB_CAN_ID_STD,
                                            tx_data, FEB_CAN_MAGNETOMETER_DATA_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}
