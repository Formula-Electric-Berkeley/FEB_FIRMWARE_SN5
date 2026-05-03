/**
 ******************************************************************************
 * @file           : FEB_CAN_Steering.c
 * @brief          : CAN reporter for AS5600L steering encoder.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_Steering.h"
#include "FEB_Steering.h"
#include "feb_can.h"
#include <stdint.h>

// #include "FEB_CAN_Fusion.h"
#include "feb_can_lib.h"
// #include "FEB_Fusion.h"
// #include "Fusion.h"

static uint32_t can_tx_error_count = 0;

static inline void tx_or_count(uint32_t frame_id, uint8_t *data, uint8_t len)
{

  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, frame_id, FEB_CAN_ID_STD, data, len) != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}

void FEB_CAN_Steering_Tick(void)
{
  /* ---------------- Angle data (0x50) ---------------- */
  {
    uint8_t buf[FEB_CAN_STEER_ANGLE_DATA_LENGTH] = {0};
    buf[0] = (uint8_t)(steer_angle >> 8);
    buf[1] = (uint8_t)(steer_angle & 0xFF);
    buf[2] = (uint8_t)(steer_raw_angle >> 8);
    buf[3] = (uint8_t)(steer_raw_angle & 0xFF);
    buf[4] = steer_agc;
    tx_or_count(FEB_CAN_STEER_ANGLE_DATA_FRAME_ID, buf, FEB_CAN_STEER_ANGLE_DATA_LENGTH);
  }

  /* ---------------- Status data (0x51) ---------------- */
  {
    uint8_t buf[FEB_CAN_STEER_STATUS_DATA_LENGTH] = {0};
    buf[0] = steer_status;
    buf[1] = (uint8_t)(steer_magnitude >> 8);
    buf[2] = (uint8_t)(steer_magnitude & 0xFF);
    tx_or_count(FEB_CAN_STEER_STATUS_DATA_FRAME_ID, buf, FEB_CAN_STEER_STATUS_DATA_LENGTH);
  }
}
