/**
 * @file    FEB_RFM95.c
 * @brief   RFM95 LoRa Radio Driver Implementation
 * @author  Formula Electric @ Berkeley
 */

#include "FEB_RFM95.h"
#include "FEB_RFM95_HW.h"
#include "FEB_RFM95_Const.h"
#include "feb_log.h"
#include "cmsis_os.h"
#include <string.h>

#define TAG "[RFM95]"

/* Event flags for ISR-to-task signaling */
#define EVT_TX_DONE (1 << 0)
#define EVT_RX_DONE (1 << 1)
#define EVT_RX_TIMEOUT (1 << 2)
#define EVT_CRC_ERROR (1 << 3)

/* Module State */
static bool s_initialized = false;
static FEB_RFM95_Stats_t s_stats = {0};
static int16_t s_last_rssi = 0;
static int8_t s_last_snr = 0;

/* FreeRTOS Event Group (from freertos.c) */
extern osEventFlagsId_t radioEventsHandle;

/* ============================================================================
 * Private Functions
 * ============================================================================ */

static void set_mode(uint8_t mode)
{
  FEB_RFM95_HW_WriteRegister(RFM95_REG_OP_MODE, RFM95_MODE_LONG_RANGE | mode);
}

static void set_frequency(uint32_t freq_hz)
{
  uint64_t frf = ((uint64_t)freq_hz << 19) / 32000000;
  FEB_RFM95_HW_WriteRegister(RFM95_REG_FRF_MSB, (uint8_t)(frf >> 16));
  FEB_RFM95_HW_WriteRegister(RFM95_REG_FRF_MID, (uint8_t)(frf >> 8));
  FEB_RFM95_HW_WriteRegister(RFM95_REG_FRF_LSB, (uint8_t)(frf));
}

static void set_tx_power(int8_t power_dbm)
{
  if (power_dbm < 2)
    power_dbm = 2;
  if (power_dbm > 17)
    power_dbm = 17;
  FEB_RFM95_HW_WriteRegister(RFM95_REG_PA_CONFIG, RFM95_PA_BOOST | (power_dbm - 2));
}

static void clear_irq_flags(void)
{
  FEB_RFM95_HW_WriteRegister(RFM95_REG_IRQ_FLAGS, 0xFF);
}

/* ============================================================================
 * Public API - Initialization
 * ============================================================================ */

void FEB_RFM95_GetDefaultConfig(FEB_RFM95_Config_t *config)
{
  if (config == NULL)
    return;

  config->frequency_hz = RFM95_DEFAULT_FREQUENCY_HZ;
  config->tx_power_dbm = RFM95_DEFAULT_TX_POWER_DBM;
  config->bandwidth = RFM95_DEFAULT_BANDWIDTH;
  config->spreading_factor = RFM95_DEFAULT_SPREADING_FACTOR;
  config->coding_rate = RFM95_DEFAULT_CODING_RATE;
  config->sync_word = RFM95_DEFAULT_SYNC_WORD;
  config->preamble_length = RFM95_DEFAULT_PREAMBLE_LENGTH;
}

