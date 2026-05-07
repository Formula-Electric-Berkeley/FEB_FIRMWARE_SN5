/**
 ******************************************************************************
 * @file           : FEB_CAN_WSS.c
 * @brief          : CAN reporter for wheel speed sensors. Variant-aware:
 *                   FRONT and REAR DBC layouts genuinely differ, so this is
 *                   the one reporter with an explicit #if FEB_SN_IS_FRONT().
 *                   No-op if FEB_SN_HAS_WSS=0.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * FRONT (0x24) wss_front_data:
 *   wss_left_front  (u16, 0.1 RPM/LSB)
 *   wss_right_front (u16, 0.1 RPM/LSB)
 *   wss_dir_flags   (u8: bit0 left_dir, bit1 right_dir; 1 = reverse)
 *
 * REAR  (0x25) wss_rear_data:
 *   wss_right_rear  (u32, 0.1 RPM/LSB)
 *   wss_left_rear   (u32, 0.1 RPM/LSB)
 *   (no direction flags — FEB_WSS.c still tracks direction internally)
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
  /* The encode helpers expect physical units (RPM); globals are in 0.1 RPM units, so divide. */
  const double left_rpm = (double)left_rpm_x10 * 0.1;
  const double right_rpm = (double)right_rpm_x10 * 0.1;

#if FEB_SN_IS_FRONT()
  uint8_t flags = 0;
  if (left_dir < 0)
    flags |= (1u << 0);
  if (right_dir < 0)
    flags |= (1u << 1);

  struct feb_can_wss_front_data_t s = {
      .wss_left_front = feb_can_wss_front_data_wss_left_front_encode(left_rpm),
      .wss_right_front = feb_can_wss_front_data_wss_right_front_encode(right_rpm),
      .wss_dir_flags = feb_can_wss_front_data_wss_dir_flags_encode((double)flags),
  };
  uint8_t buf[FEB_SN_WSS_LENGTH];
  feb_can_wss_front_data_pack(buf, &s, sizeof(buf));
#else /* REAR */
  struct feb_can_wss_rear_data_t s = {
      .wss_left_rear = feb_can_wss_rear_data_wss_left_rear_encode(left_rpm),
      .wss_right_rear = feb_can_wss_rear_data_wss_right_rear_encode(right_rpm),
  };
  uint8_t buf[FEB_SN_WSS_LENGTH];
  feb_can_wss_rear_data_pack(buf, &s, sizeof(buf));
#endif

  if (FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_SN_WSS_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf)) != FEB_CAN_OK)
  {
    can_tx_error_count++;
  }
#endif
}
