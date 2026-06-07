/**
 ******************************************************************************
 * @file           : FEB_CAN_LinearPotentiometer.c
 * @brief          : CAN Linear Potentiometer Reporter Module Implementation.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_LinearPotentiometer.h"

#include <stddef.h>

#include "FEB_LinearPotentiometer.h"
#include "FEB_SN_Config.h"
#include "feb_can.h"
#include "feb_can_lib.h"

static uint32_t can_tx_error_count = 0;

void FEB_CAN_LinearPotentiometer_Init(void) {}

#if FEB_SN_HAS_LINEAR_POTENTIOMETER
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
#endif

void FEB_CAN_LinearPotentiometer_Tick(void)
{
#if FEB_SN_HAS_LINEAR_POTENTIOMETER
  /* Variant-agnostic: FEB_SN_LINPOT_* / feb_sn_linpot_* resolve to the FRONT
   * (0x1E) or REAR (0x1F) message via FEB_SN_Config.h. [0] = Left, [1] = Right. */
  struct feb_sn_linpot_t msg = {
      .feb_sn_linpot_left = mm_to_can_units(lp_position_mm[0]),
      .feb_sn_linpot_right = mm_to_can_units(lp_position_mm[1]),
  };

  uint8_t tx_data[FEB_SN_LINPOT_LENGTH] = {0};
  if (feb_sn_linpot_pack(tx_data, &msg, sizeof(tx_data)) <= 0)
  {
    can_tx_error_count++;
    return;
  }

  FEB_CAN_Status_t status =
      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_SN_LINPOT_FRAME_ID, FEB_CAN_ID_STD, tx_data, FEB_SN_LINPOT_LENGTH);
  if (status != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
#endif
}
