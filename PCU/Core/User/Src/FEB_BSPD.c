#include "FEB_BSPD.h"
#include "FEB_Debug.h"

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
  uint8_t data[8];
  data[0] = BSPD.state;

  FEB_CAN_Status_t status = FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_BSPD_STATUS, data, 1);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_BSPD, "Failed to transmit BSPD status: %d", status);
  }
  else
  {
    LOG_D(TAG_BSPD, "BSPD status transmitted: %d", BSPD.state);
  }
}