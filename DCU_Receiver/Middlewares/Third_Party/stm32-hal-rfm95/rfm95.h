/**
 * @file    rfm95.h
 * @brief   RFM95W LoRa Radio Driver (Simplified - No LoRaWAN)
 *
 * Based on stm32-hal-rfm95 by Henri Heimann, stripped of LoRaWAN functionality
 * for basic peer-to-peer LoRa communication.
 *
 * @see     https://github.com/henriheimann/stm32-hal-rfm95
 */

#ifndef RFM95_H
#define RFM95_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* STM32 HAL includes based on target MCU */
#if defined(STM32F446xx) || defined(STM32F405xx) || defined(STM32F407xx) || \
    defined(STM32F411xE) || defined(STM32F401xC) || defined(STM32F401xE)
#include "stm32f4xx_hal.h"
#else
#error "Unsupported MCU - add HAL include for your platform"
#endif

/* ============================================================================
 * Configuration Defaults
 * ============================================================================ */

#ifndef RFM95_SPI_TIMEOUT
#define RFM95_SPI_TIMEOUT 100
#endif

#ifndef RFM95_DEFAULT_FREQUENCY
#define RFM95_DEFAULT_FREQUENCY 915000000  /* US ISM band */
#endif

#ifndef RFM95_DEFAULT_TX_POWER
#define RFM95_DEFAULT_TX_POWER 14  /* dBm */
#endif

#ifndef RFM95_DEFAULT_SPREADING_FACTOR
#define RFM95_DEFAULT_SPREADING_FACTOR 7
#endif

#ifndef RFM95_DEFAULT_BANDWIDTH
#define RFM95_DEFAULT_BANDWIDTH 7  /* 125 kHz */
#endif

#ifndef RFM95_DEFAULT_CODING_RATE
#define RFM95_DEFAULT_CODING_RATE 1  /* 4/5 */
#endif

/* ============================================================================
 * Register Definitions
 * ============================================================================ */

#define RFM95_REG_FIFO                   0x00
#define RFM95_REG_OP_MODE                0x01
#define RFM95_REG_FRF_MSB                0x06
#define RFM95_REG_FRF_MID                0x07
#define RFM95_REG_FRF_LSB                0x08
#define RFM95_REG_PA_CONFIG              0x09
#define RFM95_REG_PA_RAMP                0x0A
#define RFM95_REG_OCP                    0x0B
#define RFM95_REG_LNA                    0x0C
#define RFM95_REG_FIFO_ADDR_PTR          0x0D
#define RFM95_REG_FIFO_TX_BASE_ADDR      0x0E
#define RFM95_REG_FIFO_RX_BASE_ADDR      0x0F
#define RFM95_REG_FIFO_RX_CURRENT_ADDR   0x10
#define RFM95_REG_IRQ_FLAGS_MASK         0x11
#define RFM95_REG_IRQ_FLAGS              0x12
#define RFM95_REG_RX_NB_BYTES            0x13
#define RFM95_REG_RX_HEADER_CNT_MSB      0x14
#define RFM95_REG_RX_HEADER_CNT_LSB      0x15
#define RFM95_REG_RX_PACKET_CNT_MSB      0x16
#define RFM95_REG_RX_PACKET_CNT_LSB      0x17
#define RFM95_REG_MODEM_STAT             0x18
#define RFM95_REG_PKT_SNR_VALUE          0x19
#define RFM95_REG_PKT_RSSI_VALUE         0x1A
#define RFM95_REG_RSSI_VALUE             0x1B
#define RFM95_REG_HOP_CHANNEL            0x1C
#define RFM95_REG_MODEM_CONFIG_1         0x1D
#define RFM95_REG_MODEM_CONFIG_2         0x1E
#define RFM95_REG_SYMB_TIMEOUT_LSB       0x1F
#define RFM95_REG_PREAMBLE_MSB           0x20
#define RFM95_REG_PREAMBLE_LSB           0x21
#define RFM95_REG_PAYLOAD_LENGTH         0x22
#define RFM95_REG_MAX_PAYLOAD_LENGTH     0x23
#define RFM95_REG_HOP_PERIOD             0x24
#define RFM95_REG_FIFO_RX_BYTE_ADDR      0x25
#define RFM95_REG_MODEM_CONFIG_3         0x26
#define RFM95_REG_DETECTION_OPTIMIZE     0x31
#define RFM95_REG_INVERT_IQ              0x33
#define RFM95_REG_DETECTION_THRESHOLD    0x37
#define RFM95_REG_SYNC_WORD              0x39
#define RFM95_REG_DIO_MAPPING_1          0x40
#define RFM95_REG_DIO_MAPPING_2          0x41
#define RFM95_REG_VERSION                0x42
#define RFM95_REG_PA_DAC                 0x4D

