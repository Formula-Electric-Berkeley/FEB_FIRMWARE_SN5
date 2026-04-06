/**
 * @file    FEB_RFM95_Const.h
 * @brief   RFM95/SX1276 Register Definitions and Constants
 * @author  Formula Electric @ Berkeley
 */

#ifndef FEB_RFM95_CONST_H
#define FEB_RFM95_CONST_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* ============================================================================
   * SX1276 Register Addresses
   * ============================================================================ */

#define RFM95_REG_FIFO 0x00
#define RFM95_REG_OP_MODE 0x01
#define RFM95_REG_FRF_MSB 0x06
#define RFM95_REG_FRF_MID 0x07
#define RFM95_REG_FRF_LSB 0x08
#define RFM95_REG_PA_CONFIG 0x09
#define RFM95_REG_PA_RAMP 0x0A
#define RFM95_REG_OCP 0x0B
#define RFM95_REG_LNA 0x0C
#define RFM95_REG_FIFO_ADDR_PTR 0x0D
#define RFM95_REG_FIFO_TX_BASE_ADDR 0x0E
#define RFM95_REG_FIFO_RX_BASE_ADDR 0x0F
#define RFM95_REG_FIFO_RX_CURRENT_ADDR 0x10
#define RFM95_REG_IRQ_FLAGS_MASK 0x11
#define RFM95_REG_IRQ_FLAGS 0x12
#define RFM95_REG_RX_NB_BYTES 0x13
#define RFM95_REG_PKT_SNR_VALUE 0x19
#define RFM95_REG_PKT_RSSI_VALUE 0x1A
#define RFM95_REG_MODEM_CONFIG_1 0x1D
#define RFM95_REG_MODEM_CONFIG_2 0x1E
#define RFM95_REG_SYMB_TIMEOUT_LSB 0x1F
#define RFM95_REG_PREAMBLE_MSB 0x20
#define RFM95_REG_PREAMBLE_LSB 0x21
#define RFM95_REG_PAYLOAD_LENGTH 0x22
#define RFM95_REG_MODEM_CONFIG_3 0x26
#define RFM95_REG_DETECTION_OPTIMIZE 0x31
#define RFM95_REG_DETECTION_THRESHOLD 0x37
#define RFM95_REG_SYNC_WORD 0x39
#define RFM95_REG_DIO_MAPPING_1 0x40
#define RFM95_REG_DIO_MAPPING_2 0x41
#define RFM95_REG_VERSION 0x42
#define RFM95_REG_PA_DAC 0x4D

  /* ============================================================================
   * Operating Modes (REG_OP_MODE)
   * ============================================================================ */

#define RFM95_MODE_LONG_RANGE 0x80
#define RFM95_MODE_SLEEP 0x00
#define RFM95_MODE_STDBY 0x01
#define RFM95_MODE_FSTX 0x02
#define RFM95_MODE_TX 0x03
#define RFM95_MODE_FSRX 0x04
#define RFM95_MODE_RX_CONTINUOUS 0x05
#define RFM95_MODE_RX_SINGLE 0x06
#define RFM95_MODE_CAD 0x07

  /* ============================================================================
   * IRQ Flags (REG_IRQ_FLAGS)
   * ============================================================================ */

#define RFM95_IRQ_RX_TIMEOUT 0x80
#define RFM95_IRQ_RX_DONE 0x40
#define RFM95_IRQ_PAYLOAD_CRC_ERROR 0x20
#define RFM95_IRQ_VALID_HEADER 0x10
#define RFM95_IRQ_TX_DONE 0x08
#define RFM95_IRQ_CAD_DONE 0x04
#define RFM95_IRQ_FHSS_CHANGE 0x02
#define RFM95_IRQ_CAD_DETECTED 0x01

  /* ============================================================================
   * PA Config
   * ============================================================================ */

#define RFM95_PA_BOOST 0x80

  /* ============================================================================
   * Default Configuration
   * ============================================================================ */

#define RFM95_DEFAULT_FREQUENCY_HZ 915000000 /* US ISM band */
#define RFM95_DEFAULT_TX_POWER_DBM 14
#define RFM95_DEFAULT_BANDWIDTH 0x70        /* 125 kHz */
#define RFM95_DEFAULT_CODING_RATE 0x02      /* 4/5 */
#define RFM95_DEFAULT_SPREADING_FACTOR 0x70 /* SF7 */
#define RFM95_DEFAULT_SYNC_WORD 0x12        /* Private network */
#define RFM95_DEFAULT_PREAMBLE_LENGTH 8

#define RFM95_CHIP_VERSION 0x12

#define RFM95_MAX_PAYLOAD_LENGTH 255
#define RFM95_SPI_TIMEOUT_MS 10

#ifdef __cplusplus
}
#endif

#endif /* FEB_RFM95_CONST_H */
