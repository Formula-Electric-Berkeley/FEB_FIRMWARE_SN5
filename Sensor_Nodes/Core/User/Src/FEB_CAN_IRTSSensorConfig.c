/**
 ******************************************************************************
 * @file           : FEB_CAN_IRTSSensorConfig.c
 * @brief          : One-shot CAN burst to program an Infrared Tire Temperature
 *                   Sensor (IRTS). Transmits a fixed 8-byte config frame at
 *                   1 Hz for 15 s, then stops automatically.
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Modeled on FEB_SN_PingPong.c: state lives here, the console command kicks it
 * off, and FEB_CAN_IRTSSensorConfig_Tick() (called from the main loop) does the
 * timed transmitting. The very first frame goes out on Start(); subsequent
 * frames go out once per second until the 15 s window closes.
 ******************************************************************************
 */

#include "FEB_CAN_IRTSSensorConfig.h"
#include "feb_can_lib.h"
#include "feb_log.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define TAG_IRTS "[IRTS]"

/* The four config frames. Each tick (1 Hz) all four are sent in order, frame 0
 * first. Fill in the CAN ID, the ID type, and the 8 payload bytes (byte 0
 * first) for each. Use FEB_CAN_ID_STD for an 11-bit ID or FEB_CAN_ID_EXT for a
 * 29-bit ID. */
typedef struct
{
  uint32_t can_id;
  FEB_CAN_ID_Type_t id_type;
  uint8_t data[8];
} FEB_IRTS_Frame_t;

static const FEB_IRTS_Frame_t FEB_IRTS_CONFIG_FRAMES[] = {
    /* ---- frame 0 ---- */
    {
        .can_id = 0x4B0,
        .id_type = FEB_CAN_ID_STD,
        .data = {0x75, 0x30, 0x04, 0xB0, 95, 4, 40, 4},
    },
    /* ---- frame 1 ---- */
    {
        .can_id = 0x4B4,
        .id_type = FEB_CAN_ID_STD,
        .data = {0x75, 0x30, 0x04, 0xB4, 95, 4, 40, 4},
    },
    /* ---- frame 2 ---- */
    {
        .can_id = 0x4B8,
        .id_type = FEB_CAN_ID_STD,
        .data = {0x75, 0x30, 0x04, 0xB8, 95, 4, 40, 4},
    },
    /* ---- frame 3 ---- */
    {
        .can_id = 0x4BC,
        .id_type = FEB_CAN_ID_STD,
        .data = {0x75, 0x30, 0x04, 0xBC, 95, 4, 40, 4},
    },
};

#define FEB_IRTS_FRAME_COUNT (sizeof(FEB_IRTS_CONFIG_FRAMES) / sizeof(FEB_IRTS_CONFIG_FRAMES[0]))

/* CAN peripheral the sensor lives on. Sensor-node reporters all use
 * instance 1; change here if the IRTS is wired to CAN2. */
#define FEB_IRTS_CAN_INSTANCE FEB_CAN_INSTANCE_1

#define FEB_IRTS_BURST_DURATION_MS 15000u /* total run time */
#define FEB_IRTS_SEND_PERIOD_MS 1000u     /* 1 Hz */

static bool active = false;
static uint32_t start_ms = 0;     /* tick when the burst began */
static uint32_t next_send_ms = 0; /* tick of the next scheduled send */
static uint32_t sent_count = 0;   /* frames sent in the current/last burst */

/* Send all four config frames back-to-back (one 1 Hz cycle). */
static void irts_send_frames(void)
{
  for (size_t i = 0; i < FEB_IRTS_FRAME_COUNT; i++)
  {
    const FEB_IRTS_Frame_t *f = &FEB_IRTS_CONFIG_FRAMES[i];
    uint8_t buf[8];
    memcpy(buf, f->data, sizeof(buf));

    FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_IRTS_CAN_INSTANCE, f->can_id, f->id_type, buf, sizeof(buf));
    sent_count++;

    if (status == FEB_CAN_OK)
    {
      LOG_D(TAG_IRTS, "TX frame%u ID:0x%03X #%lu enq", (unsigned)i, (unsigned int)f->can_id, (unsigned long)sent_count);
    }
    else
    {
      LOG_W(TAG_IRTS, "TX frame%u ID:0x%03X #%lu dropped status=%d", (unsigned)i, (unsigned int)f->can_id,
            (unsigned long)sent_count, (int)status);
    }
  }
}

void FEB_CAN_IRTSSensorConfig_Init(void)
{
  active = false;
  start_ms = 0;
  next_send_ms = 0;
  sent_count = 0;
}

void FEB_CAN_IRTSSensorConfig_Start(void)
{
  const uint32_t now = HAL_GetTick();
  active = true;
  start_ms = now;
  sent_count = 0;

  /* Fire the first cycle immediately, then schedule the rest at 1 Hz. */
  irts_send_frames();
  next_send_ms = now + FEB_IRTS_SEND_PERIOD_MS;
}

void FEB_CAN_IRTSSensorConfig_Stop(void)
{
  active = false;
}

bool FEB_CAN_IRTSSensorConfig_IsActive(void)
{
  return active;
}

uint32_t FEB_CAN_IRTSSensorConfig_RemainingMs(void)
{
  if (!active)
  {
    return 0;
  }
  const uint32_t elapsed = HAL_GetTick() - start_ms;
  if (elapsed >= FEB_IRTS_BURST_DURATION_MS)
  {
    return 0;
  }
  return FEB_IRTS_BURST_DURATION_MS - elapsed;
}

uint32_t FEB_CAN_IRTSSensorConfig_SentCount(void)
{
  return sent_count;
}

void FEB_CAN_IRTSSensorConfig_Tick(void)
{
  if (!active)
  {
    return;
  }

  const uint32_t now = HAL_GetTick();

  /* Window closed: stop after the full 15 s. */
  if ((uint32_t)(now - start_ms) >= FEB_IRTS_BURST_DURATION_MS)
  {
    active = false;
    LOG_I(TAG_IRTS, "burst done (%lu frames over %u ms)", (unsigned long)sent_count,
          (unsigned int)FEB_IRTS_BURST_DURATION_MS);
    return;
  }

  /* 1 Hz cadence. */
  if ((int32_t)(now - next_send_ms) >= 0)
  {
    irts_send_frames();
    next_send_ms += FEB_IRTS_SEND_PERIOD_MS;
  }
}