FEB_RFM95_Status_t FEB_RFM95_Init(const FEB_RFM95_Config_t *config)
{
  FEB_RFM95_Config_t cfg;

  if (config == NULL)
  {
    FEB_RFM95_GetDefaultConfig(&cfg);
    config = &cfg;
  }

  /* Enable module and reset */
  FEB_RFM95_HW_Enable();
  osDelay(10);
  FEB_RFM95_HW_Reset();

  /* Verify chip version */
  uint8_t version = FEB_RFM95_HW_ReadRegister(RFM95_REG_VERSION);
  if (version != RFM95_CHIP_VERSION)
  {
    LOG_E(TAG, "Chip version mismatch: 0x%02X (expected 0x%02X)", version, RFM95_CHIP_VERSION);
    return FEB_RFM95_ERR_INIT;
  }

  /* Enter sleep mode to configure LoRa */
  set_mode(RFM95_MODE_SLEEP);
  osDelay(10);

  /* Set LoRa mode */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_OP_MODE, RFM95_MODE_LONG_RANGE | RFM95_MODE_SLEEP);
  osDelay(10);

  /* Configure frequency */
  set_frequency(config->frequency_hz);

  /* Configure FIFO base addresses */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_FIFO_TX_BASE_ADDR, 0x00);
  FEB_RFM95_HW_WriteRegister(RFM95_REG_FIFO_RX_BASE_ADDR, 0x00);

  /* Set LNA boost */
  uint8_t lna = FEB_RFM95_HW_ReadRegister(RFM95_REG_LNA);
  FEB_RFM95_HW_WriteRegister(RFM95_REG_LNA, lna | 0x03);

  /* Configure modem (BW=125kHz, CR=4/5, SF=7, CRC on) */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_MODEM_CONFIG_1, config->bandwidth | config->coding_rate);
  FEB_RFM95_HW_WriteRegister(RFM95_REG_MODEM_CONFIG_2, config->spreading_factor | 0x04); /* CRC on */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_MODEM_CONFIG_3, 0x04);                            /* AGC on */

  /* Configure preamble */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_PREAMBLE_MSB, (config->preamble_length >> 8) & 0xFF);
  FEB_RFM95_HW_WriteRegister(RFM95_REG_PREAMBLE_LSB, config->preamble_length & 0xFF);

  /* Set sync word */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_SYNC_WORD, config->sync_word);

  /* Detection optimization for SF7-12 */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_DETECTION_OPTIMIZE, 0x03);
  FEB_RFM95_HW_WriteRegister(RFM95_REG_DETECTION_THRESHOLD, 0x0A);

  /* Set TX power */
  set_tx_power(config->tx_power_dbm);

  /* Enter standby */
  set_mode(RFM95_MODE_STDBY);

  /* Configure DIO mapping: DIO0 = RxDone/TxDone */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_DIO_MAPPING_1, 0x00);

  /* Reset stats */
  memset(&s_stats, 0, sizeof(s_stats));
  s_initialized = true;

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

  /* Enter standby */
  set_mode(RFM95_MODE_STDBY);

  /* Configure DIO0 for TxDone */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_DIO_MAPPING_1, 0x40);

  /* Set FIFO pointer */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_FIFO_ADDR_PTR, 0x00);

  /* Write payload */
  FEB_RFM95_HW_WriteBuffer(RFM95_REG_FIFO, data, length);

  /* Set payload length */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_PAYLOAD_LENGTH, length);

  /* Clear IRQ flags */
  clear_irq_flags();

  /* Clear pending events */
  osEventFlagsClear(radioEventsHandle, EVT_TX_DONE);

  /* Start TX */
  set_mode(RFM95_MODE_TX);

  /* Wait for TxDone */
  uint32_t flags = osEventFlagsWait(radioEventsHandle, EVT_TX_DONE, osFlagsWaitAny, timeout_ms);

  set_mode(RFM95_MODE_STDBY);

  if (flags & EVT_TX_DONE)
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

  /* Clear pending events */
  osEventFlagsClear(radioEventsHandle, EVT_RX_DONE | EVT_RX_TIMEOUT | EVT_CRC_ERROR);

  /* Start receive */
  FEB_RFM95_StartReceive();

  /* Wait for event */
  uint32_t flags =
      osEventFlagsWait(radioEventsHandle, EVT_RX_DONE | EVT_RX_TIMEOUT | EVT_CRC_ERROR, osFlagsWaitAny, timeout_ms);

  set_mode(RFM95_MODE_STDBY);

  if (flags & EVT_RX_DONE)
  {
    /* Read packet info */
    s_last_rssi = -157 + FEB_RFM95_HW_ReadRegister(RFM95_REG_PKT_RSSI_VALUE);
    s_last_snr = ((int8_t)FEB_RFM95_HW_ReadRegister(RFM95_REG_PKT_SNR_VALUE)) / 4;

    /* Read payload length */
    *length = FEB_RFM95_HW_ReadRegister(RFM95_REG_RX_NB_BYTES);

    /* Set FIFO pointer to RX current address */
    uint8_t rx_addr = FEB_RFM95_HW_ReadRegister(RFM95_REG_FIFO_RX_CURRENT_ADDR);
    FEB_RFM95_HW_WriteRegister(RFM95_REG_FIFO_ADDR_PTR, rx_addr);

    /* Read payload */
    FEB_RFM95_HW_ReadBuffer(RFM95_REG_FIFO, buffer, *length);

    s_stats.rx_count++;
    s_stats.last_rssi = s_last_rssi;
    s_stats.last_snr = s_last_snr;

    return FEB_RFM95_OK;
  }

  if (flags & EVT_CRC_ERROR)
  {
    s_stats.rx_errors++;
    *length = 0;
    return FEB_RFM95_ERR_RX_CRC;
  }

  s_stats.rx_timeouts++;
  *length = 0;
  return FEB_RFM95_ERR_RX_TIMEOUT;
}

void FEB_RFM95_StartReceive(void)
{
  set_mode(RFM95_MODE_STDBY);

  /* Configure DIO0 for RxDone */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_DIO_MAPPING_1, 0x00);

  /* Set FIFO pointer */
  FEB_RFM95_HW_WriteRegister(RFM95_REG_FIFO_ADDR_PTR, 0x00);

  /* Clear IRQ flags */
  clear_irq_flags();

  /* Enter continuous RX */
  set_mode(RFM95_MODE_RX_CONTINUOUS);
}

void FEB_RFM95_Standby(void)
{
  set_mode(RFM95_MODE_STDBY);
}

void FEB_RFM95_Sleep(void)
{
  set_mode(RFM95_MODE_SLEEP);
}

/* ============================================================================
 * Public API - Interrupt Handling
 * ============================================================================ */

void FEB_RFM95_OnDIO0(void)
{
  uint8_t irq_flags = FEB_RFM95_HW_ReadRegister(RFM95_REG_IRQ_FLAGS);

  if (irq_flags & RFM95_IRQ_TX_DONE)
  {
    clear_irq_flags();
    osEventFlagsSet(radioEventsHandle, EVT_TX_DONE);
  }

  if (irq_flags & RFM95_IRQ_RX_DONE)
  {
    if (irq_flags & RFM95_IRQ_PAYLOAD_CRC_ERROR)
    {
      clear_irq_flags();
      osEventFlagsSet(radioEventsHandle, EVT_CRC_ERROR);
    }
    else
    {
      clear_irq_flags();
      osEventFlagsSet(radioEventsHandle, EVT_RX_DONE);
    }
  }
}

void FEB_RFM95_OnDIO1(void)
{
  uint8_t irq_flags = FEB_RFM95_HW_ReadRegister(RFM95_REG_IRQ_FLAGS);

  if (irq_flags & RFM95_IRQ_RX_TIMEOUT)
  {
    clear_irq_flags();
    osEventFlagsSet(radioEventsHandle, EVT_RX_TIMEOUT);
  }
}

/* ============================================================================
 * Public API - Status & Statistics
 * ============================================================================ */

int16_t FEB_RFM95_GetRSSI(void)
{
  return s_last_rssi;
}
int8_t FEB_RFM95_GetSNR(void)
{
  return s_last_snr;
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
