/**
 ******************************************************************************
 * @file           : FEB_CAN_LinearPotentiometer.c
 * @brief          : CAN Linear Potentiometer Reporter Module Implementation.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_LinearPotentiometer.h"

#include <stddef.h>
#include <string.h>

#include "FEB_LinearPotentiometer.h"
#include "feb_can.h"
#include "feb_can_lib.h"

static uint32_t can_tx_error_count = 0;

void FEB_CAN_LinearPotentiometer_Init(void) {}

static uint16_t mm_to_can_units(float mm)
{
  float scaled = mm * 100.0f; /* 0.01 mm/LSB on the wire */
  if (scaled < 0.0f)
  {
    return 0u;
  }
  if (scaled > 65535.0f)
  {
    return 65535u;
  }
  return (uint16_t)scaled;
}

void FEB_CAN_LinearPotentiometer_Tick(void)
{
  struct feb_can_linear_potentiometer_front_t msg = {
      .linear_potentiometer_1_front = mm_to_can_units(lp_displacement_mm[0]),
      .linear_potentiometer_2_front = mm_to_can_units(lp_displacement_mm[1]),
  };

  uint8_t tx_data[FEB_CAN_LINEAR_POTENTIOMETER_FRONT_LENGTH] = {0};
  if (feb_can_linear_potentiometer_front_pack(tx_data, &msg, sizeof(tx_data)) <= 0)
  {
    can_tx_error_count++;
    return;
  }

  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_LINEAR_POTENTIOMETER_FRONT_FRAME_ID,
                                            FEB_CAN_ID_STD, tx_data, FEB_CAN_LINEAR_POTENTIOMETER_FRONT_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
}
