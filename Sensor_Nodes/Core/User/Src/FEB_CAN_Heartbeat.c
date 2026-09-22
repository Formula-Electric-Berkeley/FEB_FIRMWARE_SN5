/**
 ******************************************************************************
 * @file           : FEB_CAN_Heartbeat.c
 * @brief          : Node liveness + fault heartbeat (0xD4 FRONT / 0xD5 REAR).
 *                   Variant-agnostic via FEB_SN_Config.h.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_CAN_Heartbeat.h"

#include "FEB_SN_Config.h"
#include "feb_can.h"
#include "feb_can_lib.h"

#include "FEB_Fusion.h"
#include "FEB_GPS.h"
#include "FEB_IMU.h"
#include "FEB_LinearPotentiometer.h"
#include "FEB_WSS.h"
#include "Fusion.h"
#include "stm32f4xx_hal.h"

#include <string.h>

/* A fix older than this means the receiver has stopped producing, even though
 * the last-known position is still being rebroadcast. Generous enough not to
 * trip on a single dropped sentence at 1 Hz. */
#define GPS_STALE_MS 3000u

/* ADC counts within this distance of a rail read as an open or shorted wiper
 * rather than a real suspension position. */
#define LP_RAIL_MARGIN 8u
#define LP_ADC_MAX 4095u

static bool latched_fault[FEB_SN_FAULT_COUNT];

void FEB_CAN_Heartbeat_Init(void)
{
  memset(latched_fault, 0, sizeof(latched_fault));
}

void FEB_CAN_Heartbeat_SetFault(FEB_SN_Fault_t fault, bool asserted)
{
  if (fault < FEB_SN_FAULT_COUNT)
  {
    latched_fault[fault] = asserted;
  }
}

bool FEB_CAN_Heartbeat_GetFault(FEB_SN_Fault_t fault)
{
  return (fault < FEB_SN_FAULT_COUNT) ? latched_fault[fault] : false;
}

#if FEB_SN_HAS_LINEAR_POTENTIOMETER
static bool lp_at_rail(void)
{
  for (int i = 0; i < FEB_LP_COUNT; i++)
  {
    if ((lp_raw[i] <= LP_RAIL_MARGIN) || (lp_raw[i] >= (LP_ADC_MAX - LP_RAIL_MARGIN)))
    {
      return true;
    }
  }
  return false;
}
#endif

/* Rising-edge detector over a monotonic counter: reports "failing right now"
 * rather than "failed at some point since boot", so a transient glitch clears
 * itself on the next tick. */
static bool counter_advanced(uint32_t now, uint32_t *prev)
{
  const bool advanced = (now != *prev);
  *prev = now;
  return advanced;
}

void FEB_CAN_Heartbeat_Tick(void)
{
  static uint32_t prev_imu_bus_err = 0;
  static uint32_t prev_mag_bus_err = 0;
  static uint32_t prev_busoff = 0;
  static uint32_t prev_txovf = 0;
  static uint32_t prev_rxovf = 0;

  struct feb_sn_heartbeat_t hb;
  memset(&hb, 0, sizeof(hb));

  /* ---- Byte 0: device init / read faults ---- */
  hb.imu_init_failed = latched_fault[FEB_SN_FAULT_IMU_INIT] ? 1u : 0u;
  hb.mag_init_failed = latched_fault[FEB_SN_FAULT_MAG_INIT] ? 1u : 0u;
  hb.gps_init_failed = latched_fault[FEB_SN_FAULT_GPS_INIT] ? 1u : 0u;

  hb.imu_read_failed = counter_advanced(imu_bus_error_count, &prev_imu_bus_err) ? 1u : 0u;
  hb.mag_read_failed = counter_advanced(mag_bus_error_count, &prev_mag_bus_err) ? 1u : 0u;

#if FEB_SN_HAS_FUSION
  {
    FusionAhrsFlags flags;
    FEB_Fusion_GetFlags(&flags);
    hb.fusion_uncalibrated = flags.startup ? 1u : 0u;
  }
#endif

#if FEB_SN_HAS_LINEAR_POTENTIOMETER
  hb.lp_out_of_range = lp_at_rail() ? 1u : 0u;
#endif

  /* ---- Byte 1: data freshness ---- */
#if FEB_SN_HAS_GPS
  {
    hb.gps_no_fix = FEB_GPS_HasFix() ? 0u : 1u;
    hb.gps_link_slow = FEB_GPS_IsFastLink() ? 0u : 1u;

    const uint32_t last = FEB_GPS_GetLastUpdateMs();
    /* Never updated is stale too — otherwise a receiver that never produced a
     * single fix would report clean. */
    hb.gps_stale = ((last == 0u) || ((uint32_t)(HAL_GetTick() - last) > GPS_STALE_MS)) ? 1u : 0u;
  }
#endif

#if FEB_SN_HAS_WSS
  hb.wss_left_no_signal = FEB_WSS_LeftHasSignal() ? 0u : 1u;
  hb.wss_right_no_signal = FEB_WSS_RightHasSignal() ? 0u : 1u;
#endif

  /* ---- Byte 2: CAN controller health ---- */
  hb.can_bus_off = counter_advanced(FEB_CAN_GetBusOffCount(), &prev_busoff) ? 1u : 0u;
  hb.can_tx_overflow = counter_advanced(FEB_CAN_GetTxQueueOverflowCount(), &prev_txovf) ? 1u : 0u;
  hb.can_rx_overflow = counter_advanced(FEB_CAN_GetRxQueueOverflowCount(), &prev_rxovf) ? 1u : 0u;

  uint8_t buf[FEB_SN_HEARTBEAT_LENGTH];
  if (feb_sn_heartbeat_pack(buf, &hb, sizeof(buf)) <= 0)
  {
    return;
  }

  /* Intentionally unguarded by any FEB_SN_HAS_* flag: the whole point is to
   * keep transmitting even when the sensors are unhappy. A send failure is
   * visible on the bus as a missing heartbeat, which is exactly the signal
   * consumers watch for, so there is nothing useful to count here. */
  (void)FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_SN_HEARTBEAT_FRAME_ID, FEB_CAN_ID_STD, buf, sizeof(buf));
}
