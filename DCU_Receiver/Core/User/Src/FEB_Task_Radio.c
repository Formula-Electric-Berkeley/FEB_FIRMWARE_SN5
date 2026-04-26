/**
 * @file    FEB_Task_Radio.c
 * @brief   Radio Task Implementation - Ping-Pong Demo
 * @author  Formula Electric @ Berkeley
 */

#include "FEB_Task_Radio.h"
#include "FEB_RFM95.h"
#include "feb_can_latest.h"
#include "feb_log.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define TAG "[Radio]"

/* Configuration */
#define PING_INTERVAL_MS 1000
#define RX_TIMEOUT_MS 3000
#define LISTEN_RX_TIMEOUT_MS 1000
#define MAX_INIT_RETRIES 5

/* Role Selection - change for second device */
#define RADIO_ROLE_PING 0
#define RADIO_ROLE_PONG 1
#define RADIO_ROLE RADIO_ROLE_PONG

/* Messages */
static const char PING_MSG[] = "PING";
#if (RADIO_ROLE == RADIO_ROLE_PONG)
static const char PONG_MSG[] = "PONG";
#endif

/* Wire format for CAN-over-radio:
 *   [0] = 0xFE magic
 *   [1..2] = frame_id (little-endian uint16)
 *   [3] = dlc (0..8)
 *   [4..4+dlc-1] = payload
 * Total length = 4 + dlc (<= 12). */
#define RADIO_CAN_MAGIC 0xFE

static void handle_radio_payload(const uint8_t *buf, uint8_t len)
{
  if (len >= 1 && buf[0] == RADIO_CAN_MAGIC && len >= 4)
  {
    uint16_t frame_id = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
    uint8_t dlc = buf[3];
    if (dlc <= 8 && (uint16_t)len == (uint16_t)(4 + dlc))
    {
      int rc = FEB_CAN_State_Update(frame_id, &buf[4], dlc, HAL_GetTick());
      if (rc != 0)
      {
        LOG_W(TAG, "CAN frame 0x%03X update failed (%d)", (unsigned)frame_id, rc);
      }
    }
    else
    {
      LOG_W(TAG, "CAN frame malformed: len=%u dlc=%u", (unsigned)len, (unsigned)dlc);
    }
  }
}

static volatile bool s_listen_mode = false;

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

  LOG_I(TAG, "Task started (%s mode)", RADIO_ROLE == RADIO_ROLE_PING ? "PING" : "PONG");

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
  uint8_t rx_buffer[64];
  uint8_t rx_len;
#if (RADIO_ROLE == RADIO_ROLE_PING)
  uint32_t last_tick = osKernelGetTickCount();
#endif

  for (;;)
  {
    if (s_listen_mode)
    {
      /* Listen-only mode: continuously receive, no TX */
      status = FEB_RFM95_Receive(rx_buffer, &rx_len, LISTEN_RX_TIMEOUT_MS);
      if (status == FEB_RFM95_OK)
      {
        LOG_I(TAG, "[listen] RX %u bytes, RSSI=%d, SNR=%d",
              rx_len, FEB_RFM95_GetRSSI(), FEB_RFM95_GetSNR());
        handle_radio_payload(rx_buffer, rx_len);
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
