/**
 * @file ADBMS6830B_Memory_Map.h
 * @brief ADBMS6830B Memory Map
 * Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ADBMS6830B.pdf
 *
 * This file defines the memory map for the ADBMS6830B battery monitor IC.
 */

#ifndef ADBMS6830B_MEMORY_MAP_H
#define ADBMS6830B_MEMORY_MAP_H

#include <stdint.h>
#include <string.h>

/*============================================================================
 * Encode/Decode Helper Macros
 *============================================================================*/

/** Register data size in bytes (all ADBMS6830B registers are 6 bytes) */
#define ADBMS_REG_SIZE 6

/** Decode little-endian 16-bit value from byte array */
#define LE16_DECODE(b) ((uint16_t)(b)[0] | ((uint16_t)(b)[1] << 8))

/** Encode 16-bit value to little-endian byte array */
#define LE16_ENCODE(v, b)                                                                                              \
  do                                                                                                                   \
  {                                                                                                                    \
    (b)[0] = (v) & 0xFF;                                                                                               \
    (b)[1] = ((v) >> 8) & 0xFF;                                                                                        \
  } while (0)

/** Decode little-endian 32-bit value from byte array */
#define LE32_DECODE(b)                                                                                                 \
  ((uint32_t)(b)[0] | ((uint32_t)(b)[1] << 8) | ((uint32_t)(b)[2] << 16) | ((uint32_t)(b)[3] << 24))

/** Encode 32-bit value to little-endian byte array */
#define LE32_ENCODE(v, b)                                                                                              \
  do                                                                                                                   \
  {                                                                                                                    \
    (b)[0] = (v) & 0xFF;                                                                                               \
    (b)[1] = ((v) >> 8) & 0xFF;                                                                                        \
    (b)[2] = ((v) >> 16) & 0xFF;                                                                                       \
    (b)[3] = ((v) >> 24) & 0xFF;                                                                                       \
  } while (0)

/*============================================================================
 * Memory Map - ADBMS6830B
 *============================================================================*/

/**
 * @brief Register structure for RDSID command (Serial ID register)
 *
 * This structure maps the bytes returned by the RDSID (Read Serial ID) command
 * from the ADBMS6830B IC. The register contains a unique 48-bit Serial ID and a
 * 6-bit Device ID.
 *
 *  - SID[0]..SID[5]: Serial Identification, 6 bytes (48 bits, LSB first)
 *  - DID:    Device Identification, 6 bits (bits [6:1] of byte 1)
 */
typedef struct __attribute__((packed))
{
  uint8_t SID[6];  /**< 48-bit unique Serial Identification number (little-endian) */
  uint8_t DID : 6; /**< 6-bit Device Identification - must equal 0b00 0011 - is part of the Serial ID */
} RDSID_t;

/** Decode RDSID register from raw bytes
 *  DID is in byte 1 bits [6:1], SID is all 6 bytes
 */
#define RDSID_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memcpy((s)->SID, (b), 6);                                                                                          \
    (s)->DID = ((b)[1] >> 1) & 0x3F;                                                                                   \
  } while (0)

/** Encode RDSID register to raw bytes (read-only register, but provided for completeness)
 *  DID is in byte 1 bits [6:1]
 */
#define RDSID_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    memcpy((b), (s)->SID, 6);                                                                                          \
    (b)[1] = ((s)->SID[1] & 0x81) | (((s)->DID & 0x3F) << 1);                                                          \
  } while (0)

/**
 * @brief Register structure for CFGA (Configuration Register Group A)
 *
 * This structure maps the bitfields within the CFGA register group A,
 * as defined in the ADBMS6830B memory map. Bitfield widths and descriptions
 * are based on the datasheet specification.
 *
 *  - CTH      : Cell Test Hysteresis control, 3 bits
 *  - REFON    : Reference On, 1 bit
 *  - FLAG_D   : Fault/Flag data, 8 bits
 *  - OWA      : Open Wire Alert bits, 3 bits
 *  - OWRNG    : Open Wire Range Select, 1 bit
 *  - SOAKON   : Soak Function Enable, 1 bit
 *  - GPO      : General Purpose Output control, 11 bits (bit 0 not used)
 *  - FC       : Fault Code, 3 bits
 *  - COMM_BK  : Communication Backup selection, 5 bits
 *  - MUTE_ST  : Mute Status, 3 bits
 *  - SNAP_ST  : Snapshot Status, 2 bits
 */
typedef struct __attribute__((packed))
{
  // CFGAR0: Byte 0
  uint8_t CTH : 3;   /**< Cell Test Hysteresis control (bits [2:0]) */
  uint8_t REFON : 1; /**< Reference On (bit [7]) */
  // CFGAR1: Byte 1
  uint8_t FLAG_D : 8; /**< Fault/Flag data (bits [7:0]) */
  // CFGAR2: Byte 2
  uint8_t OWA : 3;    /**< Open Wire Alert (bits [5:3]) */
  uint8_t OWRNG : 1;  /**< Open Wire Range Select (bit [6]) */
  uint8_t SOAKON : 1; /**< Soak Function Enable (bit [7]) */
  // CFGAR3: Byte 3 & 4
  uint16_t GPO : 11; /**< General Purpose Output control (bits 3: [7:0] 4: [1:0]) - bit 0 is GPO[1] */
  // CFGAR5: Byte 5
  uint8_t FC : 3;      /**< Fault Code (bits [2:0]) */
  uint8_t COMM_BK : 5; /**< Communication Backup selection (bit 3) */
  uint8_t MUTE_ST : 1; /**< Mute Status (bit 4) */
  uint8_t SNAP_ST : 1; /**< Snapshot Status (bit 5) */
} CFGARA_t;

/** Decode CFGARA register from raw bytes
 *  Byte layout: [0]CTH[2:0],REFON[3] [1]FLAG_D [2]OWA[2:0],OWRNG[3],SOAKON[4] [3-4]GPO[10:0] [5]FC[2:0],COMM_BK[7:3]
 */
#define CFGARA_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->CTH = (b)[0] & 0x07;                                                                                          \
    (s)->REFON = ((b)[0] >> 3) & 0x01;                                                                                 \
    (s)->FLAG_D = (b)[1];                                                                                              \
    (s)->OWA = (b)[2] & 0x07;                                                                                          \
    (s)->OWRNG = ((b)[2] >> 3) & 0x01;                                                                                 \
    (s)->SOAKON = ((b)[2] >> 4) & 0x01;                                                                                \
    (s)->GPO = LE16_DECODE(&(b)[3]) & 0x07FF;                                                                          \
    (s)->FC = (b)[5] & 0x07;                                                                                           \
    (s)->COMM_BK = ((b)[5] >> 3) & 0x1F;                                                                               \
    (s)->MUTE_ST = 0;                                                                                                  \
    (s)->SNAP_ST = 0;                                                                                                  \
  } while (0)

/** Encode CFGARA register to raw bytes */
#define CFGARA_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    (b)[0] = ((s)->CTH & 0x07) | (((s)->REFON & 0x01) << 3);                                                           \
    (b)[1] = (s)->FLAG_D;                                                                                              \
    (b)[2] = ((s)->OWA & 0x07) | (((s)->OWRNG & 0x01) << 3) | (((s)->SOAKON & 0x01) << 4);                             \
    LE16_ENCODE((s)->GPO & 0x07FF, &(b)[3]);                                                                           \
    (b)[5] = ((s)->FC & 0x07) | (((s)->COMM_BK & 0x1F) << 3);                                                          \
  } while (0)

/**
 * @brief Register structure for CFGB (Configuration Register Group B)
 *
 * This structure maps the bitfields within the CFGB register group B,
 * as defined in the ADBMS6830B memory map. Bitfield widths and descriptions
 * are based on the datasheet specification.
 *
 *  - VUV   : Undervoltage Threshold, 12 bits
 *  - VOV   : Overvoltage Threshold, 12 bits
 *  - DCTO  : Discharge Timeout, 6 bits
 *  - DTRNG : Discharge Threshold Range, 1 bit
 *  - DTMEN : Discharge Timeout Mode Enable, 1 bit
 *  - DCC   : Discharge Cell Control, 17 bits
 */
typedef struct __attribute__((packed))
{
  // CFGBR0 & 1: Byte 0 & 1
  uint16_t VUV : 12; /**< Undervoltage Threshold (bits 0: [7:0] 1: [3:0]) */
  // CFGBR1 & 2: Byte 1 & 2
  uint16_t VOV : 12; /**< Overvoltage Threshold (bits 1: [7:4] 2: [7:0]) */
  // CFGBR3: Byte 3
  uint8_t DCTO : 6;  /**< Discharge Timeout (bits [5:0]) */
  uint8_t DTRNG : 1; /**< Discharge Threshold Range (bit [6]) */
  uint8_t DTMEN : 1; /**< Discharge Timeout Mode Enable (bit [7]) */
  // CFGBR4 & 5: Byte 4 & 5
  uint32_t DCC : 17; /**< Discharge Cell Control (bits 4: [7:0] 5: [7:0]) - bit 0 is unused */
} CFGBR_t;

/** Decode CFGBR register from raw bytes
 *  Byte layout: [0-1]VUV[11:0] [1-2]VOV[11:0] [3]DCTO[5:0],DTRNG[6],DTMEN[7] [4-5]DCC[16:0]
 */
#define CFGBR_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->VUV = (uint16_t)(b)[0] | (((uint16_t)(b)[1] & 0x0F) << 8);                                                    \
    (s)->VOV = (((uint16_t)(b)[1] >> 4) & 0x0F) | ((uint16_t)(b)[2] << 4);                                             \
    (s)->DCTO = (b)[3] & 0x3F;                                                                                         \
    (s)->DTRNG = ((b)[3] >> 6) & 0x01;                                                                                 \
    (s)->DTMEN = ((b)[3] >> 7) & 0x01;                                                                                 \
    (s)->DCC = LE16_DECODE(&(b)[4]) | (((uint32_t)(b)[5] >> 7) << 16);                                                 \
  } while (0)

/** Encode CFGBR register to raw bytes */
#define CFGBR_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    (b)[0] = (s)->VUV & 0xFF;                                                                                          \
    (b)[1] = (((s)->VUV >> 8) & 0x0F) | (((s)->VOV & 0x0F) << 4);                                                      \
    (b)[2] = ((s)->VOV >> 4) & 0xFF;                                                                                   \
    (b)[3] = ((s)->DCTO & 0x3F) | (((s)->DTRNG & 0x01) << 6) | (((s)->DTMEN & 0x01) << 7);                             \
    (b)[4] = (s)->DCC & 0xFF;                                                                                          \
    (b)[5] = ((s)->DCC >> 8) & 0xFF;                                                                                   \
  } while (0)

/*============================================================================
 * Cell Voltage Registers (Tables 57-62)
 *============================================================================*/

/**
 * @brief Cell Voltage Register Group A (RDCVA) - Table 57
 *
 *  - C1V : Cell 1 Voltage, 16 bits
 *  - C2V : Cell 2 Voltage, 16 bits
 *  - C3V : Cell 3 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // CVAR0 & CVAR1: Bytes 0 & 1
  uint16_t C1V : 16; /**< Cell 1 Voltage (0: [7:0], 1: [15:8]) */
  // CVAR2 & CVAR3: Bytes 2 & 3
  uint16_t C2V : 16; /**< Cell 2 Voltage (2: [7:0], 3: [15:8]) */
  // CVAR4 & CVAR5: Bytes 4 & 5
  uint16_t C3V : 16; /**< Cell 3 Voltage (4: [7:0], 5: [15:8]) */
} RDCVA_t;

/** Decode RDCVA register from raw bytes */
#define RDCVA_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->C1V = LE16_DECODE(&(b)[0]);                                                                                   \
    (s)->C2V = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->C3V = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDCVA register to raw bytes */
#define RDCVA_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->C1V, &(b)[0]);                                                                                    \
    LE16_ENCODE((s)->C2V, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->C3V, &(b)[4]);                                                                                    \
  } while (0)

/**
 * @brief Cell Voltage Register Group B (RDCVB) - Table 58
 *
 *  - C4V : Cell 4 Voltage, 16 bits
 *  - C5V : Cell 5 Voltage, 16 bits
 *  - C6V : Cell 6 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // CVBR0 & CVBR1: Bytes 0 & 1
  uint16_t C4V : 16; /**< Cell 4 Voltage (0: [7:0], 1: [15:8]) */
  // CVBR2 & CVBR3: Bytes 2 & 3
  uint16_t C5V : 16; /**< Cell 5 Voltage (2: [7:0], 3: [15:8]) */
  // CVBR4 & CVBR5: Bytes 4 & 5
  uint16_t C6V : 16; /**< Cell 6 Voltage (4: [7:0], 5: [15:8]) */
} RDCVB_t;

