/**
 * @file    FEB_Task_Radio.c
 * @brief   Radio Task Implementation - Ping-Pong Demo
 * @author  Formula Electric @ Berkeley
 */

#include "FEB_Task_Radio.h"
#include "FEB_RFM95.h"
#include "FEB_Radio_Protocol.h"
#include "FEB_CAN_Stream.h"
#include "feb_can_latest.h"
#include "feb_log.h"
#include "feb_uart.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

#define TAG "[Radio]"

/* Configuration */
#define PING_INTERVAL_MS 1000
#define RX_TIMEOUT_MS 3000
/* Kept at 500 ms so the listen loop ticks at least twice a second even when no
 * packets arrive — that bounds the `signal` row cadence when the link is idle. */
#define LISTEN_RX_TIMEOUT_MS 500
#define SIGNAL_INTERVAL_MS 500
/* If no radio packet arrives within this window, the link is considered down and
 * the `signal` row reports nan,nan instead of a stale RSSI/SNR. */
#define LINK_TIMEOUT_MS 1000
#define MAX_INIT_RETRIES 5

/* Role Selection - change for second device */
#define RADIO_ROLE_PING 0
#define RADIO_ROLE_PONG 1
#define RADIO_ROLE RADIO_ROLE_PING

/* Messages */
static const char PING_MSG[] = "PING";
#if (RADIO_ROLE == RADIO_ROLE_PONG)
static const char PONG_MSG[] = "PONG";
#endif

/* Per-frame callback for FEB_Radio_Parse: update the local CAN state model and,
 * if a host is streaming, emit the frame as a `can,...` row identical to the
 * DCU's. CAN wire formats (0xFB batch / 0xFE legacy single) live in
 * FEB_Radio_Protocol.h and are shared byte-for-byte with the DCU transmitter. */
static void on_decoded_frame(uint32_t can_id, uint8_t id_type, uint8_t bus, const uint8_t *data, uint8_t dlc, void *ctx)
{
  (void)id_type;
  (void)ctx;

  int rc = FEB_CAN_State_Update(can_id, data, dlc, HAL_GetTick());
  if (rc != 0)
  {
    LOG_W(TAG, "CAN frame 0x%03X update failed (%d)", (unsigned)can_id, rc);
  }

  /* Same bus/id/dlc/data the originating DCU captured, so a host app cannot tell
   * whether it is reading from the DCU or from the receiver. */
  FEB_CAN_Stream_EmitFrame(bus, can_id, dlc, data);
}

/* Dispatch a received packet by its leading magic (packet type). Every link
 * packet carries a type byte (see the registry in FEB_Radio_Protocol.h); this
 * switch is the single place new packet types get handled. */
static void handle_radio_payload(const uint8_t *buf, uint8_t len)
{
  if (len < 1)
  {
    return;
  }

  switch (buf[0])
  {
  case FEB_RADIO_MAGIC_BATCH:
    /* CAN frame(s): parser walks the batch and calls on_decoded_frame each. */
    if (FEB_Radio_Parse(buf, len, on_decoded_frame, NULL) < 0)
    {
      LOG_W(TAG, "Malformed CAN packet: type=0x%02X len=%u", buf[0], (unsigned)len);
    }
    break;

  case FEB_RADIO_MAGIC_TEXT:
    /* RESERVED: inbound ASCII (receiver->DCU is the live path; if the DCU ever
     * sends text this is where it would be surfaced). Ignored for now. */
    break;

  default:
    /* Plain-text PING/PONG link-check traffic or an unknown type — not ours. */
    break;
  }
}

static void print_raw_packet(const uint8_t *buf, uint8_t len, int16_t rssi, int8_t snr)
{
  char line[128];
  int pos = 0;
  pos += snprintf(line + pos, sizeof(line) - pos, "RX[%d] RSSI=%d SNR=%d: ", len, rssi, snr);
  for (uint8_t i = 0; i < len && pos < (int)(sizeof(line) - 4); i++)
  {
    pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", buf[i]);
  }
  line[pos++] = '\r';
  line[pos++] = '\n';
  FEB_UART_Write(FEB_UART_INSTANCE_1, (const uint8_t *)line, pos);
  FEB_UART_Write(FEB_UART_INSTANCE_2, (const uint8_t *)line, pos);
}

