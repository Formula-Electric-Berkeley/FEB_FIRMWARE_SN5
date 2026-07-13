/**
 * @file    FEB_Task_Radio.c
 * @brief   Radio Task Implementation
 * @author  Formula Electric @ Berkeley
 *
 * Three coexisting modes, selected at runtime (see the dcu|radio console cmds):
 *   - stream  : CAN-over-radio TX. Drains the forward queue, packs frames into
 *               a batch packet (FEB_Radio_Protocol.h), and transmits. Between
 *               batches it briefly listens so the receiver can send ASCII back
 *               (foundation for DCU_Receiver -> DCU messaging).
 *   - listen  : RX-only, logs whatever arrives.
 *   - ping    : the original ping/pong link-check demo (default).
 */

#include "FEB_Task_Radio.h"
#include "FEB_RFM95.h"
#include "FEB_Radio_Protocol.h"
#include "DCU_CAN_Log.h" /* DCU_CAN_Frame_t */
#include "feb_log.h"
#include "cmsis_os.h"
#include <string.h>

#define TAG "[Radio]"

/* Configuration */
#define PING_INTERVAL_MS 1000
#define RX_TIMEOUT_MS 3000
#define LISTEN_RX_TIMEOUT_MS 1000
#define MAX_INIT_RETRIES 5

/* Streaming tunables --------------------------------------------------------
 * The radio task waits up to STREAM_LINGER_MS for the first frame of a batch,
 * then drains everything already queued without further waiting and sends one
 * packet. This bounds added latency while still coalescing bursts. When no
 * frames are pending it opens a short RX window so inbound ASCII can arrive. */
#define STREAM_LINGER_MS 15
#define STREAM_TX_TIMEOUT_MS 1000
#define STREAM_IDLE_RX_MS 20
#define SIGNAL_INTERVAL_MS 500
#define FWD_QUEUE_DEPTH 64U

/* Role Selection - change for second device */
#define RADIO_ROLE_PING 0
#define RADIO_ROLE_PONG 1
#define RADIO_ROLE RADIO_ROLE_PING

/* Messages */
static const char PING_MSG[] = "PING";
#if (RADIO_ROLE == RADIO_ROLE_PONG)
static const char PONG_MSG[] = "PONG";
#endif

static volatile bool s_listen_mode = false;
static volatile bool s_stream_mode = true;

/* Forward path: CAN logger task -> radio task. Created in StartRadioTask before
 * the init-retry loop so producers have a valid handle as early as possible. */
static osMessageQueueId_t s_fwd_queue = NULL;
static volatile uint32_t s_fwd_drops = 0;

void FEB_Task_Radio_SetListenMode(bool enable)
{
  s_listen_mode = enable;
}

bool FEB_Task_Radio_GetListenMode(void)
{
  return s_listen_mode;
}

void FEB_Task_Radio_SetStreamMode(bool enable)
{
  s_stream_mode = enable;
}

bool FEB_Task_Radio_GetStreamMode(void)
{
  return s_stream_mode;
}

void FEB_Task_Radio_ForwardCanFrame(const struct DCU_CAN_Frame *frame)
{
  if (frame == NULL || s_fwd_queue == NULL || !s_stream_mode)
  {
    return;
  }

  /* Drop-oldest on a full queue so the freshest telemetry still gets through. */
  if (osMessageQueuePut(s_fwd_queue, frame, 0U, 0U) != osOK)
  {
    DCU_CAN_Frame_t discarded;
    (void)osMessageQueueGet(s_fwd_queue, &discarded, NULL, 0U);
    (void)osMessageQueuePut(s_fwd_queue, frame, 0U, 0U);
    s_fwd_drops++;
  }
}

uint32_t FEB_Task_Radio_GetForwardDropCount(void)
{
  return s_fwd_drops;
}

/* Build one batch from the forward queue and transmit it. Returns true if a
 * packet was sent (i.e. there was at least one frame to forward). */
static bool stream_pump(void)
{
  uint8_t pkt[FEB_RADIO_BATCH_MAX_BYTES];
  FEB_Radio_BatchBuilder_t b;
  FEB_Radio_BatchBegin(&b, pkt, sizeof(pkt));

  DCU_CAN_Frame_t frame;
  uint32_t wait = pdMS_TO_TICKS(STREAM_LINGER_MS); /* block for the first frame */

  while (FEB_Radio_BatchCount(&b) < FEB_RADIO_BATCH_MAX_FRAMES &&
         osMessageQueueGet(s_fwd_queue, &frame, NULL, wait) == osOK)
  {
    if (!FEB_Radio_BatchAdd(&b, frame.can_id, frame.id_type, frame.bus, frame.data, frame.dlc))
    {
      /* Batch is full — the queue still holds this frame's successors; put this
       * one back so it leads the next batch, then ship what we have. */
      (void)osMessageQueuePut(s_fwd_queue, &frame, 0U, 0U);
      break;
    }
    wait = 0U; /* subsequent frames: take only what is already queued */
  }

  if (FEB_Radio_BatchCount(&b) == 0)
  {
    return false;
  }

  if (FEB_RFM95_Transmit(pkt, b.len, STREAM_TX_TIMEOUT_MS) != FEB_RFM95_OK)
  {
    LOG_W(TAG, "stream TX failed (%u frames)", (unsigned)FEB_Radio_BatchCount(&b));
  }
  return true;
}