/** Decode RDCVB register from raw bytes */
#define RDCVB_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->C4V = LE16_DECODE(&(b)[0]);                                                                                   \
    (s)->C5V = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->C6V = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDCVB register to raw bytes */
#define RDCVB_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->C4V, &(b)[0]);                                                                                    \
    LE16_ENCODE((s)->C5V, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->C6V, &(b)[4]);                                                                                    \
  } while (0)

/**
 * @brief Cell Voltage Register Group C (RDCVC) - Table 59
 *
 *  - C7V : Cell 7 Voltage, 16 bits
 *  - C8V : Cell 8 Voltage, 16 bits
 *  - C9V : Cell 9 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // CVCR0 & CVCR1: Bytes 0 & 1
  uint16_t C7V : 16; /**< Cell 7 Voltage (0: [7:0], 1: [15:8]) */
  // CVCR2 & CVCR3: Bytes 2 & 3
  uint16_t C8V : 16; /**< Cell 8 Voltage (2: [7:0], 3: [15:8]) */
  // CVCR4 & CVCR5: Bytes 4 & 5
  uint16_t C9V : 16; /**< Cell 9 Voltage (4: [7:0], 5: [15:8]) */
} RDCVC_t;

/** Decode RDCVC register from raw bytes */
#define RDCVC_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->C7V = LE16_DECODE(&(b)[0]);                                                                                   \
    (s)->C8V = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->C9V = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDCVC register to raw bytes */
#define RDCVC_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->C7V, &(b)[0]);                                                                                    \
    LE16_ENCODE((s)->C8V, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->C9V, &(b)[4]);                                                                                    \
  } while (0)

/**
 * @brief Cell Voltage Register Group D (RDCVD) - Table 60
 *
 *  - C10V : Cell 10 Voltage, 16 bits
 *  - C11V : Cell 11 Voltage, 16 bits
 *  - C12V : Cell 12 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // CVDR0 & CVDR1: Bytes 0 & 1
  uint16_t C10V : 16; /**< Cell 10 Voltage (0: [7:0], 1: [15:8]) */
  // CVDR2 & CVDR3: Bytes 2 & 3
  uint16_t C11V : 16; /**< Cell 11 Voltage (2: [7:0], 3: [15:8]) */
  // CVDR4 & CVDR5: Bytes 4 & 5
  uint16_t C12V : 16; /**< Cell 12 Voltage (4: [7:0], 5: [15:8]) */
} RDCVD_t;

/** Decode RDCVD register from raw bytes */
#define RDCVD_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->C10V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->C11V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->C12V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDCVD register to raw bytes */
#define RDCVD_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->C10V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->C11V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->C12V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief Cell Voltage Register Group E (RDCVE) - Table 61
 *
 *  - C13V : Cell 13 Voltage, 16 bits
 *  - C14V : Cell 14 Voltage, 16 bits
 *  - C15V : Cell 15 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // CVER0 & CVER1: Bytes 0 & 1
  uint16_t C13V : 16; /**< Cell 13 Voltage (0: [7:0], 1: [15:8]) */
  // CVER2 & CVER3: Bytes 2 & 3
  uint16_t C14V : 16; /**< Cell 14 Voltage (2: [7:0], 3: [15:8]) */
  // CVER4 & CVER5: Bytes 4 & 5
  uint16_t C15V : 16; /**< Cell 15 Voltage (4: [7:0], 5: [15:8]) */
} RDCVE_t;

/** Decode RDCVE register from raw bytes */
#define RDCVE_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->C13V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->C14V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->C15V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDCVE register to raw bytes */
#define RDCVE_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->C13V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->C14V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->C15V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief Cell Voltage Register Group F (RDCVF) - Table 62
 *
 *  - C16V    : Cell 16 Voltage, 16 bits
 *  - reserved: Reserved, 32 bits (all 1s)
 */
typedef struct __attribute__((packed))
{
  // CVFR0 & CVFR1: Bytes 0 & 1
  uint16_t C16V : 16; /**< Cell 16 Voltage (0: [7:0], 1: [15:8]) */
  // CVFR2 to CVFR5: Bytes 2 to 5
  uint32_t reserved : 32; /**< Reserved (2: [7:0], 3: [15:8], 4: [7:0], 5: [15:8]) - all 1s */
} RDCVF_t;

/** Decode RDCVF register from raw bytes */
#define RDCVF_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->C16V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->reserved = LE32_DECODE(&(b)[2]);                                                                              \
  } while (0)

/** Encode RDCVF register to raw bytes */
#define RDCVF_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0xFF, ADBMS_REG_SIZE);                                                                                 \
    LE16_ENCODE((s)->C16V, &(b)[0]);                                                                                   \
  } while (0)

/*============================================================================
 * Averaged Cell Voltage Registers (Tables 63-68)
 *============================================================================*/

/**
 * @brief Averaged Cell Voltage Register Group A (RDACA) - Table 63
 *
 *  - AC1V : Averaged Cell 1 Voltage, 16 bits
 *  - AC2V : Averaged Cell 2 Voltage, 16 bits
 *  - AC3V : Averaged Cell 3 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // ACVAR0 & ACVAR1: Bytes 0 & 1
  uint16_t AC1V : 16; /**< Averaged Cell 1 Voltage (0: [7:0], 1: [15:8]) */
  // ACVAR2 & ACVAR3: Bytes 2 & 3
  uint16_t AC2V : 16; /**< Averaged Cell 2 Voltage (2: [7:0], 3: [15:8]) */
  // ACVAR4 & ACVAR5: Bytes 4 & 5
  uint16_t AC3V : 16; /**< Averaged Cell 3 Voltage (4: [7:0], 5: [15:8]) */
} RDACA_t;

/** Decode RDACA register from raw bytes */
#define RDACA_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->AC1V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->AC2V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->AC3V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDACA register to raw bytes */
#define RDACA_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->AC1V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->AC2V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->AC3V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief Averaged Cell Voltage Register Group B (RDACB) - Table 64
 *
 *  - AC4V : Averaged Cell 4 Voltage, 16 bits
 *  - AC5V : Averaged Cell 5 Voltage, 16 bits
 *  - AC6V : Averaged Cell 6 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // ACVBR0 & ACVBR1: Bytes 0 & 1
  uint16_t AC4V : 16; /**< Averaged Cell 4 Voltage (0: [7:0], 1: [15:8]) */
  // ACVBR2 & ACVBR3: Bytes 2 & 3
  uint16_t AC5V : 16; /**< Averaged Cell 5 Voltage (2: [7:0], 3: [15:8]) */
  // ACVBR4 & ACVBR5: Bytes 4 & 5
  uint16_t AC6V : 16; /**< Averaged Cell 6 Voltage (4: [7:0], 5: [15:8]) */
} RDACB_t;

/** Decode RDACB register from raw bytes */
#define RDACB_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->AC4V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->AC5V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->AC6V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDACB register to raw bytes */
#define RDACB_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->AC4V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->AC5V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->AC6V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief Averaged Cell Voltage Register Group C (RDACC) - Table 65
 *
 *  - AC7V : Averaged Cell 7 Voltage, 16 bits
 *  - AC8V : Averaged Cell 8 Voltage, 16 bits
 *  - AC9V : Averaged Cell 9 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // ACVACR0 & ACVACR1: Bytes 0 & 1
  uint16_t AC7V : 16; /**< Averaged Cell 7 Voltage (0: [7:0], 1: [15:8]) */
  // ACVACR2 & ACVACR3: Bytes 2 & 3
  uint16_t AC8V : 16; /**< Averaged Cell 8 Voltage (2: [7:0], 3: [15:8]) */
  // ACVACR4 & ACVACR5: Bytes 4 & 5
  uint16_t AC9V : 16; /**< Averaged Cell 9 Voltage (4: [7:0], 5: [15:8]) */
} RDACC_t;

/** Decode RDACC register from raw bytes */
#define RDACC_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->AC7V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->AC8V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->AC9V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDACC register to raw bytes */
#define RDACC_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->AC7V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->AC8V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->AC9V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief Averaged Cell Voltage Register Group D (RDACD) - Table 66
 *
 *  - AC10V : Averaged Cell 10 Voltage, 16 bits
 *  - AC11V : Averaged Cell 11 Voltage, 16 bits
 *  - AC12V : Averaged Cell 12 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // ACVDR0 & ACVDR1: Bytes 0 & 1
  uint16_t AC10V : 16; /**< Averaged Cell 10 Voltage (0: [7:0], 1: [15:8]) */
  // ACVDR2 & ACVDR3: Bytes 2 & 3
  uint16_t AC11V : 16; /**< Averaged Cell 11 Voltage (2: [7:0], 3: [15:8]) */
  // ACVDR4 & ACVDR5: Bytes 4 & 5
  uint16_t AC12V : 16; /**< Averaged Cell 12 Voltage (4: [7:0], 5: [15:8]) */
} RDACD_t;

/** Decode RDACD register from raw bytes */
#define RDACD_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->AC10V = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->AC11V = LE16_DECODE(&(b)[2]);                                                                                 \
    (s)->AC12V = LE16_DECODE(&(b)[4]);                                                                                 \
  } while (0)

/** Encode RDACD register to raw bytes */
#define RDACD_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->AC10V, &(b)[0]);                                                                                  \
    LE16_ENCODE((s)->AC11V, &(b)[2]);                                                                                  \
    LE16_ENCODE((s)->AC12V, &(b)[4]);                                                                                  \
  } while (0)

/**
 * @brief Averaged Cell Voltage Register Group E (RDACE) - Table 67
 *
 *  - AC13V : Averaged Cell 13 Voltage, 16 bits
 *  - AC14V : Averaged Cell 14 Voltage, 16 bits
 *  - AC15V : Averaged Cell 15 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // ACVER0 & ACVER1: Bytes 0 & 1
  uint16_t AC13V : 16; /**< Averaged Cell 13 Voltage (0: [7:0], 1: [15:8]) */
  // ACVER2 & ACVER3: Bytes 2 & 3
  uint16_t AC14V : 16; /**< Averaged Cell 14 Voltage (2: [7:0], 3: [15:8]) */
  // ACVER4 & ACVER5: Bytes 4 & 5
  uint16_t AC15V : 16; /**< Averaged Cell 15 Voltage (4: [7:0], 5: [15:8]) */
} RDACE_t;

/** Decode RDACE register from raw bytes */
#define RDACE_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->AC13V = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->AC14V = LE16_DECODE(&(b)[2]);                                                                                 \
    (s)->AC15V = LE16_DECODE(&(b)[4]);                                                                                 \
  } while (0)

/** Encode RDACE register to raw bytes */
#define RDACE_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->AC13V, &(b)[0]);                                                                                  \
    LE16_ENCODE((s)->AC14V, &(b)[2]);                                                                                  \
    LE16_ENCODE((s)->AC15V, &(b)[4]);                                                                                  \
  } while (0)

/**
 * @brief Averaged Cell Voltage Register Group F (RDACF) - Table 68
 *
 *  - AC16V     : Averaged Cell 16 Voltage, 16 bits
 *  - reserved  : Reserved, 32 bits (all 1s)
 */
typedef struct __attribute__((packed))
{
  // ACVFR0 & ACVFR1: Bytes 0 & 1
  uint16_t AC16V : 16; /**< Averaged Cell 16 Voltage (0: [7:0], 1: [15:8]) */
  // ACVFR2, ACVFR3, ACVFR4 & ACVFR5: Bytes 2-5
  uint32_t reserved : 32; /**< Reserved (2: [7:0], 3: [15:8], 4: [7:0], 5: [15:8]) - all 1s */
} RDACF_t;

/** Decode RDACF register from raw bytes */
#define RDACF_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->AC16V = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->reserved = LE32_DECODE(&(b)[2]);                                                                              \
  } while (0)

/** Encode RDACF register to raw bytes */
#define RDACF_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0xFF, ADBMS_REG_SIZE);                                                                                 \
    LE16_ENCODE((s)->AC16V, &(b)[0]);                                                                                  \
  } while (0)

/*============================================================================
 * Filtered Cell Voltage Registers (Tables 69-74)
 *============================================================================*/

/**
 * @brief Filtered Cell Voltage Register Group A (RDFCA) - Table 69
 *
 *  - FC1V : Filtered Cell 1 Voltage, 16 bits
 *  - FC2V : Filtered Cell 2 Voltage, 16 bits
 *  - FC3V : Filtered Cell 3 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // FCVAR0 & FCVAR1: Bytes 0 & 1
  uint16_t FC1V : 16; /**< Filtered Cell 1 Voltage (0: [7:0], 1: [15:8]) */
  // FCVAR2 & FCVAR3: Bytes 2 & 3
  uint16_t FC2V : 16; /**< Filtered Cell 2 Voltage (2: [7:0], 3: [15:8]) */
  // FCVAR4 & FCVAR5: Bytes 4 & 5
  uint16_t FC3V : 16; /**< Filtered Cell 3 Voltage (4: [7:0], 5: [15:8]) */
} RDFCA_t;