/* Operating modes */
#define RFM95_MODE_LONG_RANGE_MODE       0x80
#define RFM95_MODE_SLEEP                 0x00
#define RFM95_MODE_STDBY                 0x01
#define RFM95_MODE_FSTX                  0x02
#define RFM95_MODE_TX                    0x03
#define RFM95_MODE_FSRX                  0x04
#define RFM95_MODE_RX_CONTINUOUS         0x05
#define RFM95_MODE_RX_SINGLE             0x06
#define RFM95_MODE_CAD                   0x07

/* IRQ flags */
#define RFM95_IRQ_RX_TIMEOUT             0x80
#define RFM95_IRQ_RX_DONE                0x40
#define RFM95_IRQ_PAYLOAD_CRC_ERROR      0x20
#define RFM95_IRQ_VALID_HEADER           0x10
#define RFM95_IRQ_TX_DONE                0x08
#define RFM95_IRQ_CAD_DONE               0x04
#define RFM95_IRQ_FHSS_CHANGE_CHANNEL    0x02
#define RFM95_IRQ_CAD_DETECTED           0x01

/* PA config */
#define RFM95_PA_BOOST                   0x80

/* Expected chip version */
#define RFM95_CHIP_VERSION               0x12

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Interrupt sources
 */
typedef enum {
    RFM95_INTERRUPT_DIO0 = 0,
    RFM95_INTERRUPT_DIO1,
    RFM95_INTERRUPT_DIO5
} rfm95_interrupt_t;

/**
 * @brief Radio configuration
 */
typedef struct {
    uint32_t frequency;         /**< Carrier frequency in Hz */
    int8_t   tx_power;          /**< TX power in dBm (2-20) */
    uint8_t  spreading_factor;  /**< Spreading factor (6-12) */
    uint8_t  bandwidth;         /**< Bandwidth (0-9, see datasheet) */
    uint8_t  coding_rate;       /**< Coding rate (1-4 for 4/5 to 4/8) */
    uint8_t  sync_word;         /**< Sync word for network isolation */
    uint16_t preamble_length;   /**< Preamble length in symbols */
    bool     crc_enabled;       /**< Enable CRC checking */
} rfm95_config_t;

/**
 * @brief Handle structure for RFM95 transceiver
 */
typedef struct {
    /* Hardware interface */
    SPI_HandleTypeDef *spi_handle;   /**< SPI peripheral handle */
    GPIO_TypeDef      *nss_port;     /**< NSS (chip select) GPIO port */
    uint16_t           nss_pin;      /**< NSS GPIO pin */
    GPIO_TypeDef      *nrst_port;    /**< NRST (reset) GPIO port */
    uint16_t           nrst_pin;     /**< NRST GPIO pin */
    GPIO_TypeDef      *en_port;      /**< Enable GPIO port (optional, can be NULL) */
    uint16_t           en_pin;       /**< Enable GPIO pin */

    /* Configuration */
    rfm95_config_t     config;       /**< Current radio configuration */

    /* State */
    volatile bool      tx_done;      /**< TX complete flag (set by interrupt) */
    volatile bool      rx_done;      /**< RX complete flag (set by interrupt) */
    volatile bool      rx_timeout;   /**< RX timeout flag (set by interrupt) */
    volatile bool      crc_error;    /**< CRC error flag (set by interrupt) */

    /* Last packet info */
    int16_t            last_rssi;    /**< RSSI of last received packet */
    int8_t             last_snr;     /**< SNR of last received packet */
} rfm95_handle_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @param config Pointer to config structure to fill
 */
void rfm95_get_default_config(rfm95_config_t *config);

/**
 * @brief Initialize the RFM95 module
 * @param handle Pointer to handle structure (must be pre-configured with hardware info)
 * @return true on success, false on failure
 */
bool rfm95_init(rfm95_handle_t *handle);

/**
 * @brief Set TX power
 * @param handle Pointer to handle
 * @param power TX power in dBm (2-20)
 * @return true on success
 */
