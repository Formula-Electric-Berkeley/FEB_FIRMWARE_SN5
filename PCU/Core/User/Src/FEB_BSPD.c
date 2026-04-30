#include "FEB_BSPD.h"
#include "feb_log.h"

/* Global BSPD data */
BSPD_TYPE BSPD;

// Reads the status of the BSPD if the BSPD reset is active.
void FEB_BSPD_CheckReset()
{
  bool reset_pin_state = HAL_GPIO_ReadPin(BSPD_RESET_PORT, BSPD_RESET_PIN);
  if (reset_pin_state)
  {
    if (BSPD.state == 0)
    {
      LOG_W(TAG_BSPD, "BSPD reset activated");
    }
    BSPD.state = 1; // BSPD reset is active
  }
  else
  {
    if (BSPD.state == 1)
    {
      LOG_I(TAG_BSPD, "BSPD reset deactivated");
    }
    BSPD.state = 0;
  }
  FEB_BSPD_CAN_Transmit();
}

// Sends the BSPD status over CAN
void FEB_BSPD_CAN_Transmit()
{
  struct feb_can_bspd_state_t msg = {0};
  msg.bspd_state = BSPD.state;

  uint8_t data[FEB_CAN_BSPD_STATE_LENGTH];
  int packed = feb_can_bspd_state_pack(data, &msg, sizeof(data));
  FEB_CAN_Status_t status =
      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BSPD_STATE_FRAME_ID, FEB_CAN_ID_STD, data, (uint8_t)packed);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_BSPD, "Failed to transmit BSPD status: %d", status);
  }
  else
  {
    LOG_D(TAG_BSPD, "BSPD status transmitted: %d", BSPD.state);
  }
}
