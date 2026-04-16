/**
 * @file    FEB_RFM95.c
 * @brief   RFM95 LoRa Radio Driver Wrapper
 * @author  Formula Electric @ Berkeley
 *
 * This is a wrapper around the stm32-hal-rfm95 library that provides
 * the existing FEB_RFM95 API for compatibility with FEB_Task_Radio.c
 */

#include "FEB_RFM95.h"
#include "rfm95.h"
#include "spi.h"
#include "main.h"
#include "feb_log.h"
#include "cmsis_os.h"
#include <string.h>

#define TAG "[RFM95]"

/* RFM95 handle instance */
static rfm95_handle_t s_rfm95;

/* Module state */
static bool s_initialized = false;
static FEB_RFM95_Stats_t s_stats = {0};

/* FreeRTOS Event Group (from freertos.c) */
extern osEventFlagsId_t radioEventsHandle;

/* Event flags for ISR-to-task signaling */
#define EVT_TX_DONE (1 << 0)
#define EVT_RX_DONE (1 << 1)
#define EVT_RX_TIMEOUT (1 << 2)
#define EVT_CRC_ERROR (1 << 3)

/* ============================================================================
 * Public API - Initialization
 * ============================================================================ */

void FEB_RFM95_GetDefaultConfig(FEB_RFM95_Config_t *config)
{
  if (config == NULL)
    return;

  config->frequency_hz = 915000000; /* US ISM band */
  config->tx_power_dbm = 14;
  config->bandwidth = 7; /* 125 kHz */
  config->spreading_factor = 7;
  config->coding_rate = 1; /* 4/5 */
  config->sync_word = 0x12;
  config->preamble_length = 8;
}

FEB_RFM95_Status_t FEB_RFM95_Init(const FEB_RFM95_Config_t *config)
{
  FEB_RFM95_Config_t cfg;

  if (config == NULL)
  {
    FEB_RFM95_GetDefaultConfig(&cfg);
    config = &cfg;
  }

  /* Configure the rfm95 handle with hardware pins */
  s_rfm95.spi_handle = &hspi3;
  s_rfm95.nss_port = RD_CS_GPIO_Port;
  s_rfm95.nss_pin = RD_CS_Pin;
  s_rfm95.nrst_port = RD_RST_GPIO_Port;
  s_rfm95.nrst_pin = RD_RST_Pin;
  s_rfm95.en_port = RD_EN_GPIO_Port;
  s_rfm95.en_pin = RD_EN_Pin;

  /* Copy config to rfm95 handle */
  s_rfm95.config.frequency = config->frequency_hz;
  s_rfm95.config.tx_power = config->tx_power_dbm;
  s_rfm95.config.bandwidth = config->bandwidth;
  s_rfm95.config.spreading_factor = config->spreading_factor;
  s_rfm95.config.coding_rate = config->coding_rate;
  s_rfm95.config.sync_word = config->sync_word;
  s_rfm95.config.preamble_length = config->preamble_length;
  s_rfm95.config.crc_enabled = true;

  /* Ensure CS is high before init */
  HAL_GPIO_WritePin(RD_CS_GPIO_Port, RD_CS_Pin, GPIO_PIN_SET);

  /* Initialize the rfm95 driver */
  if (!rfm95_init(&s_rfm95))
  {
    uint8_t version = rfm95_get_version(&s_rfm95);
    LOG_E(TAG, "Chip version mismatch: 0x%02X (expected 0x%02X)", version, 0x12);
    return FEB_RFM95_ERR_INIT;
  }

  /* Reset stats */
  memset(&s_stats, 0, sizeof(s_stats));
  s_initialized = true;

  LOG_I(TAG, "Version: 0x%02X", rfm95_get_version(&s_rfm95));
  LOG_I(TAG, "Initialized: freq=%lu Hz, power=%d dBm", config->frequency_hz, config->tx_power_dbm);

  return FEB_RFM95_OK;
}

