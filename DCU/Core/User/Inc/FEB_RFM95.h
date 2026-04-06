/**
 * @file    FEB_RFM95.h
 * @brief   RFM95 LoRa Radio Driver Interface
 * @author  Formula Electric @ Berkeley
 *
 * Expandable driver for raw LoRa peer-to-peer communication.
 * Supports configurable frequency, bandwidth, spreading factor, and TX power.
 */

#ifndef FEB_RFM95_H
#define FEB_RFM95_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdint.h>

  /* ============================================================================
   * Status Codes
   * ============================================================================ */

  typedef enum
  {
    FEB_RFM95_OK = 0,
    FEB_RFM95_ERR_INIT,
    FEB_RFM95_ERR_NOT_INITIALIZED,
    FEB_RFM95_ERR_TX_FAILED,
    FEB_RFM95_ERR_TX_TIMEOUT,
    FEB_RFM95_ERR_RX_TIMEOUT,
    FEB_RFM95_ERR_RX_CRC,
    FEB_RFM95_ERR_INVALID_PARAM
  } FEB_RFM95_Status_t;

  /* ============================================================================
   * Configuration Structure
   * ============================================================================ */

  typedef struct
  {
    uint32_t frequency_hz;    /**< Carrier frequency in Hz */
    int8_t tx_power_dbm;      /**< TX power in dBm (2-17) */
    uint8_t bandwidth;        /**< Bandwidth setting (see datasheet) */
    uint8_t spreading_factor; /**< Spreading factor (6-12) */
    uint8_t coding_rate;      /**< Coding rate (1=4/5, 2=4/6, 3=4/7, 4=4/8) */
    uint8_t sync_word;        /**< Sync word for network isolation */
    uint16_t preamble_length; /**< Preamble length in symbols */
  } FEB_RFM95_Config_t;

  /* ============================================================================
   * Statistics Structure
   * ============================================================================ */

  typedef struct
  {
    uint32_t tx_count;
    uint32_t tx_errors;
    uint32_t rx_count;
    uint32_t rx_errors;
    uint32_t rx_timeouts;
    int16_t last_rssi;
    int8_t last_snr;
  } FEB_RFM95_Stats_t;

  /* ============================================================================
   * Public API - Initialization
   * ============================================================================ */

  /**
   * @brief Get default configuration
   * @param config Pointer to config structure to fill
   */
  void FEB_RFM95_GetDefaultConfig(FEB_RFM95_Config_t *config);

  /**
   * @brief Initialize the RFM95 module
   * @param config Configuration (NULL for defaults)
   * @return Status code
   */
  FEB_RFM95_Status_t FEB_RFM95_Init(const FEB_RFM95_Config_t *config);

  /* ============================================================================
   * Public API - TX/RX Operations
   * ============================================================================ */

  /**
   * @brief Transmit data (blocking)
   * @param data Data buffer
   * @param length Data length (max 255)
   * @param timeout_ms Timeout in milliseconds
   * @return Status code
   */
  FEB_RFM95_Status_t FEB_RFM95_Transmit(const uint8_t *data, uint8_t length, uint32_t timeout_ms);

  /**
   * @brief Receive data (blocking)
   * @param buffer Buffer to store received data
   * @param length Pointer to store received length
   * @param timeout_ms Timeout in milliseconds
   * @return Status code
   */
  FEB_RFM95_Status_t FEB_RFM95_Receive(uint8_t *buffer, uint8_t *length, uint32_t timeout_ms);

  /**
   * @brief Start continuous receive mode (non-blocking)
   */
  void FEB_RFM95_StartReceive(void);

  /**
   * @brief Enter standby mode
   */
  void FEB_RFM95_Standby(void);

  /**
   * @brief Enter sleep mode
   */
  void FEB_RFM95_Sleep(void);

  /* ============================================================================
   * Public API - Interrupt Handling
   * ============================================================================ */

  /**
   * @brief DIO0 interrupt handler (call from GPIO EXTI callback)
   */
  void FEB_RFM95_OnDIO0(void);

  /**
   * @brief DIO1 interrupt handler (call from GPIO EXTI callback)
   */
  void FEB_RFM95_OnDIO1(void);

  /* ============================================================================
   * Public API - Status & Statistics
   * ============================================================================ */

  /**
   * @brief Get last packet RSSI
   * @return RSSI in dBm
   */
  int16_t FEB_RFM95_GetRSSI(void);

  /**
   * @brief Get last packet SNR
   * @return SNR in dB
   */
  int8_t FEB_RFM95_GetSNR(void);

  /**
   * @brief Get statistics
   * @param stats Pointer to stats structure to fill
   */
  void FEB_RFM95_GetStats(FEB_RFM95_Stats_t *stats);

  /**
   * @brief Reset statistics
   */
  void FEB_RFM95_ResetStats(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_RFM95_H */
