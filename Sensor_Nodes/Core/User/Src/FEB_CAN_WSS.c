/**
 ******************************************************************************
 * @file           : FEB_CAN_WSS.c
 * @brief          : CAN reporter for wheel speed sensors. FRONT and REAR DBC
 *                   layouts are identical (two uint16 @ 0.01 mph + dir flags),
 *                   so this reporter is fully variant-agnostic via the
 *                   FEB_SN_* WSS aliases. No-op if FEB_SN_HAS_WSS=0.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * WSS (0x24 FRONT / 0x25 REAR) wss_*_data:
 *   wss_left_*    (u16, 0.01 mph/LSB)
 *   wss_right_*   (u16, 0.01 mph/LSB)
 *   wss_dir_flags (u8: bit0 left_dir, bit1 right_dir; 1 = reverse)
 ******************************************************************************
 */

#include "FEB_CAN_WSS.h"
#include "feb_can_lib.h"
#include "feb_can.h"
#include "FEB_WSS.h"
#include "FEB_SN_Config.h"
#include <stdint.h>

static uint32_t can_tx_error_count = 0;

void FEB_CAN_WSS_Init(void) {}

void FEB_CAN_WSS_Tick(void)
{
#if FEB_SN_HAS_WSS
  /* The encode helpers expect physical units (mph); globals are in 0.01 mph units, so scale. */
  const double left_mph = (double)left_mph_x100 * 0.01;
  const double right_mph = (double)right_mph_x100 * 0.01;

  uint8_t flags = 0;
  if (left_dir < 0)
    flags |= (1u << 0);
  if (right_dir < 0)
    flags |= (1u << 1);

  struct feb_sn_wss_t s = {
      .feb_sn_wss_left = feb_sn_wss_left_encode(left_mph),
      .feb_sn_wss_right = feb_sn_wss_right_encode(right_mph),
      .feb_sn_wss_dir_flags = feb_sn_wss_dir_flags_encode((double)flags),
  };
  uint8_t buf[FEB_SN_WSS_LENGTH];
  feb_sn_wss_pack(buf, &s, sizeof(buf));

  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_SN_WSS_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf)) != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
#endif
}