/** Decode RDFCA register from raw bytes */
#define RDFCA_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->FC1V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->FC2V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->FC3V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDFCA register to raw bytes */
#define RDFCA_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->FC1V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->FC2V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->FC3V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief Filtered Cell Voltage Register Group B (RDFCB) - Table 70
 *
 *  - FC4V : Filtered Cell 4 Voltage, 16 bits
 *  - FC5V : Filtered Cell 5 Voltage, 16 bits
 *  - FC6V : Filtered Cell 6 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // FCVBR0 & FCVBR1: Bytes 0 & 1
  uint16_t FC4V : 16; /**< Filtered Cell 4 Voltage (0: [7:0], 1: [15:8]) */
  // FCVBR2 & FCVBR3: Bytes 2 & 3
  uint16_t FC5V : 16; /**< Filtered Cell 5 Voltage (2: [7:0], 3: [15:8]) */
  // FCVBR4 & FCVBR5: Bytes 4 & 5
  uint16_t FC6V : 16; /**< Filtered Cell 6 Voltage (4: [7:0], 5: [15:8]) */
} RDFCB_t;

/** Decode RDFCB register from raw bytes */
#define RDFCB_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->FC4V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->FC5V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->FC6V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDFCB register to raw bytes */
#define RDFCB_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->FC4V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->FC5V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->FC6V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief Filtered Cell Voltage Register Group C (RDFCC) - Table 71
 *
 *  - FC7V : Filtered Cell 7 Voltage, 16 bits
 *  - FC8V : Filtered Cell 8 Voltage, 16 bits
 *  - FC9V : Filtered Cell 9 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // FCVCR0 & FCVCR1: Bytes 0 & 1
  uint16_t FC7V : 16; /**< Filtered Cell 7 Voltage (0: [7:0], 1: [15:8]) */
  // FCVCR2 & FCVCR3: Bytes 2 & 3
  uint16_t FC8V : 16; /**< Filtered Cell 8 Voltage (2: [7:0], 3: [15:8]) */
  // FCVCR4 & FCVCR5: Bytes 4 & 5
  uint16_t FC9V : 16; /**< Filtered Cell 9 Voltage (4: [7:0], 5: [15:8]) */
} RDFCC_t;

/** Decode RDFCC register from raw bytes */
#define RDFCC_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->FC7V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->FC8V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->FC9V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDFCC register to raw bytes */
#define RDFCC_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->FC7V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->FC8V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->FC9V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief Filtered Cell Voltage Register Group D (RDFCD) - Table 72
 *
 *  - FC10V : Filtered Cell 10 Voltage, 16 bits
 *  - FC11V : Filtered Cell 11 Voltage, 16 bits
 *  - FC12V : Filtered Cell 12 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // FCVDR0 & FCVDR1: Bytes 0 & 1
  uint16_t FC10V : 16; /**< Filtered Cell 10 Voltage (0: [7:0], 1: [15:8]) */
  // FCVDR2 & FCVDR3: Bytes 2 & 3
  uint16_t FC11V : 16; /**< Filtered Cell 11 Voltage (2: [7:0], 3: [15:8]) */
  // FCVDR4 & FCVDR5: Bytes 4 & 5
  uint16_t FC12V : 16; /**< Filtered Cell 12 Voltage (4: [7:0], 5: [15:8]) */
} RDFCD_t;

/** Decode RDFCD register from raw bytes */
#define RDFCD_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->FC10V = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->FC11V = LE16_DECODE(&(b)[2]);                                                                                 \
    (s)->FC12V = LE16_DECODE(&(b)[4]);                                                                                 \
  } while (0)

/** Encode RDFCD register to raw bytes */
#define RDFCD_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->FC10V, &(b)[0]);                                                                                  \
    LE16_ENCODE((s)->FC11V, &(b)[2]);                                                                                  \
    LE16_ENCODE((s)->FC12V, &(b)[4]);                                                                                  \
  } while (0)

/**
 * @brief Filtered Cell Voltage Register Group E (RDFCE) - Table 73
 *
 *  - FC13V : Filtered Cell 13 Voltage, 16 bits
 *  - FC14V : Filtered Cell 14 Voltage, 16 bits
 *  - FC15V : Filtered Cell 15 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // FCVER0 & FCVER1: Bytes 0 & 1
  uint16_t FC13V : 16; /**< Filtered Cell 13 Voltage (0: [7:0], 1: [15:8]) */
  // FCVER2 & FCVER3: Bytes 2 & 3
  uint16_t FC14V : 16; /**< Filtered Cell 14 Voltage (2: [7:0], 3: [15:8]) */
  // FCVER4 & FCVER5: Bytes 4 & 5
  uint16_t FC15V : 16; /**< Filtered Cell 15 Voltage (4: [7:0], 5: [15:8]) */
} RDFCE_t;

/** Decode RDFCE register from raw bytes */
#define RDFCE_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->FC13V = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->FC14V = LE16_DECODE(&(b)[2]);                                                                                 \
    (s)->FC15V = LE16_DECODE(&(b)[4]);                                                                                 \
  } while (0)

/** Encode RDFCE register to raw bytes */
#define RDFCE_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->FC13V, &(b)[0]);                                                                                  \
    LE16_ENCODE((s)->FC14V, &(b)[2]);                                                                                  \
    LE16_ENCODE((s)->FC15V, &(b)[4]);                                                                                  \
  } while (0)

/**
 * @brief Filtered Cell Voltage Register Group F (RDFCF) - Table 74
 *
 *  - FC16V    : Filtered Cell 16 Voltage, 16 bits
 *  - reserved : Reserved, 32 bits (all 1s)
 */
typedef struct __attribute__((packed))
{
  // FCVFR0 & FCVFR1: Bytes 0 & 1
  uint16_t FC16V : 16; /**< Filtered Cell 16 Voltage (0: [7:0], 1: [15:8]) */
  // FCVFR2 to FCVFR5: Bytes 2 to 5
  uint32_t reserved : 32; /**< Reserved (2: [7:0], 3: [15:8], 4: [7:0], 5: [15:8]) - all 1s */
} RDFCF_t;

/** Decode RDFCF register from raw bytes */
#define RDFCF_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->FC16V = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->reserved = LE32_DECODE(&(b)[2]);                                                                              \
  } while (0)

/** Encode RDFCF register to raw bytes */
#define RDFCF_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0xFF, ADBMS_REG_SIZE);                                                                                 \
    LE16_ENCODE((s)->FC16V, &(b)[0]);                                                                                  \
  } while (0)

/*============================================================================
 * S-Voltage Registers (Tables 75-80)
 *============================================================================*/

/**
 * @brief S-Voltage Register Group A (RDSVA) - Table 75
 *
 *  - S1V : S-Voltage 1, 16 bits
 *  - S2V : S-Voltage 2, 16 bits
 *  - S3V : S-Voltage 3, 16 bits
 */
typedef struct __attribute__((packed))
{
  // SVAR0 & SVAR1: Bytes 0 & 1
  uint16_t S1V : 16; /**< S-Voltage 1 (0: [7:0], 1: [15:8]) */
  // SVAR2 & SVAR3: Bytes 2 & 3
  uint16_t S2V : 16; /**< S-Voltage 2 (2: [7:0], 3: [15:8]) */
  // SVAR4 & SVAR5: Bytes 4 & 5
  uint16_t S3V : 16; /**< S-Voltage 3 (4: [7:0], 5: [15:8]) */
} RDSVA_t;

/** Decode RDSVA register from raw bytes */
#define RDSVA_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->S1V = LE16_DECODE(&(b)[0]);                                                                                   \
    (s)->S2V = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->S3V = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDSVA register to raw bytes */
#define RDSVA_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->S1V, &(b)[0]);                                                                                    \
    LE16_ENCODE((s)->S2V, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->S3V, &(b)[4]);                                                                                    \
  } while (0)

/**
 * @brief S-Voltage Register Group B (RDSVB) - Table 76
 *
 *  - S4V : S-Voltage 4, 16 bits
 *  - S5V : S-Voltage 5, 16 bits
 *  - S6V : S-Voltage 6, 16 bits
 */
typedef struct __attribute__((packed))
{
  // SVBR0 & SVBR1: Bytes 0 & 1
  uint16_t S4V : 16; /**< S-Voltage 4 (0: [7:0], 1: [15:8]) */
  // SVBR2 & SVBR3: Bytes 2 & 3
  uint16_t S5V : 16; /**< S-Voltage 5 (2: [7:0], 3: [15:8]) */
  // SVBR4 & SVBR5: Bytes 4 & 5
  uint16_t S6V : 16; /**< S-Voltage 6 (4: [7:0], 5: [15:8]) */
} RDSVB_t;

/** Decode RDSVB register from raw bytes */
#define RDSVB_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->S4V = LE16_DECODE(&(b)[0]);                                                                                   \
    (s)->S5V = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->S6V = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDSVB register to raw bytes */
#define RDSVB_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->S4V, &(b)[0]);                                                                                    \
    LE16_ENCODE((s)->S5V, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->S6V, &(b)[4]);                                                                                    \
  } while (0)

/**
 * @brief S-Voltage Register Group C (RDSVC) - Table 77
 *
 *  - S7V : S-Voltage 7, 16 bits
 *  - S8V : S-Voltage 8, 16 bits
 *  - S9V : S-Voltage 9, 16 bits
 */
typedef struct __attribute__((packed))
{
  // SVCR0 & SVCR1: Bytes 0 & 1
  uint16_t S7V : 16; /**< S-Voltage 7 (0: [7:0], 1: [15:8]) */
  // SVCR2 & SVCR3: Bytes 2 & 3
  uint16_t S8V : 16; /**< S-Voltage 8 (2: [7:0], 3: [15:8]) */
  // SVCR4 & SVCR5: Bytes 4 & 5
  uint16_t S9V : 16; /**< S-Voltage 9 (4: [7:0], 5: [15:8]) */
} RDSVC_t;

/** Decode RDSVC register from raw bytes */
#define RDSVC_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->S7V = LE16_DECODE(&(b)[0]);                                                                                   \
    (s)->S8V = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->S9V = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDSVC register to raw bytes */
#define RDSVC_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->S7V, &(b)[0]);                                                                                    \
    LE16_ENCODE((s)->S8V, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->S9V, &(b)[4]);                                                                                    \
  } while (0)

/**
 * @brief S-Voltage Register Group D (RDSVD) - Table 78
 *
 *  - S10V : S-Voltage 10, 16 bits
 *  - S11V : S-Voltage 11, 16 bits
 *  - S12V : S-Voltage 12, 16 bits
 */
typedef struct __attribute__((packed))
{
  // SVDR0 & SVDR1: Bytes 0 & 1
  uint16_t S10V : 16; /**< S-Voltage 10 (0: [7:0], 1: [15:8]) */
  // SVDR2 & SVDR3: Bytes 2 & 3
  uint16_t S11V : 16; /**< S-Voltage 11 (2: [7:0], 3: [15:8]) */
  // SVDR4 & SVDR5: Bytes 4 & 5
  uint16_t S12V : 16; /**< S-Voltage 12 (4: [7:0], 5: [15:8]) */
} RDSVD_t;

/** Decode RDSVD register from raw bytes */
#define RDSVD_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->S10V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->S11V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->S12V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDSVD register to raw bytes */
#define RDSVD_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->S10V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->S11V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->S12V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief S-Voltage Register Group E (RDSVE) - Table 79
 *
 *  - S13V : S-Voltage 13, 16 bits
 *  - S14V : S-Voltage 14, 16 bits
 *  - S15V : S-Voltage 15, 16 bits
 */
typedef struct __attribute__((packed))
{
  // SVER0 & SVER1: Bytes 0 & 1
  uint16_t S13V : 16; /**< S-Voltage 13 (0: [7:0], 1: [15:8]) */
  // SVER2 & SVER3: Bytes 2 & 3
  uint16_t S14V : 16; /**< S-Voltage 14 (2: [7:0], 3: [15:8]) */
  // SVER4 & SVER5: Bytes 4 & 5
  uint16_t S15V : 16; /**< S-Voltage 15 (4: [7:0], 5: [15:8]) */
} RDSVE_t;

/** Decode RDSVE register from raw bytes */
#define RDSVE_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->S13V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->S14V = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->S15V = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDSVE register to raw bytes */
#define RDSVE_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->S13V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->S14V, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->S15V, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief S-Voltage Register Group F (RDSVF) - Table 80
 *
 *  - S16V     : S-Voltage 16, 16 bits
 *  - reserved : Reserved, 32 bits (all 1s)
 */