bool rfm95_set_power(rfm95_handle_t *handle, int8_t power);

/**
 * @brief Set frequency
 * @param handle Pointer to handle
 * @param frequency Frequency in Hz
 * @return true on success
 */
bool rfm95_set_frequency(rfm95_handle_t *handle, uint32_t frequency);

/**
 * @brief Transmit data (blocking with timeout)
 * @param handle Pointer to handle
 * @param data Data buffer to transmit
 * @param len Length of data (max 255)
 * @param timeout_ms Timeout in milliseconds
 * @return true on success, false on timeout or error
 */
bool rfm95_transmit(rfm95_handle_t *handle, const uint8_t *data, size_t len, uint32_t timeout_ms);

/**
 * @brief Receive data (blocking with timeout)
 * @param handle Pointer to handle
 * @param buffer Buffer to store received data
 * @param len Pointer to store received length (input: buffer size)
 * @param timeout_ms Timeout in milliseconds
 * @return true on success, false on timeout or CRC error
 */
bool rfm95_receive(rfm95_handle_t *handle, uint8_t *buffer, size_t *len, uint32_t timeout_ms);

/**
 * @brief Start continuous receive mode (non-blocking)
 * @param handle Pointer to handle
 */
void rfm95_start_receive(rfm95_handle_t *handle);

/**
 * @brief Enter standby mode
 * @param handle Pointer to handle
 */
void rfm95_standby(rfm95_handle_t *handle);

/**
 * @brief Enter sleep mode
 * @param handle Pointer to handle
 */
void rfm95_sleep(rfm95_handle_t *handle);

/**
 * @brief Hardware reset
 * @param handle Pointer to handle
 */
void rfm95_reset(rfm95_handle_t *handle);

/**
 * @brief Interrupt handler - call from EXTI callback
 * @param handle Pointer to handle
 * @param interrupt Which DIO pin triggered
 */
void rfm95_on_interrupt(rfm95_handle_t *handle, rfm95_interrupt_t interrupt);

/**
 * @brief Get RSSI of last received packet
 * @param handle Pointer to handle
 * @return RSSI in dBm
 */
int16_t rfm95_get_rssi(rfm95_handle_t *handle);

/**
 * @brief Get SNR of last received packet
 * @param handle Pointer to handle
 * @return SNR in dB
 */
int8_t rfm95_get_snr(rfm95_handle_t *handle);

/**
 * @brief Read chip version register
 * @param handle Pointer to handle
 * @return Chip version (should be 0x12 for RFM95)
 */
uint8_t rfm95_get_version(rfm95_handle_t *handle);

/* ============================================================================
 * Low-level register access (for debugging)
 * ============================================================================ */

/**
 * @brief Read a single register
 * @param handle Pointer to handle
 * @param reg Register address
 * @return Register value
 */
uint8_t rfm95_read_register(rfm95_handle_t *handle, uint8_t reg);

/**
 * @brief Write a single register
 * @param handle Pointer to handle
 * @param reg Register address
 * @param value Value to write
 */
void rfm95_write_register(rfm95_handle_t *handle, uint8_t reg, uint8_t value);

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

/**
 * @brief Test SPI using full-duplex TransmitReceive (polling mode)
 * @param handle Pointer to handle
 * @return Version byte read (0x12 expected)
 */
uint8_t rfm95_debug_spi_poll(rfm95_handle_t *handle);

/**
 * @brief Test SPI using separate Transmit + Receive calls
 * @param handle Pointer to handle
 * @return Version byte read (0x12 expected)
 */
uint8_t rfm95_debug_spi_separate(rfm95_handle_t *handle);

/**
 * @brief Test SPI using direct register access (bypasses internal functions)
 * @param handle Pointer to handle
 * @return Version byte read (0x12 expected)
 */
uint8_t rfm95_debug_spi_raw(rfm95_handle_t *handle);

/**
 * @brief Print GPIO pin states for debugging
 * @param handle Pointer to handle
 */
void rfm95_debug_gpio_status(rfm95_handle_t *handle);

/**
 * @brief Perform hardware reset sequence with logging
 * @param handle Pointer to handle
 */
void rfm95_debug_reset(rfm95_handle_t *handle);

/**
 * @brief Toggle EN pin and test response
 * @param handle Pointer to handle
 */
void rfm95_debug_enable(rfm95_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* RFM95_H */
