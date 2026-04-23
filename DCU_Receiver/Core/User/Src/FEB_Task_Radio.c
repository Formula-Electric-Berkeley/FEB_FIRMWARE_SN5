/**
 * @file    FEB_Task_Radio.c
 * @brief   Radio Task Implementation - Ping-Pong Demo
 * @author  Formula Electric @ Berkeley
 */

#include "FEB_Task_Radio.h"
#include "FEB_RFM95.h"
#include "feb_log.h"
#include "cmsis_os.h"
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
          LOG_D(TAG, "TX PONG #%lu", stats.tx_count);
        }
      }
    }
#endif

    osDelay(pdMS_TO_TICKS(10));
  }
}