typedef struct __attribute__((packed))
{
  // SVFR0 & SVFR1: Bytes 0 & 1
  uint16_t S16V : 16; /**< S-Voltage 16 (0: [7:0], 1: [15:8]) */
  // SVFR2 to SVFR5: Bytes 2 to 5
  uint32_t reserved : 32; /**< Reserved (2: [7:0], 3: [15:8], 4: [7:0], 5: [15:8]) - all 1s */
} RDSVF_t;

/** Decode RDSVF register from raw bytes */
#define RDSVF_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->S16V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->reserved = LE32_DECODE(&(b)[2]);                                                                              \
  } while (0)

/** Encode RDSVF register to raw bytes */
#define RDSVF_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0xFF, ADBMS_REG_SIZE);                                                                                 \
    LE16_ENCODE((s)->S16V, &(b)[0]);                                                                                   \
  } while (0)

/*============================================================================
 * Auxiliary Registers (Tables 81-84)
 *============================================================================*/

/**
 * @brief Auxiliary Register Group A (RDAUXA) - Table 81
 *
 *  - G1V : GPIO 1 Voltage, 16 bits
 *  - G2V : GPIO 2 Voltage, 16 bits
 *  - G3V : GPIO 3 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // GPAR0 & GPAR1: Bytes 0 & 1
  uint16_t G1V : 16; /**< GPIO 1 Voltage (0: [7:0], 1: [15:8]) */
  // GPAR2 & GPAR3: Bytes 2 & 3
  uint16_t G2V : 16; /**< GPIO 2 Voltage (2: [7:0], 3: [15:8]) */
  // GPAR4 & GPAR5: Bytes 4 & 5
  uint16_t G3V : 16; /**< GPIO 3 Voltage (4: [7:0], 5: [15:8]) */
} RDAUXA_t;

/** Decode RDAUXA register from raw bytes */
#define RDAUXA_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->G1V = LE16_DECODE(&(b)[0]);                                                                                   \
    (s)->G2V = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->G3V = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDAUXA register to raw bytes */
#define RDAUXA_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->G1V, &(b)[0]);                                                                                    \
    LE16_ENCODE((s)->G2V, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->G3V, &(b)[4]);                                                                                    \
  } while (0)

/**
 * @brief Auxiliary Register Group B (RDAUXB) - Table 82
 *
 *  - G4V : GPIO 4 Voltage, 16 bits
 *  - G5V : GPIO 5 Voltage, 16 bits
 *  - G6V : GPIO 6 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // GPBR0 & GPBR1: Bytes 0 & 1
  uint16_t G4V : 16; /**< GPIO 4 Voltage (0: [7:0], 1: [15:8]) */
  // GPBR2 & GPBR3: Bytes 2 & 3
  uint16_t G5V : 16; /**< GPIO 5 Voltage (2: [7:0], 3: [15:8]) */
  // GPBR4 & GPBR5: Bytes 4 & 5
  uint16_t G6V : 16; /**< GPIO 6 Voltage (4: [7:0], 5: [15:8]) */
} RDAUXB_t;

/** Decode RDAUXB register from raw bytes */
#define RDAUXB_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->G4V = LE16_DECODE(&(b)[0]);                                                                                   \
    (s)->G5V = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->G6V = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDAUXB register to raw bytes */
#define RDAUXB_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->G4V, &(b)[0]);                                                                                    \
    LE16_ENCODE((s)->G5V, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->G6V, &(b)[4]);                                                                                    \
  } while (0)

/**
 * @brief Auxiliary Register Group C (RDAUXC) - Table 83
 *
 *  - G7V : GPIO 7 Voltage, 16 bits
 *  - G8V : GPIO 8 Voltage, 16 bits
 *  - G9V : GPIO 9 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // GPCR0 & GPCR2: Bytes 0 & 1
  uint16_t G7V : 16; /**< GPIO 7 Voltage (0: [7:0], 1: [15:8]) */
  // GPCR3 & GPCR4: Bytes 2 & 3
  uint16_t G8V : 16; /**< GPIO 8 Voltage (2: [7:0], 3: [15:8]) */
  // GPCR5 & GPCR6: Bytes 4 & 5
  uint16_t G9V : 16; /**< GPIO 9 Voltage (4: [7:0], 5: [15:8]) */
} RDAUXC_t;

/** Decode RDAUXC register from raw bytes */
#define RDAUXC_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->G7V = LE16_DECODE(&(b)[0]);                                                                                   \
    (s)->G8V = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->G9V = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDAUXC register to raw bytes */
#define RDAUXC_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->G7V, &(b)[0]);                                                                                    \
    LE16_ENCODE((s)->G8V, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->G9V, &(b)[4]);                                                                                    \
  } while (0)

/**
 * @brief Auxiliary Register Group D (RDAUXD) - Table 84
 *
 *  - G10V : GPIO 10 Voltage, 16 bits
 *  - VMV  : VM Voltage, 16 bits
 *  - VPV  : VP Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // GPDR0 & GPDR1: Bytes 0 & 1
  uint16_t G10V : 16; /**< GPIO 10 Voltage (0: [7:0], 1: [15:8]) */
  // GPDR2 & GPDR3: Bytes 2 & 3
  uint16_t VMV : 16; /**< VM Voltage (2: [7:0], 3: [15:8]) */
  // GPDR4 & GPDR5: Bytes 4 & 5
  uint16_t VPV : 16; /**< VP Voltage (4: [7:0], 5: [15:8]) */
} RDAUXD_t;

/** Decode RDAUXD register from raw bytes */
#define RDAUXD_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->G10V = LE16_DECODE(&(b)[0]);                                                                                  \
    (s)->VMV = LE16_DECODE(&(b)[2]);                                                                                   \
    (s)->VPV = LE16_DECODE(&(b)[4]);                                                                                   \
  } while (0)

/** Encode RDAUXD register to raw bytes */
#define RDAUXD_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->G10V, &(b)[0]);                                                                                   \
    LE16_ENCODE((s)->VMV, &(b)[2]);                                                                                    \
    LE16_ENCODE((s)->VPV, &(b)[4]);                                                                                    \
  } while (0)

/*============================================================================
 * Redundant Auxiliary Registers (Tables 85-88)
 *============================================================================*/

/**
 * @brief Redundant Auxiliary Register Group A (RDRAXA) - Table 85
 *
 *  - R_G1V : Redundant GPIO 1 Voltage, 16 bits
 *  - R_G2V : Redundant GPIO 2 Voltage, 16 bits
 *  - R_G3V : Redundant GPIO 3 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // RGPAR0 & RGPAR1: Bytes 0 & 1
  uint16_t R_G1V : 16; /**< Redundant GPIO 1 Voltage (0: [7:0], 1: [15:8]) */
  // RGPAR2 & RGPAR3: Bytes 2 & 3
  uint16_t R_G2V : 16; /**< Redundant GPIO 2 Voltage (2: [7:0], 3: [15:8]) */
  // RGPAR4 & RGPAR5: Bytes 4 & 5
  uint16_t R_G3V : 16; /**< Redundant GPIO 3 Voltage (4: [7:0], 5: [15:8]) */
} RDRAXA_t;

/** Decode RDRAXA register from raw bytes */
#define RDRAXA_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->R_G1V = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->R_G2V = LE16_DECODE(&(b)[2]);                                                                                 \
    (s)->R_G3V = LE16_DECODE(&(b)[4]);                                                                                 \
  } while (0)

/** Encode RDRAXA register to raw bytes */
#define RDRAXA_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->R_G1V, &(b)[0]);                                                                                  \
    LE16_ENCODE((s)->R_G2V, &(b)[2]);                                                                                  \
    LE16_ENCODE((s)->R_G3V, &(b)[4]);                                                                                  \
  } while (0)

/**
 * @brief Redundant Auxiliary Register Group B (RDRAXB) - Table 86
 *
 *  - R_G4V : Redundant GPIO 4 Voltage, 16 bits
 *  - R_G5V : Redundant GPIO 5 Voltage, 16 bits
 *  - R_G6V : Redundant GPIO 6 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // RGPBR0 & RGPBR1: Bytes 0 & 1
  uint16_t R_G4V : 16; /**< Redundant GPIO 4 Voltage (0: [7:0], 1: [15:8]) */
  // RGPBR2 & RGPBR3: Bytes 2 & 3
  uint16_t R_G5V : 16; /**< Redundant GPIO 5 Voltage (2: [7:0], 3: [15:8]) */
  // RGPBR4 & RGPBR5: Bytes 4 & 5
  uint16_t R_G6V : 16; /**< Redundant GPIO 6 Voltage (4: [7:0], 5: [15:8]) */
} RDRAXB_t;

/** Decode RDRAXB register from raw bytes */
#define RDRAXB_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->R_G4V = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->R_G5V = LE16_DECODE(&(b)[2]);                                                                                 \
    (s)->R_G6V = LE16_DECODE(&(b)[4]);                                                                                 \
  } while (0)

/** Encode RDRAXB register to raw bytes */
#define RDRAXB_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->R_G4V, &(b)[0]);                                                                                  \
    LE16_ENCODE((s)->R_G5V, &(b)[2]);                                                                                  \
    LE16_ENCODE((s)->R_G6V, &(b)[4]);                                                                                  \
  } while (0)

/**
 * @brief Redundant Auxiliary Register Group C (RDRAXC) - Table 87
 *
 *  - R_G7V : Redundant GPIO 7 Voltage, 16 bits
 *  - R_G8V : Redundant GPIO 8 Voltage, 16 bits
 *  - R_G9V : Redundant GPIO 9 Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // RGPCR0 & RGPCR1: Bytes 0 & 1
  uint16_t R_G7V : 16; /**< Redundant GPIO 7 Voltage (0: [7:0], 1: [15:8]) */
  // RGPCR2 & RGPCR3: Bytes 2 & 3
  uint16_t R_G8V : 16; /**< Redundant GPIO 8 Voltage (2: [7:0], 3: [15:8]) */
  // RGPCR4 & RGPCR5: Bytes 4 & 5
  uint16_t R_G9V : 16; /**< Redundant GPIO 9 Voltage (4: [7:0], 5: [15:8]) */
} RDRAXC_t;

/** Decode RDRAXC register from raw bytes */
#define RDRAXC_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->R_G7V = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->R_G8V = LE16_DECODE(&(b)[2]);                                                                                 \
    (s)->R_G9V = LE16_DECODE(&(b)[4]);                                                                                 \
  } while (0)

/** Encode RDRAXC register to raw bytes */
#define RDRAXC_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->R_G7V, &(b)[0]);                                                                                  \
    LE16_ENCODE((s)->R_G8V, &(b)[2]);                                                                                  \
    LE16_ENCODE((s)->R_G9V, &(b)[4]);                                                                                  \
  } while (0)

/**
 * @brief Redundant Auxiliary Register Group D (RDRAXD) - Table 88
 *
 *  - R_G10V   : Redundant GPIO 10 Voltage, 16 bits
 *  - reserved : Reserved, 32 bits (all 1s)
 */
typedef struct __attribute__((packed))
{
  // RGPDR0 & RGPDR1: Bytes 0 & 1
  uint16_t R_G10V : 16; /**< Redundant GPIO 10 Voltage (0: [7:0], 1: [15:8]) */
  // RGPDR2 to RGPDR5: Bytes 2 to 5
  uint32_t reserved : 32; /**< Reserved (2: [7:0], 3: [15:8], 4: [7:0], 5: [15:8]) - all 1s */
} RDRAXD_t;

/** Decode RDRAXD register from raw bytes */
#define RDRAXD_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->R_G10V = LE16_DECODE(&(b)[0]);                                                                                \
    (s)->reserved = LE32_DECODE(&(b)[2]);                                                                              \
  } while (0)

/** Encode RDRAXD register to raw bytes */
#define RDRAXD_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0xFF, ADBMS_REG_SIZE);                                                                                 \
    LE16_ENCODE((s)->R_G10V, &(b)[0]);                                                                                 \
  } while (0)

/*============================================================================
 * Status Registers (Tables 89-93)
 *============================================================================*/

/**
 * @brief Status Register Group A (RDSTATA) - Table 89
 *
 *  - VREF2    : Reference Voltage 2, 16 bits
 *  - ITMP     : Internal Temperature, 16 bits
 *  - reserved : Reserved, 16 bits
 */