static volatile bool s_listen_mode = true;

void FEB_Task_Radio_SetListenMode(bool enable)
{
  s_listen_mode = enable;
}

bool FEB_Task_Radio_GetListenMode(void)
{
  return s_listen_mode;
}

void StartRadioTask(void *argument)
{
  (void)argument;

  LOG_I(TAG, "Task started (%s)", s_listen_mode ? "listen" : (RADIO_ROLE == RADIO_ROLE_PING ? "PING" : "PONG"));

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
  uint32_t last_signal_tick = osKernelGetTickCount();
  /* Start "expired" so the link reads as down (nan) until the first packet. */
  uint32_t last_rx_tick = osKernelGetTickCount() - pdMS_TO_TICKS(LINK_TIMEOUT_MS);
#if (RADIO_ROLE == RADIO_ROLE_PING)
  uint32_t last_tick = osKernelGetTickCount();
#endif

  for (;;)
  {
    /* While a host is streaming CAN, also publish radio link quality (RSSI/SNR)
     * as a `signal` row roughly every SIGNAL_INTERVAL_MS. Report nan,nan once no
     * packet has been seen for LINK_TIMEOUT_MS so the UI shows "no link". */
    uint32_t now_tick = osKernelGetTickCount();
    if (FEB_CAN_Stream_IsStreaming() && (now_tick - last_signal_tick) >= pdMS_TO_TICKS(SIGNAL_INTERVAL_MS))
    {
      last_signal_tick = now_tick;
      bool link_up = (now_tick - last_rx_tick) < pdMS_TO_TICKS(LINK_TIMEOUT_MS);
      FEB_CAN_Stream_EmitSignal(link_up, FEB_RFM95_GetRSSI(), FEB_RFM95_GetSNR());
    }

    if (s_listen_mode)
    {
      /* Listen-only mode: continuously receive, no TX. FEB_RFM95_Receive already
       * blocks (up to LISTEN_RX_TIMEOUT_MS), so re-arm immediately after each
       * packet — a long post-RX delay here is a window where streamed batches
       * get dropped. A 1-tick yield keeps equal-priority tasks fed. */
      status = FEB_RFM95_Receive(rx_buffer, &rx_len, LISTEN_RX_TIMEOUT_MS);
      if (status == FEB_RFM95_OK)
      {
        last_rx_tick = osKernelGetTickCount(); /* mark the link alive */
        LOG_I(TAG, "[listen] RX %u bytes, RSSI=%d, SNR=%d", rx_len, FEB_RFM95_GetRSSI(), FEB_RFM95_GetSNR());
        print_raw_packet(rx_buffer, rx_len, FEB_RFM95_GetRSSI(), FEB_RFM95_GetSNR());
        handle_radio_payload(rx_buffer, rx_len);
      }
      osDelay(1);
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
        LOG_D(TAG, "TX PING #%lu", stats.tx_count);

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
    /* PONG role: wait for PING or decode CAN-over-radio */
    status = FEB_RFM95_Receive(rx_buffer, &rx_len, RX_TIMEOUT_MS);

    if (status == FEB_RFM95_OK)
    {
      LOG_I(TAG, "RX %u bytes, RSSI=%d, SNR=%d", rx_len, FEB_RFM95_GetRSSI(), FEB_RFM95_GetSNR());
      print_raw_packet(rx_buffer, rx_len, FEB_RFM95_GetRSSI(), FEB_RFM95_GetSNR());

      if (rx_len >= 4 && memcmp(rx_buffer, PING_MSG, 4) == 0)
      {
        /* Send PONG response */
        status = FEB_RFM95_Transmit((const uint8_t *)PONG_MSG, strlen(PONG_MSG), 1000);

        if (status == FEB_RFM95_OK)
        {
          FEB_RFM95_Stats_t stats;
          FEB_RFM95_GetStats(&stats);
          LOG_D(TAG, "TX PONG #%lu", stats.tx_count);
        }
      }
      else
      {
        handle_radio_payload(rx_buffer, rx_len);
      }
    }
#endif

    osDelay(pdMS_TO_TICKS(10));
  }
}