void StartRadioTask(void *argument)
{
  (void)argument;

  LOG_I(TAG, "Task started (%s)", s_stream_mode ? "CAN stream" : (RADIO_ROLE == RADIO_ROLE_PING ? "PING" : "PONG"));

  /* Create the forward queue first so canLogTask can enqueue immediately. */
  s_fwd_queue = osMessageQueueNew(FWD_QUEUE_DEPTH, sizeof(DCU_CAN_Frame_t), NULL);
  if (s_fwd_queue == NULL)
  {
    LOG_E(TAG, "Forward queue alloc failed — radio streaming disabled");
  }

  /* Initialize with retries */
  uint8_t attempts = 0;
  FEB_RFM95_Status_t status;

  while (attempts < MAX_INIT_RETRIES)
  {
    attempts++;
    status = FEB_RFM95_Init(NULL);

    if (status == FEB_RFM95_OK)
    {
      break;
    }

    LOG_W(TAG, "Init attempt %d/%d failed", attempts, MAX_INIT_RETRIES);
    osDelay(pdMS_TO_TICKS(100));
  }

  if (status != FEB_RFM95_OK)
  {
    LOG_E(TAG, "FATAL: Init failed");
    for (;;)
    {
      osDelay(pdMS_TO_TICKS(1000));
    }
  }

  /* Main loop */
  uint8_t rx_buffer[255];
  uint8_t rx_len;
  uint32_t last_tick = osKernelGetTickCount();

  for (;;)
  {
    /* NOTE: `signal` (RSSI/SNR) rows are intentionally NOT emitted on the DCU —
     * it is the transmitter, so its last-packet RSSI/SNR is not a useful link
     * metric. The receiver emits `signal` instead. DCU_CAN_Log_EmitSignal()
     * remains available if this is ever revisited. */

    if (s_stream_mode)
    {
      /* CAN-over-radio TX. stream_pump blocks up to STREAM_LINGER_MS for work,
       * so this branch never busy-spins. When idle, open a short RX window so
       * the receiver can push ASCII back to the car (half-duplex). */
      if (!stream_pump())
      {
        status = FEB_RFM95_Receive(rx_buffer, &rx_len, STREAM_IDLE_RX_MS);
        if (status == FEB_RFM95_OK)
        {
          LOG_I(TAG, "[stream] inbound %u bytes, RSSI=%d", rx_len, FEB_RFM95_GetRSSI());
        }
      }
      continue;
    }

    if (s_listen_mode)
    {
      /* Listen-only mode: continuously receive, no TX */
      status = FEB_RFM95_Receive(rx_buffer, &rx_len, LISTEN_RX_TIMEOUT_MS);
      if (status == FEB_RFM95_OK)
      {
        LOG_I(TAG, "[listen] RX %u bytes, RSSI=%d, SNR=%d", rx_len, FEB_RFM95_GetRSSI(), FEB_RFM95_GetSNR());
      }
      osDelay(pdMS_TO_TICKS(10));
      continue;
    }

#if (RADIO_ROLE == RADIO_ROLE_PING)
    /* PING role: send PING, wait for PONG */
    uint32_t now = osKernelGetTickCount();

    if (now - last_tick >= pdMS_TO_TICKS(PING_INTERVAL_MS))
    {
      status = FEB_RFM95_Transmit((const uint8_t *)PING_MSG, strlen(PING_MSG), 1000);

      if (status == FEB_RFM95_OK)
      {
        FEB_RFM95_Stats_t stats;
        FEB_RFM95_GetStats(&stats);
        LOG_D(TAG, "TX PING #%lu", (unsigned long)stats.tx_count);

        /* Wait for PONG */
        status = FEB_RFM95_Receive(rx_buffer, &rx_len, RX_TIMEOUT_MS);

        if (status == FEB_RFM95_OK)
        {
          LOG_I(TAG, "RX %u bytes, RSSI=%d, SNR=%d", rx_len, FEB_RFM95_GetRSSI(), FEB_RFM95_GetSNR());
        }
        else if (status == FEB_RFM95_ERR_RX_TIMEOUT)
        {
          LOG_W(TAG, "RX timeout");
        }
      }

      last_tick = now;
    }

#else
    /* PONG role: wait for PING, send PONG */
    status = FEB_RFM95_Receive(rx_buffer, &rx_len, RX_TIMEOUT_MS);

    if (status == FEB_RFM95_OK)
    {
      LOG_I(TAG, "RX %u bytes, RSSI=%d, SNR=%d", rx_len, FEB_RFM95_GetRSSI(), FEB_RFM95_GetSNR());

      if (rx_len >= 4 && memcmp(rx_buffer, PING_MSG, 4) == 0)
      {
        /* Send PONG response */
        status = FEB_RFM95_Transmit((const uint8_t *)PONG_MSG, strlen(PONG_MSG), 1000);

        if (status == FEB_RFM95_OK)
        {
          FEB_RFM95_Stats_t stats;
          FEB_RFM95_GetStats(&stats);
          LOG_D(TAG, "TX PONG #%lu", (unsigned long)stats.tx_count);
        }
      }
    }
#endif

    osDelay(pdMS_TO_TICKS(10));
  }
}