typedef struct __attribute__((packed))
{
  // STAR0 & STAR1: Bytes 0 & 1
  uint16_t VREF2 : 16; /**< Reference Voltage 2 (0: [7:0], 1: [15:8]) */
  // STAR2 & STAR3: Bytes 2 & 3
  uint16_t ITMP : 16; /**< Internal Temperature (2: [7:0], 3: [15:8]) */
  // STAR4 & STAR5: Bytes 4 & 5
  uint16_t reserved : 16; /**< Reserved (4: [7:0], 5: [15:8]) */
} RDSTATA_t;

/** Decode RDSTATA register from raw bytes */
#define RDSTATA_DECODE(b, s)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->VREF2 = LE16_DECODE(&(b)[0]);                                                                                 \
    (s)->ITMP = LE16_DECODE(&(b)[2]);                                                                                  \
    (s)->reserved = LE16_DECODE(&(b)[4]);                                                                              \
  } while (0)

/** Encode RDSTATA register to raw bytes */
#define RDSTATA_ENCODE(s, b)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->VREF2, &(b)[0]);                                                                                  \
    LE16_ENCODE((s)->ITMP, &(b)[2]);                                                                                   \
    LE16_ENCODE((s)->reserved, &(b)[4]);                                                                               \
  } while (0)

/**
 * @brief Status Register Group B (RDSTATB) - Table 90
 *
 *  - VD   : Digital Supply Voltage, 16 bits
 *  - VA   : Analog Supply Voltage, 16 bits
 *  - VRES : VRES Voltage, 16 bits
 */
typedef struct __attribute__((packed))
{
  // STBR0 & STBR1: Bytes 0 & 1
  uint16_t VD : 16; /**< Digital Supply Voltage (0: [7:0], 1: [15:8]) */
  // STBR2 & STBR3: Bytes 2 & 3
  uint16_t VA : 16; /**< Analog Supply Voltage (2: [7:0], 3: [15:8]) */
  // STBR4 & STBR5: Bytes 4 & 5
  uint16_t VRES : 16; /**< VRES Voltage (4: [7:0], 5: [15:8]) */
} RDSTATB_t;

/** Decode RDSTATB register from raw bytes */
#define RDSTATB_DECODE(b, s)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->VD = LE16_DECODE(&(b)[0]);                                                                                    \
    (s)->VA = LE16_DECODE(&(b)[2]);                                                                                    \
    (s)->VRES = LE16_DECODE(&(b)[4]);                                                                                  \
  } while (0)

/** Encode RDSTATB register to raw bytes */
#define RDSTATB_ENCODE(s, b)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->VD, &(b)[0]);                                                                                     \
    LE16_ENCODE((s)->VA, &(b)[2]);                                                                                     \
    LE16_ENCODE((s)->VRES, &(b)[4]);                                                                                   \
  } while (0)

/**
 * @brief Status Register Group C (RDSTATC) - Table 91
 *
 *  - CS_FLT          : Cell short-fault flags, 16 bits (bit k => cell k+1, same as former CS(k+1)FLT)
 *  - CT             : Conversion counter, 11 bits (CT_high:5 + CT_low:6)
 *  - CTS            : Conversion type status, 2 bits
 *  - SMED           : S-Voltage Measurement Error Detected, 1 bit
 *  - SED            : S-Voltage Error Detected, 1 bit
 *  - CMED           : Cell Measurement Error Detected, 1 bit
 *  - CED            : Cell Error Detected, 1 bit
 *  - VD_UV          : VD Undervoltage flag, 1 bit
 *  - VD_OV          : VD Overvoltage flag, 1 bit
 *  - VA_UV          : VA Undervoltage flag, 1 bit
 *  - VA_OV          : VA Overvoltage flag, 1 bit
 *  - OSCCHK         : Oscillator Check, 1 bit
 *  - TMODCHK        : Test Mode Check, 1 bit
 *  - THSD           : Thermal Shutdown flag, 1 bit
 *  - SLEEP          : Sleep flag, 1 bit
 *  - SPIFLT         : SPI Fault, 1 bit
 *  - COMP           : Comparator flag, 1 bit
 *  - VDE            : Voltage Delta Error, 1 bit
 *  - VDEL           : Voltage Delta flag, 1 bit
 */
typedef struct __attribute__((packed))
{
  // STCR0 & STCR1: Bytes 0 & 1 — 16 cell short-fault bits (Table 91)
  uint16_t CS_FLT : 16; /**< Bits [7:0]=STCR0, [15:8]=STCR1; bit b => cell (b+1) short fault */

  // STCR2 & STCR3: Bytes 2 & 3
  /**
   * Conversion counter CT[10:0] is split across:
   *   - Byte 2, bits [4:0] (CT[10:6], MSBs)
   *   - Byte 3, bits [7:2] (CT[5:0], LSBs)
   * For software access, treat as a contiguous 11-bit value within a uint16_t.
   *
   * Layout (little-endian order):
   *   [reserved1|CT_high] [CT_low|CTS]
   *   [7:5]|[4:0]         [7:2]|[1:0]
   *
   * To extract CT[10:0] from raw bytes:
   *   CT = ((byte2 & 0x1F) << 6) | ((byte3 & 0xFC) >> 2);
   */
  uint16_t CT : 11;      /**< Conversion counter bits CT[10:0] across bytes 2 and 3 (see above for packing) */
  uint8_t CTS : 2;       /**< Conversion type status (3: [1:0]) */
  uint8_t reserved1 : 3; /**< Reserved - always 0 (2: [7:5]) */

  // STCR4: Byte 4
  uint8_t SMED : 1;  /**< S-Voltage Measurement Error Detected (4: [0]) */
  uint8_t SED : 1;   /**< S-Voltage Error Detected (4: [1]) */
  uint8_t CMED : 1;  /**< Cell Measurement Error Detected (4: [2]) */
  uint8_t CED : 1;   /**< Cell Error Detected (4: [3]) */
  uint8_t VD_UV : 1; /**< VD Undervoltage flag (4: [4]) */
  uint8_t VD_OV : 1; /**< VD Overvoltage flag (4: [5]) */
  uint8_t VA_UV : 1; /**< VA Undervoltage flag (4: [6]) */
  uint8_t VA_OV : 1; /**< VA Overvoltage flag (4: [7]) */

  // STCR5: Byte 5
  uint8_t OSCCHK : 1;  /**< Oscillator Check (5: [0]) */
  uint8_t TMODCHK : 1; /**< Test Mode Check (5: [1]) */
  uint8_t THSD : 1;    /**< Thermal Shutdown flag (5: [2]) */
  uint8_t SLEEP : 1;   /**< Sleep flag (5: [3]) */
  uint8_t SPIFLT : 1;  /**< SPI Fault (5: [4]) */
  uint8_t COMP : 1;    /**< Comparator flag (5: [5]) */
  uint8_t VDE : 1;     /**< Voltage Delta Error (5: [6]) */
  uint8_t VDEL : 1;    /**< Voltage Delta flag (5: [7]) */
} RDSTATC_t;

/** Decode RDSTATC register from raw bytes */
#define RDSTATC_DECODE(b, s)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->CS_FLT = LE16_DECODE(&(b)[0]);                                                                                \
    (s)->CT = (((uint16_t)(b)[2] & 0x1F) << 6) | (((b)[3] >> 2) & 0x3F);                                               \
    (s)->CTS = (b)[3] & 0x03;                                                                                          \
    (s)->reserved1 = ((b)[2] >> 5) & 0x07;                                                                             \
    (s)->SMED = (b)[4] & 0x01;                                                                                         \
    (s)->SED = ((b)[4] >> 1) & 0x01;                                                                                   \
    (s)->CMED = ((b)[4] >> 2) & 0x01;                                                                                  \
    (s)->CED = ((b)[4] >> 3) & 0x01;                                                                                   \
    (s)->VD_UV = ((b)[4] >> 4) & 0x01;                                                                                 \
    (s)->VD_OV = ((b)[4] >> 5) & 0x01;                                                                                 \
    (s)->VA_UV = ((b)[4] >> 6) & 0x01;                                                                                 \
    (s)->VA_OV = ((b)[4] >> 7) & 0x01;                                                                                 \
    (s)->OSCCHK = (b)[5] & 0x01;                                                                                       \
    (s)->TMODCHK = ((b)[5] >> 1) & 0x01;                                                                               \
    (s)->THSD = ((b)[5] >> 2) & 0x01;                                                                                  \
    (s)->SLEEP = ((b)[5] >> 3) & 0x01;                                                                                 \
    (s)->SPIFLT = ((b)[5] >> 4) & 0x01;                                                                                \
    (s)->COMP = ((b)[5] >> 5) & 0x01;                                                                                  \
    (s)->VDE = ((b)[5] >> 6) & 0x01;                                                                                   \
    (s)->VDEL = ((b)[5] >> 7) & 0x01;                                                                                  \
  } while (0)

/** Encode RDSTATC register to raw bytes */
#define RDSTATC_ENCODE(s, b)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    LE16_ENCODE((s)->CS_FLT, &(b)[0]);                                                                                 \
    (b)[2] = (((s)->CT >> 6) & 0x1F) | (((s)->reserved1 & 0x07) << 5);                                                 \
    (b)[3] = (((s)->CT & 0x3F) << 2) | ((s)->CTS & 0x03);                                                              \
    (b)[4] = ((s)->SMED & 0x01) | (((s)->SED & 0x01) << 1) | (((s)->CMED & 0x01) << 2) | (((s)->CED & 0x01) << 3) |    \
             (((s)->VD_UV & 0x01) << 4) | (((s)->VD_OV & 0x01) << 5) | (((s)->VA_UV & 0x01) << 6) |                    \
             (((s)->VA_OV & 0x01) << 7);                                                                               \
    (b)[5] = ((s)->OSCCHK & 0x01) | (((s)->TMODCHK & 0x01) << 1) | (((s)->THSD & 0x01) << 2) |                         \
             (((s)->SLEEP & 0x01) << 3) | (((s)->SPIFLT & 0x01) << 4) | (((s)->COMP & 0x01) << 5) |                    \
             (((s)->VDE & 0x01) << 6) | (((s)->VDEL & 0x01) << 7);                                                     \
  } while (0)

/**
 * @brief Status Register Group D (RDSTATD) - Table 92
 *
 *  - C_UV / C_OV : Cells 1–16 undervoltage / overvoltage flags (16 bits each; see members)
 *  - reserved : Reserved, 8 bits (all 1s)
 *  - OC_CNTR : Open Circuit Counter, 8 bits
 */
typedef struct __attribute__((packed))
{
  /**
   * Undervoltage flags for cells 1–16 (logical halfword; bit i == cell (i + 1) UV, same as CxUV in Table 92).
   *
   * On the wire, the IC does not send all UV bits then all OV bits. Bytes STDR0–STDR3 alternate UV and OV
   * per cell (within each byte: CnUV then CnOV, four cells per byte; byte order STDR0 .. STDR3).
   * So you cannot simply memcpy() the first four response bytes onto C_UV and expect a match.
   *
   * Build C_UV from the raw 32-bit little-endian word
   * w = STDR0 | (STDR1 << 8) | (STDR2 << 16) | (STDR3 << 24):
   *
   *   uint16_t C_UV = 0;
   *   for (unsigned i = 0; i < 16; i++) {
   *     C_UV |= (uint16_t)(((w >> (2 * i)) & 1u) << i);
   *   }
   */
  uint16_t C_UV;
  /**
   * Overvoltage flags for cells 1–16; bit i == cell (i + 1) OV (CxOV in Table 92).
   *
   * From the same interleaved wire word w as for C_UV:
   *
   *   uint16_t C_OV = 0;
   *   for (unsigned i = 0; i < 16; i++) {
   *     C_OV |= (uint16_t)(((w >> (2 * i + 1)) & 1u) << i);
   *   }
   */
  uint16_t C_OV;
  // STDR4: Byte 4
  uint8_t reserved : 8; /**< Reserved (4: [7:0]) - all 1s */
  // STDR5: Byte 5
  uint8_t OC_CNTR : 8; /**< Open Circuit Counter (5: [7:0]) */
} RDSTATD_t;

/** Decode RDSTATD register from raw bytes (de-interleaves UV/OV bits) */
#define RDSTATD_DECODE(b, s)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    uint32_t w = LE32_DECODE(b);                                                                                       \
    (s)->C_UV = 0;                                                                                                     \
    (s)->C_OV = 0;                                                                                                     \
    for (unsigned i = 0; i < 16; i++)                                                                                  \
    {                                                                                                                  \
      (s)->C_UV |= (uint16_t)(((w >> (2 * i)) & 1u) << i);                                                             \
      (s)->C_OV |= (uint16_t)(((w >> (2 * i + 1)) & 1u) << i);                                                         \
    }                                                                                                                  \
    (s)->reserved = (b)[4];                                                                                            \
    (s)->OC_CNTR = (b)[5];                                                                                             \
  } while (0)

