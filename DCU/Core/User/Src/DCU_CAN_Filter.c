/**
 ******************************************************************************
 * @file           : DCU_CAN_Filter.c
 * @brief          : Allow-list + per-ID rate limit deciding which CAN frames
 *                   are forwarded to radio
 * @author         : Formula Electric @ Berkeley
 *
 * The CSV logger calls DCU_CAN_Filter_ShouldForwardToRadio() for every frame it
 * captures on CAN1/CAN2. Returning true enqueues that frame for CAN-over-radio
 * transmission (see FEB_Task_Radio_ForwardCanFrame). To keep the radio link
 * (slow: a few hundred bytes/sec) from being flooded, the decision is:
 *   1. If the (bus, can_id) is listed in k_radio_allow[], forward it subject to
 *      that entry's own .min_interval_ms rate limit.
 *   2. Otherwise, IF the "forward all" mode is enabled (DCU_CAN_FORWARD_ALL),
 *      forward it subject to the shared DCU_CAN_FORWARD_ALL_INTERVAL_MS limit.
 *   3. Otherwise, drop it.
 *
 *  >>> EDIT k_radio_allow[] for per-ID rates; toggle/tune the catch-all with the
 *      DCU_CAN_FORWARD_ALL / DCU_CAN_FORWARD_ALL_INTERVAL_MS defines below. <<<
 *
 * Threading: called only from the canLogTask context (one task), so the
 * last-forwarded timestamps need no locking.
 ******************************************************************************
 */

#include "DCU_CAN_Filter.h"
#include "DCU_CAN_Log.h" /* full DCU_CAN_Frame_t definition (bus, can_id, ts_ms) */

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Forward-all (catch-all) mode
 *
 * When enabled, any CAN ID that is NOT in k_radio_allow[] is also forwarded,
 * rate-limited to one frame per DCU_CAN_FORWARD_ALL_INTERVAL_MS per ID. IDs that
 * ARE in the allow-list keep their own tuned rate and are unaffected.
 *
 *   - Set DCU_CAN_FORWARD_ALL to 0 to disable (allow-list only).
 *   - Change DCU_CAN_FORWARD_ALL_INTERVAL_MS to set the catch-all rate.
 * ============================================================================ */
#define DCU_CAN_FORWARD_ALL 1                /* 1 = also forward every other ID, 0 = allow-list only */
#define DCU_CAN_FORWARD_ALL_INTERVAL_MS 1000 /* min ms between forwards of a non-listed ID */

/* Match any bus for an entry by setting .bus = DCU_CAN_BUS_ANY. */
#define DCU_CAN_BUS_ANY 0U

typedef struct
{
  uint8_t bus;              /**< 1 = CAN1, 2 = CAN2, DCU_CAN_BUS_ANY = either */
  uint32_t can_id;          /**< 11-bit or 29-bit identifier to forward */
  uint32_t min_interval_ms; /**< Min ms between forwards of this entry; 0 = no limit */
} DCU_CAN_AllowEntry_t;

/* ----------------------------------------------------------------------------
 * Radio forward allow-list — ADD YOUR IDS HERE.
 *
 * .min_interval_ms throttles each ID: e.g. 100 caps it to ~10 Hz on the radio
 * regardless of how fast it appears on the bus; 0 forwards every occurrence.
 *
 * Examples (delete or replace with the real IDs you want on the radio link):
 *   { .bus = 1,               .can_id = 0x0A0, .min_interval_ms = 100 }, // <=10 Hz
 *   { .bus = 2,               .can_id = 0x123, .min_interval_ms = 50  }, // <=20 Hz
 *   { .bus = DCU_CAN_BUS_ANY, .can_id = 0x300, .min_interval_ms = 0   }, // every frame
 * -------------------------------------------------------------------------- */
static const DCU_CAN_AllowEntry_t k_radio_allow[] = {
    {.bus = 1, .can_id = 0xD0, .min_interval_ms = 500}, // PCU heartbeat
    {.bus = 1, .can_id = 0xD1, .min_interval_ms = 500}, // DASH heartbeat
    {.bus = 1, .can_id = 0xD2, .min_interval_ms = 500}, // LVPDB heartbeat
    {.bus = 1, .can_id = 0xD3, .min_interval_ms = 500}, // DCU heartbeat
    {.bus = 1, .can_id = 0xD4, .min_interval_ms = 500}, // Front sensor node heartbeat
    {.bus = 1, .can_id = 0xD5, .min_interval_ms = 500}, // Rear sensor node heartbeat

    {.bus = 1, .can_id = 0x25, .min_interval_ms = 250}, // Rear sensor node data (wheel speed, 0.01 mph/LSB)
    {.bus = 1, .can_id = 0x10, .min_interval_ms = 500}, // Dash state

    {.bus = 1, .can_id = 0x20, .min_interval_ms = 2000}, // Front left tire temperature
    {.bus = 1, .can_id = 0x21, .min_interval_ms = 2000}, // Front right tire temperature
    {.bus = 1, .can_id = 0x22, .min_interval_ms = 2000}, // Rear left tire temperature
    {.bus = 1, .can_id = 0x23, .min_interval_ms = 2000}, // Rear right tire temperature

    {.bus = 1, .can_id = 0x26, .min_interval_ms = 500}, // [IMU][FRONT] accelerometer data (raw)

    {.bus = 1, .can_id = 0x02, .min_interval_ms = 500}, // Accumulator pack voltage
    {.bus = 1, .can_id = 0x03, .min_interval_ms = 500}, // Accumulator pack temperature
    {.bus = 1, .can_id = 0x04, .min_interval_ms = 500}, // Accumulator fault flags
    {.bus = 1, .can_id = 0x05, .min_interval_ms = 500}, // BMS state machine status
};