/* ============================================================================
 * Public API - TX/RX Operations
 * ============================================================================ */

FEB_RFM95_Status_t FEB_RFM95_Transmit(const uint8_t *data, uint8_t length, uint32_t timeout_ms)
{
  if (!s_initialized)
    return FEB_RFM95_ERR_NOT_INITIALIZED;
  if (data == NULL || length == 0)
    return FEB_RFM95_ERR_INVALID_PARAM;

  bool success = rfm95_transmit(&s_rfm95, data, length, timeout_ms);

  if (success)
  {
    s_stats.tx_count++;
    return FEB_RFM95_OK;
  }

  s_stats.tx_errors++;
  LOG_W(TAG, "TX timeout");
  return FEB_RFM95_ERR_TX_TIMEOUT;
}

FEB_RFM95_Status_t FEB_RFM95_Receive(uint8_t *buffer, uint8_t *length, uint32_t timeout_ms)
{
  if (!s_initialized)
    return FEB_RFM95_ERR_NOT_INITIALIZED;
  if (buffer == NULL || length == NULL)
    return FEB_RFM95_ERR_INVALID_PARAM;

  size_t rx_len = 0;
  bool success = rfm95_receive(&s_rfm95, buffer, &rx_len, timeout_ms);

  if (success)
  {
    *length = (uint8_t)rx_len;
    s_stats.rx_count++;
    s_stats.last_rssi = s_rfm95.last_rssi;
    s_stats.last_snr = s_rfm95.last_snr;
    return FEB_RFM95_OK;
  }

  *length = 0;

  /* Check if it was a CRC error */
  if (s_rfm95.crc_error)
  {
    s_stats.rx_errors++;
    return FEB_RFM95_ERR_RX_CRC;
  }

  s_stats.rx_timeouts++;
  return FEB_RFM95_ERR_RX_TIMEOUT;
}

void FEB_RFM95_StartReceive(void)
{
  rfm95_start_receive(&s_rfm95);
}

void FEB_RFM95_Standby(void)
{
  rfm95_standby(&s_rfm95);
}

void FEB_RFM95_Sleep(void)
{
  rfm95_sleep(&s_rfm95);
}

/* ============================================================================
 * Public API - Interrupt Handling
 * ============================================================================ */

void FEB_RFM95_OnDIO0(void)
{
  rfm95_on_interrupt(&s_rfm95, RFM95_INTERRUPT_DIO0);

  /* Signal FreeRTOS events if available */
  if (radioEventsHandle != NULL)
  {
    if (s_rfm95.tx_done)
    {
      osEventFlagsSet(radioEventsHandle, EVT_TX_DONE);
    }
    if (s_rfm95.rx_done)
    {
      osEventFlagsSet(radioEventsHandle, EVT_RX_DONE);
    }
    if (s_rfm95.crc_error)
    {
      osEventFlagsSet(radioEventsHandle, EVT_CRC_ERROR);
    }
  }
}

void FEB_RFM95_OnDIO1(void)
{
  rfm95_on_interrupt(&s_rfm95, RFM95_INTERRUPT_DIO1);

  if (radioEventsHandle != NULL)
  {
    if (s_rfm95.rx_timeout)
    {
      osEventFlagsSet(radioEventsHandle, EVT_RX_TIMEOUT);
    }
  }
}

/* ============================================================================
 * Public API - Status & Statistics
 * ============================================================================ */

int16_t FEB_RFM95_GetRSSI(void)
{
  return s_rfm95.last_rssi;
}

int8_t FEB_RFM95_GetSNR(void)
{
  return s_rfm95.last_snr;
}

void FEB_RFM95_GetStats(FEB_RFM95_Stats_t *stats)
{
  if (stats != NULL)
  {
    memcpy(stats, &s_stats, sizeof(FEB_RFM95_Stats_t));
  }
}

void FEB_RFM95_ResetStats(void)
{
  memset(&s_stats, 0, sizeof(s_stats));
}