/** Encode RDSTATD register to raw bytes (interleaves UV/OV bits) */
#define RDSTATD_ENCODE(s, b)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    uint32_t w = 0;                                                                                                    \
    for (unsigned i = 0; i < 16; i++)                                                                                  \
    {                                                                                                                  \
      w |= ((uint32_t)(((s)->C_UV >> i) & 1u) << (2 * i));                                                             \
      w |= ((uint32_t)(((s)->C_OV >> i) & 1u) << (2 * i + 1));                                                         \
    }                                                                                                                  \
    LE32_ENCODE(w, (b));                                                                                               \
    (b)[4] = (s)->reserved;                                                                                            \
    (b)[5] = (s)->OC_CNTR;                                                                                             \
  } while (0)

/**
 * @brief Status Register Group E (RDSTATE) - Table 93
 *
 *  - reserved0   : Reserved, 32 bits (all 1s)
 *  - GPI         : General Purpose Input status, 11 bits
 *  - reserved1   : Reserved, 2 bits
 *  - REV         : Revision ID, 4 bits
 */
typedef struct __attribute__((packed))
{
  // STER0: Byte 0 & 1 & 2 & 3
  uint32_t reserved0 : 32; /**< Reserved (0: [7:0], 1: [7:0], 2: [7:0], 3: [7:0]) - all 1s */
  // STER4: Byte 4 & 5
  uint16_t GPI : 11;     /**< General Purpose Input status (4: [7:0], 5: [1:0]) - bit 0 is not used */
  uint8_t reserved1 : 2; /**< Reserved (5: [3:2]) - all 0s */
  uint8_t REV : 4;       /**< Revision ID (5: [7:4]) */
} RDSTATE_t;

/** Decode RDSTATE register from raw bytes */
#define RDSTATE_DECODE(b, s)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->reserved0 = LE32_DECODE(&(b)[0]);                                                                             \
    (s)->GPI = (uint16_t)(b)[4] | (((uint16_t)(b)[5] & 0x07) << 8);                                                    \
    (s)->reserved1 = ((b)[5] >> 3) & 0x03;                                                                             \
    (s)->REV = ((b)[5] >> 4) & 0x0F;                                                                                   \
  } while (0)

/** Encode RDSTATE register to raw bytes */
#define RDSTATE_ENCODE(s, b)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0xFF, ADBMS_REG_SIZE);                                                                                 \
    (b)[4] = (s)->GPI & 0xFF;                                                                                          \
    (b)[5] = (((s)->GPI >> 8) & 0x07) | (((s)->reserved1 & 0x03) << 3) | (((s)->REV & 0x0F) << 4);                     \
  } while (0)

/*============================================================================
 * COMM Register (Table 94)
 *============================================================================*/

/**
 * @brief COMM Register Group (WRCOMM, RDCOMM) - Table 94
 *
 *  - ICOM0 : Input control byte 0, 4 bits
 *  - FCOM0 : Frame control byte 0, 4 bits
 *  - D0    : Data byte 0, 8 bits
 *  - ICOM1 : Input control byte 1, 4 bits
 *  - FCOM1 : Frame control byte 1, 4 bits
 *  - D1    : Data byte 1, 8 bits
 *  - ICOM2 : Input control byte 2, 4 bits
 *  - FCOM2 : Frame control byte 2, 4 bits
 *  - D2    : Data byte 2, 8 bits
 */
typedef struct __attribute__((packed))
{
  // COMM0: Byte 0
  uint8_t FCOM0 : 4; /**< Frame control byte 0 (0: [3:0]) */
  uint8_t ICOM0 : 4; /**< Input control byte 0 (0: [7:4]) */
  // COMM1: Byte 1
  uint8_t D0 : 8; /**< Data byte 0 (1: [7:0]) */
  // COMM2: Byte 2
  uint8_t FCOM1 : 4; /**< Frame control byte 1 (2: [3:0]) */
  uint8_t ICOM1 : 4; /**< Input control byte 1 (2: [7:4]) */
  // COMM3: Byte 3
  uint8_t D1 : 8; /**< Data byte 1 (3: [7:0]) */
  // COMM4: Byte 4
  uint8_t FCOM2 : 4; /**< Frame control byte 2 (4: [3:0]) */
  uint8_t ICOM2 : 4; /**< Input control byte 2 (4: [7:4]) */
  // COMM5: Byte 5
  uint8_t D2 : 8; /**< Data byte 2 (5: [7:0]) */
} COMM_t;

/** Decode COMM register from raw bytes */
#define COMM_DECODE(b, s)                                                                                              \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->FCOM0 = (b)[0] & 0x0F;                                                                                        \
    (s)->ICOM0 = ((b)[0] >> 4) & 0x0F;                                                                                 \
    (s)->D0 = (b)[1];                                                                                                  \
    (s)->FCOM1 = (b)[2] & 0x0F;                                                                                        \
    (s)->ICOM1 = ((b)[2] >> 4) & 0x0F;                                                                                 \
    (s)->D1 = (b)[3];                                                                                                  \
    (s)->FCOM2 = (b)[4] & 0x0F;                                                                                        \
    (s)->ICOM2 = ((b)[4] >> 4) & 0x0F;                                                                                 \
    (s)->D2 = (b)[5];                                                                                                  \
  } while (0)

/** Encode COMM register to raw bytes */
#define COMM_ENCODE(s, b)                                                                                              \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    (b)[0] = ((s)->FCOM0 & 0x0F) | (((s)->ICOM0 & 0x0F) << 4);                                                         \
    (b)[1] = (s)->D0;                                                                                                  \
    (b)[2] = ((s)->FCOM1 & 0x0F) | (((s)->ICOM1 & 0x0F) << 4);                                                         \
    (b)[3] = (s)->D1;                                                                                                  \
    (b)[4] = ((s)->FCOM2 & 0x0F) | (((s)->ICOM2 & 0x0F) << 4);                                                         \
    (b)[5] = (s)->D2;                                                                                                  \
  } while (0)

/*============================================================================
 * PWM Registers (Tables 95-96)
 *============================================================================*/

/**
 * @brief PWM Register Group A (WRPWMA, RDPWMA) - Table 95
 *
 *  - PWM1-PWM12 : PWM duty cycle for cells 1-12, 4 bits each
 */
typedef struct __attribute__((packed))
{
  // PWMR0: Byte 0
  uint8_t PWM1 : 4; /**< PWM duty cycle cell 1 (0: [3:0]) */
  uint8_t PWM2 : 4; /**< PWM duty cycle cell 2 (0: [7:4]) */
  // PWMR1: Byte 1
  uint8_t PWM3 : 4; /**< PWM duty cycle cell 3 (1: [3:0]) */
  uint8_t PWM4 : 4; /**< PWM duty cycle cell 4 (1: [7:4]) */
  // PWMR2: Byte 2
  uint8_t PWM5 : 4; /**< PWM duty cycle cell 5 (2: [3:0]) */
  uint8_t PWM6 : 4; /**< PWM duty cycle cell 6 (2: [7:4]) */
  // PWMR3: Byte 3
  uint8_t PWM7 : 4; /**< PWM duty cycle cell 7 (3: [3:0]) */
  uint8_t PWM8 : 4; /**< PWM duty cycle cell 8 (3: [7:4]) */
  // PWMR4: Byte 4
  uint8_t PWM9 : 4;  /**< PWM duty cycle cell 9 (4: [3:0]) */
  uint8_t PWM10 : 4; /**< PWM duty cycle cell 10 (4: [7:4]) */
  // PWMR5: Byte 5
  uint8_t PWM11 : 4; /**< PWM duty cycle cell 11 (5: [3:0]) */
  uint8_t PWM12 : 4; /**< PWM duty cycle cell 12 (5: [7:4]) */
} PWMA_t;

/** Decode PWMA register from raw bytes */
#define PWMA_DECODE(b, s)                                                                                              \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->PWM1 = (b)[0] & 0x0F;                                                                                         \
    (s)->PWM2 = ((b)[0] >> 4) & 0x0F;                                                                                  \
    (s)->PWM3 = (b)[1] & 0x0F;                                                                                         \
    (s)->PWM4 = ((b)[1] >> 4) & 0x0F;                                                                                  \
    (s)->PWM5 = (b)[2] & 0x0F;                                                                                         \
    (s)->PWM6 = ((b)[2] >> 4) & 0x0F;                                                                                  \
    (s)->PWM7 = (b)[3] & 0x0F;                                                                                         \
    (s)->PWM8 = ((b)[3] >> 4) & 0x0F;                                                                                  \
    (s)->PWM9 = (b)[4] & 0x0F;                                                                                         \
    (s)->PWM10 = ((b)[4] >> 4) & 0x0F;                                                                                 \
    (s)->PWM11 = (b)[5] & 0x0F;                                                                                        \
    (s)->PWM12 = ((b)[5] >> 4) & 0x0F;                                                                                 \
  } while (0)

/** Encode PWMA register to raw bytes */
#define PWMA_ENCODE(s, b)                                                                                              \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    (b)[0] = ((s)->PWM1 & 0x0F) | (((s)->PWM2 & 0x0F) << 4);                                                           \
    (b)[1] = ((s)->PWM3 & 0x0F) | (((s)->PWM4 & 0x0F) << 4);                                                           \
    (b)[2] = ((s)->PWM5 & 0x0F) | (((s)->PWM6 & 0x0F) << 4);                                                           \
    (b)[3] = ((s)->PWM7 & 0x0F) | (((s)->PWM8 & 0x0F) << 4);                                                           \
    (b)[4] = ((s)->PWM9 & 0x0F) | (((s)->PWM10 & 0x0F) << 4);                                                          \
    (b)[5] = ((s)->PWM11 & 0x0F) | (((s)->PWM12 & 0x0F) << 4);                                                         \
  } while (0)

/**
 * @brief PWM Register Group B (WRPWMB, RDPWMB) - Table 96
 *
 *  - PWM13-PWM16 : PWM duty cycle for cells 13-16, 4 bits each
 *  - reserved : Reserved bytes (32 bits; each byte all 1s when read)
 */
typedef struct __attribute__((packed))
{
  // PSR0: Byte 0
  uint8_t PWM13 : 4; /**< PWM duty cycle cell 13 (0: [3:0]) */
  uint8_t PWM14 : 4; /**< PWM duty cycle cell 14 (0: [7:4]) */
  // PSR1: Byte 1
  uint8_t PWM15 : 4; /**< PWM duty cycle cell 15 (1: [3:0]) */
  uint8_t PWM16 : 4; /**< PWM duty cycle cell 16 (1: [7:4]) */
  // PSR1: Byte 2 & 3 & 4 & 5
  uint32_t reserved : 32; /**< Reserved (2: [7:0], 3: [7:0], 4: [7:0], 5: [7:0]) - all 1s */
} PWMB_t;

/** Decode PWMB register from raw bytes */
#define PWMB_DECODE(b, s)                                                                                              \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->PWM13 = (b)[0] & 0x0F;                                                                                        \
    (s)->PWM14 = ((b)[0] >> 4) & 0x0F;                                                                                 \
    (s)->PWM15 = (b)[1] & 0x0F;                                                                                        \
    (s)->PWM16 = ((b)[1] >> 4) & 0x0F;                                                                                 \
    (s)->reserved = LE32_DECODE(&(b)[2]);                                                                              \
  } while (0)

/** Encode PWMB register to raw bytes */
#define PWMB_ENCODE(s, b)                                                                                              \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0xFF, ADBMS_REG_SIZE);                                                                                 \
    (b)[0] = ((s)->PWM13 & 0x0F) | (((s)->PWM14 & 0x0F) << 4);                                                         \
    (b)[1] = ((s)->PWM15 & 0x0F) | (((s)->PWM16 & 0x0F) << 4);                                                         \
  } while (0)

/*============================================================================
 * LPCM Registers (Tables 97-100)
 *============================================================================*/

/**
 * @brief LPCM Configuration Register Group (WRCMCFG, RDCMCFG) - Table 97
 *
 *  - CMC_TPER : Timing period, 3 bits (byte 0: [2:0])
 *  - CMC_BTM  : Bottom measurement, 1 bit (byte 0: [3])
 *  - CMC_MPER : Measurement period, 3 bits (byte 0: [6:4])
 *  - CMC_MAN  : Manual mode, 1 bit (byte 0: [7])
 *  - CMC_NDEV : Number of devices, 8 bits (byte 1: [7:0])
 *  - CMC_C    : Cell configuration, 17 bits (byte 2: [7:0], byte 3: [7:0], byte 4: [1:0])
 *  - CMC_GOE  : GPIO output enable, 3 bits (byte 4: [4:2])
 *  - CMC_DIR  : Direction, 1 bit (byte 4: [5])
 *  - CMC_G    : GPIO configuration, 11 bits (byte 4: [7:6], byte 5: [7:0])
 */