#define DCU_CAN_ALLOW_COUNT (sizeof(k_radio_allow) / sizeof(k_radio_allow[0]))
/* Guarantee a non-zero array dimension even when the allow-list is empty. */
#define DCU_CAN_ALLOW_SLOTS (DCU_CAN_ALLOW_COUNT > 0 ? DCU_CAN_ALLOW_COUNT : 1)

/* Last forward time per allow-list entry (HAL tick ms), zero-initialized. Same
 * index as k_radio_allow[]. */
static uint32_t s_last_fwd_ms[DCU_CAN_ALLOW_SLOTS];

#if DCU_CAN_FORWARD_ALL
/* Direct-mapped "last seen" cache for the catch-all path. We cannot keep a slot
 * for every possible ID (29-bit space), so IDs hash into a fixed table; on a
 * hash collision the newcomer evicts the resident and is forwarded immediately
 * (worst case: two colliding IDs each forward a bit more often than the nominal
 * interval — harmless for telemetry). Sized as a power of two for masking. */
#define DCU_CAN_FWDALL_SLOTS 128U

typedef struct
{
  uint32_t key;     /**< (bus << 29) | can_id; 0 = empty (bus is always >= 1) */
  uint32_t last_ms; /**< HAL tick of last forward for the resident key */
} DCU_CAN_FwdAllSlot_t;

static DCU_CAN_FwdAllSlot_t s_fwdall[DCU_CAN_FWDALL_SLOTS];

static bool fwdall_should_forward(uint8_t bus, uint32_t can_id, uint32_t now)
{
  const uint32_t key = ((uint32_t)bus << 29) | (can_id & 0x1FFFFFFFU);
  const uint32_t h = (key ^ (key >> 15)) & (DCU_CAN_FWDALL_SLOTS - 1U);
  DCU_CAN_FwdAllSlot_t *slot = &s_fwdall[h];

  if (slot->key == key)
  {
    /* Same ID we last saw in this slot — apply the shared rate limit. */
    if ((uint32_t)(now - slot->last_ms) >= (uint32_t)DCU_CAN_FORWARD_ALL_INTERVAL_MS)
    {
      slot->last_ms = now;
      return true;
    }
    return false;
  }

  /* Empty slot or a different (colliding) ID — claim it and forward now. */
  slot->key = key;
  slot->last_ms = now;
  return true;
}
#endif /* DCU_CAN_FORWARD_ALL */

bool DCU_CAN_Filter_ShouldForwardToRadio(const DCU_CAN_Frame_t *frame)
{
  if (frame == NULL)
  {
    return false;
  }

  for (size_t i = 0; i < DCU_CAN_ALLOW_COUNT; i++)
  {
    const bool bus_ok = (k_radio_allow[i].bus == DCU_CAN_BUS_ANY) || (k_radio_allow[i].bus == frame->bus);
    if (!bus_ok || k_radio_allow[i].can_id != frame->can_id)
    {
      continue;
    }

    const uint32_t interval = k_radio_allow[i].min_interval_ms;
    if (interval == 0U)
    {
      return true; /* no throttle for this entry */
    }

    /* Forward only if at least `interval` ms have passed since this entry last
     * went out. frame->ts_ms is HAL_GetTick() captured at enqueue; the unsigned
     * subtraction makes the comparison correct across the 32-bit tick wrap. */
    const uint32_t now = frame->ts_ms;
    if ((uint32_t)(now - s_last_fwd_ms[i]) >= interval)
    {
      s_last_fwd_ms[i] = now;
      return true;
    }
    return false; /* arrived too soon — drop this occurrence */
  }

  /* Not in the allow-list. */
#if DCU_CAN_FORWARD_ALL
  return fwdall_should_forward(frame->bus, frame->can_id, frame->ts_ms);
#else
  return false;
#endif
}