typedef struct __attribute__((packed))
{
  // CMCF0: Byte 0
  uint8_t CMC_TPER : 3; /**< Timing period (0: [2:0]) */
  uint8_t CMC_BTM : 1;  /**< Bottom measurement (0: [3]) */
  uint8_t CMC_MPER : 3; /**< Measurement period (0: [6:4]) */
  uint8_t CMC_MAN : 1;  /**< Manual mode (0: [7]) */
  // CMCF1: Byte 1
  uint8_t CMC_NDEV : 8; /**< Number of devices (1: [7:0]) */
  // CMCF2 & 2 & 4: Byte 2 & 3 & 4
  uint32_t CMC_C : 17; /**< Cell configuration (2: [7:0] 3: [7:0] 4: [1:0]) - bit 0 is not used */

  // CMCF3: Byte 4
  uint8_t CMC_GOE : 3; /**< Direction (4: [4:2]) */
  uint8_t CMC_DIR : 1; /**< Direction (4: [5]) */

  // CMCF4 & 5: Byte 4 & 5
  uint16_t CMC_G : 11; /**< GPIO configuration (4: [7:6] 5: [7:0]) - bit 0 is not used */
} CMCFG_t;

/** Decode CMCFG register from raw bytes */
#define CMCFG_DECODE(b, s)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->CMC_TPER = (b)[0] & 0x07;                                                                                     \
    (s)->CMC_BTM = ((b)[0] >> 3) & 0x01;                                                                               \
    (s)->CMC_MPER = ((b)[0] >> 4) & 0x07;                                                                              \
    (s)->CMC_MAN = ((b)[0] >> 7) & 0x01;                                                                               \
    (s)->CMC_NDEV = (b)[1];                                                                                            \
    (s)->CMC_C = (uint32_t)(b)[2] | ((uint32_t)(b)[3] << 8) | (((uint32_t)(b)[4] & 0x03) << 16);                       \
    (s)->CMC_GOE = ((b)[4] >> 2) & 0x07;                                                                               \
    (s)->CMC_DIR = ((b)[4] >> 5) & 0x01;                                                                               \
    (s)->CMC_G = (((b)[4] >> 6) & 0x03) | ((uint16_t)(b)[5] << 2);                                                     \
  } while (0)

/** Encode CMCFG register to raw bytes */
#define CMCFG_ENCODE(s, b)                                                                                             \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    (b)[0] = ((s)->CMC_TPER & 0x07) | (((s)->CMC_BTM & 0x01) << 3) | (((s)->CMC_MPER & 0x07) << 4) |                   \
             (((s)->CMC_MAN & 0x01) << 7);                                                                             \
    (b)[1] = (s)->CMC_NDEV;                                                                                            \
    (b)[2] = (s)->CMC_C & 0xFF;                                                                                        \
    (b)[3] = ((s)->CMC_C >> 8) & 0xFF;                                                                                 \
    (b)[4] = (((s)->CMC_C >> 16) & 0x03) | (((s)->CMC_GOE & 0x07) << 2) | (((s)->CMC_DIR & 0x01) << 5) |               \
             (((s)->CMC_G & 0x03) << 6);                                                                               \
    (b)[5] = ((s)->CMC_G >> 2) & 0xFF;                                                                                 \
  } while (0)

/**
 * @brief LPCM Cell Thresholds Register Group (WRCMCELLT, RDCMCELLT) - Table 98
 *
 *  - CMT_CUV : Cell undervoltage threshold, 12 bits
 *  - CMT_COV : Cell overvoltage threshold, 12 bits
 *  - CMT_CDV : Cell delta voltage threshold, 12 bits
 */
typedef struct __attribute__((packed))
{
  // CMTC0 & CMTC1: Bytes 0 & 1
  uint16_t CMT_CUV : 12; /**< Cell undervoltage threshold (0: [7:0], 1: [3:0]) */
  // CMTC1 & CMTC2: Bytes 1 & 2
  uint16_t CMT_COV : 12; /**< Cell overvoltage threshold (1: [7:4], 2: [7:0]) */
  // CMTC3 & CMTC4: Bytes 3 & 4
  uint16_t CMT_CDV : 12; /**< Cell delta voltage threshold (3: [7:0], 4: [3:0]) */
  // CMTC4 & CMTC5: Bytes 4 & 5
  uint16_t reserved : 12; /**< Reserved (4: [7:4], 5: [7:0]) - all 0s */
} CMCELLT_t;

/** Decode CMCELLT register from raw bytes */
#define CMCELLT_DECODE(b, s)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->CMT_CUV = (uint16_t)(b)[0] | (((uint16_t)(b)[1] & 0x0F) << 8);                                                \
    (s)->CMT_COV = ((uint16_t)(b)[1] >> 4) | ((uint16_t)(b)[2] << 4);                                                  \
    (s)->CMT_CDV = (uint16_t)(b)[3] | (((uint16_t)(b)[4] & 0x0F) << 8);                                                \
    (s)->reserved = ((uint16_t)(b)[4] >> 4) | ((uint16_t)(b)[5] << 4);                                                 \
  } while (0)

/** Encode CMCELLT register to raw bytes */
#define CMCELLT_ENCODE(s, b)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    (b)[0] = (s)->CMT_CUV & 0xFF;                                                                                      \
    (b)[1] = (((s)->CMT_CUV >> 8) & 0x0F) | (((s)->CMT_COV & 0x0F) << 4);                                              \
    (b)[2] = ((s)->CMT_COV >> 4) & 0xFF;                                                                               \
    (b)[3] = (s)->CMT_CDV & 0xFF;                                                                                      \
    (b)[4] = (((s)->CMT_CDV >> 8) & 0x0F) | (((s)->reserved & 0x0F) << 4);                                             \
    (b)[5] = ((s)->reserved >> 4) & 0xFF;                                                                              \
  } while (0)

/**
 * @brief LPCM GPIO Threshold Register Group (WRCMGPIOT, RDCMGPIOT) - Table 99
 *
 *  - CMT_GUV : GPIO undervoltage threshold, 12 bits
 *  - CMT_GOV : GPIO overvoltage threshold, 12 bits
 *  - CMT_GDV : GPIO delta voltage threshold, 12 bits
 */
typedef struct __attribute__((packed))
{
  // CMTG0 & CMTG1: Bytes 0 & 1
  uint16_t CMT_GUV : 12; /**< GPIO undervoltage threshold (0: [7:0], 1: [3:0]) */
  // CMTG1 & CMTG2: Bytes 1 & 2
  uint16_t CMT_GOV : 12; /**< GPIO overvoltage threshold (1: [7:4], 2: [7:0]) */
  // CMTG3 & CMTG4: Bytes 3 & 4
  uint16_t CMT_GDV : 12; /**< GPIO delta voltage threshold (3: [7:0], 4: [3:0]) */
  // CMTG4 & CMTG5: Bytes 4 & 5
  uint16_t reserved : 12; /**< Reserved (4: [7:4], 5: [7:0]) - all 0s */
} CMGPIOT_t;

/** Decode CMGPIOT register from raw bytes */
#define CMGPIOT_DECODE(b, s)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->CMT_GUV = (uint16_t)(b)[0] | (((uint16_t)(b)[1] & 0x0F) << 8);                                                \
    (s)->CMT_GOV = ((uint16_t)(b)[1] >> 4) | ((uint16_t)(b)[2] << 4);                                                  \
    (s)->CMT_GDV = (uint16_t)(b)[3] | (((uint16_t)(b)[4] & 0x0F) << 8);                                                \
    (s)->reserved = ((uint16_t)(b)[4] >> 4) | ((uint16_t)(b)[5] << 4);                                                 \
  } while (0)

/** Encode CMGPIOT register to raw bytes */
#define CMGPIOT_ENCODE(s, b)                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    (b)[0] = (s)->CMT_GUV & 0xFF;                                                                                      \
    (b)[1] = (((s)->CMT_GUV >> 8) & 0x0F) | (((s)->CMT_GOV & 0x0F) << 4);                                              \
    (b)[2] = ((s)->CMT_GOV >> 4) & 0xFF;                                                                               \
    (b)[3] = (s)->CMT_GDV & 0xFF;                                                                                      \
    (b)[4] = (((s)->CMT_GDV >> 8) & 0x0F) | (((s)->reserved & 0x0F) << 4);                                             \
    (b)[5] = ((s)->reserved >> 4) & 0xFF;                                                                              \
  } while (0)

/**
 * @brief LPCM Flags Register Group (RDCMFLAG) - Table 100
 *
 *  - CMF_CUV    : Cell undervoltage flag, 1 bit (byte 0: [0])
 *  - CMF_COV    : Cell overvoltage flag, 1 bit (byte 0: [1])
 *  - CMF_CDVN   : Cell delta voltage negative flag, 1 bit (byte 0: [2])
 *  - CMF_CDVP   : Cell delta voltage positive flag, 1 bit (byte 0: [3])
 *  - CMF_GUV    : GPIO undervoltage flag, 1 bit (byte 0: [4])
 *  - CMF_GOV    : GPIO overvoltage flag, 1 bit (byte 0: [5])
 *  - CMF_GDVN   : GPIO delta voltage negative flag, 1 bit (byte 0: [6])
 *  - CMF_GDVP   : GPIO delta voltage positive flag, 1 bit (byte 0: [7])
 *  - CMF_BTMCMP : Bottom comparator flag, 1 bit (byte 1: [0])
 *  - CMF_BTMWD  : Bottom watchdog flag, 1 bit (byte 1: [1])
 *  - CMC_EN     : LPCM enable status, 1 bit (byte 1: [7])
 *  - reserved   : Reserved (byte 1: [6:2]; bytes 2-5), all 0s when read
 */
typedef struct __attribute__((packed))
{
  // CMF0: Byte 0
  uint8_t CMF_CUV : 1;  /**< Cell undervoltage flag (0: [0]) */
  uint8_t CMF_COV : 1;  /**< Cell overvoltage flag (0: [1]) */
  uint8_t CMF_CDVN : 1; /**< Cell delta voltage negative flag (0: [2]) */
  uint8_t CMF_CDVP : 1; /**< Cell delta voltage positive flag (0: [3]) */
  uint8_t CMF_GUV : 1;  /**< GPIO undervoltage flag (0: [4]) */
  uint8_t CMF_GOV : 1;  /**< GPIO overvoltage flag (0: [5]) */
  uint8_t CMF_GDVN : 1; /**< GPIO delta voltage negative flag (0: [6]) */
  uint8_t CMF_GDVP : 1; /**< GPIO delta voltage positive flag (0: [7]) */
  // CMF1: Byte 1
  uint8_t CMF_BTMCMP : 1; /**< Bottom comparator flag (1: [0]) */
  uint8_t CMF_BTMWD : 1;  /**< Bottom watchdog flag (1: [1]) */
  uint8_t reserved1 : 5;  /**< Reserved (1: [6:2]) - all 0s */
  uint8_t CMC_EN : 1;     /**< LPCM enable status (1: [7]) */
  // CMF2 & CMF3 & CMF4 & CMF5: Bytes 2-5
  uint32_t reserved : 32; /**< Reserved (2: [7:0], 3: [7:0], 4: [7:0], 5: [7:0]) - all 0s */
} CMFLAG_t;

/** Decode CMFLAG register from raw bytes */
#define CMFLAG_DECODE(b, s)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    (s)->CMF_CUV = (b)[0] & 0x01;                                                                                      \
    (s)->CMF_COV = ((b)[0] >> 1) & 0x01;                                                                               \
    (s)->CMF_CDVN = ((b)[0] >> 2) & 0x01;                                                                              \
    (s)->CMF_CDVP = ((b)[0] >> 3) & 0x01;                                                                              \
    (s)->CMF_GUV = ((b)[0] >> 4) & 0x01;                                                                               \
    (s)->CMF_GOV = ((b)[0] >> 5) & 0x01;                                                                               \
    (s)->CMF_GDVN = ((b)[0] >> 6) & 0x01;                                                                              \
    (s)->CMF_GDVP = ((b)[0] >> 7) & 0x01;                                                                              \
    (s)->CMF_BTMCMP = (b)[1] & 0x01;                                                                                   \
    (s)->CMF_BTMWD = ((b)[1] >> 1) & 0x01;                                                                             \
    (s)->reserved1 = ((b)[1] >> 2) & 0x1F;                                                                             \
    (s)->CMC_EN = ((b)[1] >> 7) & 0x01;                                                                                \
    (s)->reserved = LE32_DECODE(&(b)[2]);                                                                              \
  } while (0)

/** Encode CMFLAG register to raw bytes (read-only, but provided for completeness) */
#define CMFLAG_ENCODE(s, b)                                                                                            \
  do                                                                                                                   \
  {                                                                                                                    \
    memset((b), 0, ADBMS_REG_SIZE);                                                                                    \
    (b)[0] = ((s)->CMF_CUV & 0x01) | (((s)->CMF_COV & 0x01) << 1) | (((s)->CMF_CDVN & 0x01) << 2) |                    \
             (((s)->CMF_CDVP & 0x01) << 3) | (((s)->CMF_GUV & 0x01) << 4) | (((s)->CMF_GOV & 0x01) << 5) |             \
             (((s)->CMF_GDVN & 0x01) << 6) | (((s)->CMF_GDVP & 0x01) << 7);                                            \
    (b)[1] = ((s)->CMF_BTMCMP & 0x01) | (((s)->CMF_BTMWD & 0x01) << 1) | (((s)->reserved1 & 0x1F) << 2) |              \
             (((s)->CMC_EN & 0x01) << 7);                                                                              \
    LE32_ENCODE((s)->reserved, &(b)[2]);                                                                               \
  } while (0)

/*============================================================================
 * Retention Register (Table 101)
 *============================================================================*/

/**
 * @brief Retention Register Group (ULRR, WRRR, RDRR) - Table 101
 *
 *  - RR : 48-bit Retention Register (6 bytes, little-endian)
 */
typedef struct __attribute__((packed))
{
  // RRR0 & RRR1 & RRR2 & RRR3 & RRR4 & RRR5: Bytes 0-5
  uint8_t RR[6]; /**< 48-bit Retention Register (0: RR[47:40], 1: RR[39:32], ..., 5: RR[7:0]) */
} RRR_t;

/** Decode RRR register from raw bytes */
#define RRR_DECODE(b, s)                                                                                               \
  do                                                                                                                   \
  {                                                                                                                    \
    memcpy((s)->RR, (b), ADBMS_REG_SIZE);                                                                              \
  } while (0)

/** Encode RRR register to raw bytes */
#define RRR_ENCODE(s, b)                                                                                               \
  do                                                                                                                   \
  {                                                                                                                    \
    memcpy((b), (s)->RR, ADBMS_REG_SIZE);                                                                              \
  } while (0)

/*============================================================================
 * Configuration Register A Enumerations (Table 102)
 *============================================================================*/

/**
 * @brief CTH: C-ADC vs S-ADC comparison voltage threshold (3-bit field)
 *
 * This threshold determines when the comparison between C-ADC and S-ADC
 * measurements triggers a fault. Lower values are more sensitive.
 */
typedef enum
{
  ADBMS_CTH_5_1_MV = 0,   /**< 5.1 mV threshold */
  ADBMS_CTH_8_1_MV = 1,   /**< 8.1 mV threshold (default) */
  ADBMS_CTH_9_MV = 2,     /**< 9 mV threshold */
  ADBMS_CTH_10_05_MV = 3, /**< 10.05 mV threshold */
  ADBMS_CTH_15_MV = 4,    /**< 15 mV threshold */
  ADBMS_CTH_19_95_MV = 5, /**< 19.95 mV threshold */
  ADBMS_CTH_25_05_MV = 6, /**< 25.05 mV threshold */
  ADBMS_CTH_40_05_MV = 7  /**< 40.05 mV threshold */
} ADBMS_CTH_t;

/**
 * @brief FLAG_D bit positions for latent fault diagnostic (8-bit field)
 *
 * Setting these flags in Status Register C for latent fault diagnostic does NOT
 * cause the ADBMS6830B to behave as if the flag was set by the actual diagnostic
 * mechanism. For example, setting THSD via FLAG_D[4] does not cause a power-on reset.
 */
typedef enum
{
  ADBMS_FLAG_D_OSC_FAST = (1 << 0), /**< Forces oscillator counter fast */
  ADBMS_FLAG_D_OSC_SLOW = (1 << 1), /**< Forces oscillator counter slow */
  ADBMS_FLAG_D_ED = (1 << 2),       /**< Forces supply error detection (ED) */
  ADBMS_FLAG_D_OV_DELTA = (1 << 3), /**< 1=selects supply OV and delta detection, 0=UV */
  ADBMS_FLAG_D_THSD = (1 << 4),     /**< Sets THSD (thermal shutdown flag) */
  ADBMS_FLAG_D_NVM_ED = (1 << 5),   /**< Forces NVM error detection (sets CED and SED) */
  ADBMS_FLAG_D_NVM_MED = (1 << 6),  /**< Forces NVM multiple error (sets CMED and SMED) */
  ADBMS_FLAG_D_TMODCHK = (1 << 7)   /**< Forces TMODCHK (test mode check) */
} ADBMS_FLAG_D_Bits_t;

/**
 * @brief FC: IIR filter parameter (3-bit field)
 *
 * Controls the IIR filter coefficient for filtered cell voltage measurements.
 * See datasheet Table 21 for detailed filter response characteristics.
 */
typedef enum
{
  ADBMS_FC_0 = 0, /**< IIR filter coefficient 0 (fastest response) */
  ADBMS_FC_1 = 1, /**< IIR filter coefficient 1 */
  ADBMS_FC_2 = 2, /**< IIR filter coefficient 2 */
  ADBMS_FC_3 = 3, /**< IIR filter coefficient 3 */
  ADBMS_FC_4 = 4, /**< IIR filter coefficient 4 */
  ADBMS_FC_5 = 5, /**< IIR filter coefficient 5 */
  ADBMS_FC_6 = 6, /**< IIR filter coefficient 6 */
  ADBMS_FC_7 = 7  /**< IIR filter coefficient 7 (slowest response, most filtering) */
} ADBMS_FC_t;

/*============================================================================
 * LPCM Configuration Enumerations (Table 112)
 *============================================================================*/

/**
 * @brief CMC_MPER: Fault monitoring measure (heartbeat) period
 *
 * Sets the period between LPCM heartbeat measurements.
 */
typedef enum
{
  ADBMS_CMC_MPER_1S = 0,  /**< 1 second */
  ADBMS_CMC_MPER_2S = 1,  /**< 2 seconds */
  ADBMS_CMC_MPER_4S = 2,  /**< 4 seconds */
  ADBMS_CMC_MPER_8S = 3,  /**< 8 seconds */
  ADBMS_CMC_MPER_12S = 4, /**< 12 seconds */
  ADBMS_CMC_MPER_16S = 5, /**< 16 seconds */
  ADBMS_CMC_MPER_32S = 6, /**< 32 seconds */
  ADBMS_CMC_MPER_1S_2 = 7 /**< 1 second (alternate encoding) */
} ADBMS_CMC_MPER_t;

/**
 * @brief CMC_TPER: Fault monitoring bridgeless timeout period
 *
 * Sets the bridgeless LPCM timeout period.
 */
typedef enum
{
  ADBMS_CMC_TPER_1_5S = 0,  /**< 1.5 seconds */
  ADBMS_CMC_TPER_3S = 1,    /**< 3 seconds */
  ADBMS_CMC_TPER_6S = 2,    /**< 6 seconds */
  ADBMS_CMC_TPER_12S = 3,   /**< 12 seconds */
  ADBMS_CMC_TPER_18S = 4,   /**< 18 seconds */
  ADBMS_CMC_TPER_24S = 5,   /**< 24 seconds */
  ADBMS_CMC_TPER_48S = 6,   /**< 48 seconds */
  ADBMS_CMC_TPER_1_5S_2 = 7 /**< 1.5 seconds (alternate encoding) */
} ADBMS_CMC_TPER_t;

/**
 * @brief CMC_GOE: LPCM interrupt to GPIO configuration
 *
 * Configures GPIO(s) as interrupt outputs for bridgeless LPCM.
 * When used as interrupts, the host must configure CMM_G[4:3] accordingly
 * to mask the selected GPIO against use as an analog input.
 * GPIOs are open-drain and require an external pull-up resistor.
 */
typedef enum
{
  ADBMS_CMC_GOE_DISABLED = 0,       /**< No GPIO outputs enabled */
  ADBMS_CMC_GOE_GPIO3_LOW = 1,      /**< GPIO3 active low (interrupt asserts low) */
  ADBMS_CMC_GOE_GPIO3_HIGH = 2,     /**< GPIO3 active high (interrupt asserts high) */
  ADBMS_CMC_GOE_GPIO4_LOW = 3,      /**< GPIO4 active low */
  ADBMS_CMC_GOE_GPIO4_HIGH = 4,     /**< GPIO4 active high */
  ADBMS_CMC_GOE_GPIO4H_GPIO3L = 5,  /**< GPIO4 active high, GPIO3 active low */
  ADBMS_CMC_GOE_GPIO4L_GPIO3L = 6,  /**< GPIO4 active low, GPIO3 active low */
  ADBMS_CMC_GOE_GPIO4H_GPIO3L_2 = 7 /**< GPIO4 active high, GPIO3 active low (alt) */
} ADBMS_CMC_GOE_t;

/*============================================================================
 * Communication Register Enumerations (Table 116)
 *============================================================================*/

/**
 * @brief ICOM: Initial communication control bits for I2C (write)
 *
 * Controls the I2C communication sequence start conditions.
 */
typedef enum
{
  ADBMS_ICOM_BLANK = 0x0, /**< Blank (no action) */
  ADBMS_ICOM_STOP = 0x1,  /**< I2C stop condition */
  ADBMS_ICOM_START = 0x6, /**< I2C start condition */
  ADBMS_ICOM_NO_TX = 0x7  /**< No transmit */
} ADBMS_ICOM_I2C_t;

/**
 * @brief ICOM: Initial communication control bits for SPI (read)
 */
typedef enum
{
  ADBMS_ICOM_SPI_CSB_LOW = 0x8,     /**< CSB low */
  ADBMS_ICOM_SPI_CSB_HIGH = 0x9,    /**< CSB high */
  ADBMS_ICOM_SPI_CSB_FALLING = 0xA, /**< CSB falling edge */
  ADBMS_ICOM_SPI_NO_TX = 0xF        /**< No transmit */
} ADBMS_ICOM_SPI_t;

/**
 * @brief FCOM: Final communication control bits for I2C (write)
 */
typedef enum
{
  ADBMS_FCOM_I2C_MACK = 0x0,      /**< Master ACK */
  ADBMS_FCOM_I2C_MNACK = 0x8,     /**< Master NACK */
  ADBMS_FCOM_I2C_MNACK_STOP = 0x9 /**< Master NACK + stop */
} ADBMS_FCOM_I2C_Write_t;

/**
 * @brief FCOM: Final communication control bits for I2C (read)
 */
typedef enum
{
  ADBMS_FCOM_I2C_ACK_MASTER = 0x0,      /**< ACK from master */
  ADBMS_FCOM_I2C_ACK_SLAVE_STOP = 0x1,  /**< ACK from slave + stop from master */
  ADBMS_FCOM_I2C_ACK_SLAVE = 0x7,       /**< ACK from slave */
  ADBMS_FCOM_I2C_NACK_SLAVE_STOP = 0x9, /**< NACK from slave + stop from master */
  ADBMS_FCOM_I2C_NACK_SLAVE = 0xF       /**< NACK from slave */
} ADBMS_FCOM_I2C_Read_t;

/**
 * @brief FCOM: Final communication control bits for SPI (read)
 */
typedef enum
{
  ADBMS_FCOM_SPI_CSB_LOW = 0x0, /**< CSB low (x000) */
  ADBMS_FCOM_SPI_CSB_HIGH = 0x9 /**< CSB high */
} ADBMS_FCOM_SPI_t;

/*============================================================================
 * Register Format Constants (Table 107)
 *============================================================================*/

/** Cell/GPIO/Aux voltage conversion: LSB = 150µV, offset = 1.5V */
#define ADBMS_VOLTAGE_LSB_UV 150     /**< LSB in microvolts */
#define ADBMS_VOLTAGE_OFFSET_MV 1500 /**< Offset in millivolts */

/** VUV/VOV threshold conversion: LSB = 2.4mV (16 * 150µV), offset = 1.5V */
#define ADBMS_THRESHOLD_LSB_UV 2400 /**< LSB in microvolts (16 * 150µV) */

/** VPV (V+ to V-) conversion: LSB = 3.75mV, offset = 37.5V */
#define ADBMS_VPV_LSB_UV 3750     /**< LSB in microvolts */
#define ADBMS_VPV_OFFSET_MV 37500 /**< Offset in millivolts */

/** ITMP (internal temperature) conversion: LSB = 20m°C, offset = -73°C */
#define ADBMS_ITMP_LSB_MDC 20       /**< LSB in milli-degrees Celsius */
#define ADBMS_ITMP_OFFSET_DC (-730) /**< Offset in deci-degrees Celsius */

#endif /* ADBMS6830B_MEMORY_MAP_H */
