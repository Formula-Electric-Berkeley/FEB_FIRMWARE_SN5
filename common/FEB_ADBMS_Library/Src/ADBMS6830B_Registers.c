/**
 * @file ADBMS6830B_Registers.c
 * @brief Complete ADBMS6830B Register Interface Implementation
 *
 * Self-contained driver for ADBMS6830B battery monitor IC.
 * Handles all SPI communication, PEC generation/verification.
 *
 * @see ADBMS6830B_Registers.h for API documentation
 */

#include "ADBMS6830B_Registers.h"
#include <string.h>

/*============================================================================
 * Global State
 *============================================================================*/

ADBMS_Chain_t g_adbms = {0};

/*============================================================================
 * Static SPI Buffers
 *
 * These buffers are file-static rather than stack-allocated to reduce task
 * stack usage. The driver is single-threaded-by-contract: all callers must
 * hold ADBMSMutexHandle (or equivalent) around any driver call sequence.
 *============================================================================*/

static uint8_t s_tx_buf[4 + ADBMS_MAX_ICS * 8]; /* cmd + (data + PEC) per IC */
static uint8_t s_rx_buf[ADBMS_MAX_ICS * 38];    /* (36 data + 2 PEC) per IC */

/*============================================================================
 * PEC-15 Lookup Table
 *
 * 15-bit CRC for command codes
 * Polynomial: x^15 + x^14 + x^10 + x^8 + x^7 + x^4 + x^3 + 1
 * Seed: 16
 *============================================================================*/

static const uint16_t s_pec15_table[256] = {
    0x0000, 0xC599, 0xCEAB, 0x0B32, 0xD8CF, 0x1D56, 0x1664, 0xD3FD, 0xF407, 0x319E, 0x3AAC, 0xFF35, 0x2CC8, 0xE951,
    0xE263, 0x27FA, 0xAD97, 0x680E, 0x633C, 0xA6A5, 0x7558, 0xB0C1, 0xBBF3, 0x7E6A, 0x5990, 0x9C09, 0x973B, 0x52A2,
    0x815F, 0x44C6, 0x4FF4, 0x8A6D, 0x5B2E, 0x9EB7, 0x9585, 0x501C, 0x83E1, 0x4678, 0x4D4A, 0x88D3, 0xAF29, 0x6AB0,
    0x6182, 0xA41B, 0x77E6, 0xB27F, 0xB94D, 0x7CD4, 0xF6B9, 0x3320, 0x3812, 0xFD8B, 0x2E76, 0xEBEF, 0xE0DD, 0x2544,
    0x02BE, 0xC727, 0xCC15, 0x098C, 0xDA71, 0x1FE8, 0x14DA, 0xD143, 0xF3C5, 0x365C, 0x3D6E, 0xF8F7, 0x2B0A, 0xEE93,
    0xE5A1, 0x2038, 0x07C2, 0xC25B, 0xC969, 0x0CF0, 0xDF0D, 0x1A94, 0x11A6, 0xD43F, 0x5E52, 0x9BCB, 0x90F9, 0x5560,
    0x869D, 0x4304, 0x4836, 0x8DAF, 0xAA55, 0x6FCC, 0x64FE, 0xA167, 0x729A, 0xB703, 0xBC31, 0x79A8, 0xA8EB, 0x6D72,
    0x6640, 0xA3D9, 0x7024, 0xB5BD, 0xBE8F, 0x7B16, 0x5CEC, 0x9975, 0x9247, 0x57DE, 0x8423, 0x41BA, 0x4A88, 0x8F11,
    0x057C, 0xC0E5, 0xCBD7, 0x0E4E, 0xDDB3, 0x182A, 0x1318, 0xD681, 0xF17B, 0x34E2, 0x3FD0, 0xFA49, 0x29B4, 0xEC2D,
    0xE71F, 0x2286, 0xA213, 0x678A, 0x6CB8, 0xA921, 0x7ADC, 0xBF45, 0xB477, 0x71EE, 0x5614, 0x938D, 0x98BF, 0x5D26,
    0x8EDB, 0x4B42, 0x4070, 0x85E9, 0x0F84, 0xCA1D, 0xC12F, 0x04B6, 0xD74B, 0x12D2, 0x19E0, 0xDC79, 0xFB83, 0x3E1A,
    0x3528, 0xF0B1, 0x234C, 0xE6D5, 0xEDE7, 0x287E, 0xF93D, 0x3CA4, 0x3796, 0xF20F, 0x21F2, 0xE46B, 0xEF59, 0x2AC0,
    0x0D3A, 0xC8A3, 0xC391, 0x0608, 0xD5F5, 0x106C, 0x1B5E, 0xDEC7, 0x54AA, 0x9133, 0x9A01, 0x5F98, 0x8C65, 0x49FC,
    0x42CE, 0x8757, 0xA0AD, 0x6534, 0x6E06, 0xAB9F, 0x7862, 0xBDFB, 0xB6C9, 0x7350, 0x51D6, 0x944F, 0x9F7D, 0x5AE4,
    0x8919, 0x4C80, 0x47B2, 0x822B, 0xA5D1, 0x6048, 0x6B7A, 0xAEE3, 0x7D1E, 0xB887, 0xB3B5, 0x762C, 0xFC41, 0x39D8,
    0x32EA, 0xF773, 0x248E, 0xE117, 0xEA25, 0x2FBC, 0x0846, 0xCDDF, 0xC6ED, 0x0374, 0xD089, 0x1510, 0x1E22, 0xDBBB,
    0x0AF8, 0xCF61, 0xC453, 0x01CA, 0xD237, 0x17AE, 0x1C9C, 0xD905, 0xFEFF, 0x3B66, 0x3054, 0xF5CD, 0x2630, 0xE3A9,
    0xE89B, 0x2D02, 0xA76F, 0x62F6, 0x69C4, 0xAC5D, 0x7FA0, 0xBA39, 0xB10B, 0x7492, 0x5368, 0x96F1, 0x9DC3, 0x585A,
    0x8BA7, 0x4E3E, 0x450C, 0x8095};

const uint16_t s_pec10_table[256] = {
    0x0,   0x8f,  0x11e, 0x191, 0x23c, 0x2b3, 0x322, 0x3ad, 0xf7,  0x78,  0x1e9, // precomputed CRC10 Table
    0x166, 0x2cb, 0x244, 0x3d5, 0x35a, 0x1ee, 0x161, 0xf0,  0x7f,  0x3d2, 0x35d, 0x2cc, 0x243, 0x119, 0x196, 0x7,
    0x88,  0x325, 0x3aa, 0x23b, 0x2b4, 0x3dc, 0x353, 0x2c2, 0x24d, 0x1e0, 0x16f, 0xfe,  0x71,  0x32b, 0x3a4, 0x235,
    0x2ba, 0x117, 0x198, 0x9,   0x86,  0x232, 0x2bd, 0x32c, 0x3a3, 0xe,   0x81,  0x110, 0x19f, 0x2c5, 0x24a, 0x3db,
    0x354, 0xf9,  0x76,  0x1e7, 0x168, 0x337, 0x3b8, 0x229, 0x2a6, 0x10b, 0x184, 0x15,  0x9a,  0x3c0, 0x34f, 0x2de,
    0x251, 0x1fc, 0x173, 0xe2,  0x6d,  0x2d9, 0x256, 0x3c7, 0x348, 0xe5,  0x6a,  0x1fb, 0x174, 0x22e, 0x2a1, 0x330,
    0x3bf, 0x12,  0x9d,  0x10c, 0x183, 0xeb,  0x64,  0x1f5, 0x17a, 0x2d7, 0x258, 0x3c9, 0x346, 0x1c,  0x93,  0x102,
    0x18d, 0x220, 0x2af, 0x33e, 0x3b1, 0x105, 0x18a, 0x1b,  0x94,  0x339, 0x3b6, 0x227, 0x2a8, 0x1f2, 0x17d, 0xec,
    0x63,  0x3ce, 0x341, 0x2d0, 0x25f, 0x2e1, 0x26e, 0x3ff, 0x370, 0xdd,  0x52,  0x1c3, 0x14c, 0x216, 0x299, 0x308,
    0x387, 0x2a,  0xa5,  0x134, 0x1bb, 0x30f, 0x380, 0x211, 0x29e, 0x133, 0x1bc, 0x2d,  0xa2,  0x3f8, 0x377, 0x2e6,
    0x269, 0x1c4, 0x14b, 0xda,  0x55,  0x13d, 0x1b2, 0x23,  0xac,  0x301, 0x38e, 0x21f, 0x290, 0x1ca, 0x145, 0xd4,
    0x5b,  0x3f6, 0x379, 0x2e8, 0x267, 0xd3,  0x5c,  0x1cd, 0x142, 0x2ef, 0x260, 0x3f1, 0x37e, 0x24,  0xab,  0x13a,
    0x1b5, 0x218, 0x297, 0x306, 0x389, 0x1d6, 0x159, 0xc8,  0x47,  0x3ea, 0x365, 0x2f4, 0x27b, 0x121, 0x1ae, 0x3f,
    0xb0,  0x31d, 0x392, 0x203, 0x28c, 0x38,  0xb7,  0x126, 0x1a9, 0x204, 0x28b, 0x31a, 0x395, 0xcf,  0x40,  0x1d1,
    0x15e, 0x2f3, 0x27c, 0x3ed, 0x362, 0x20a, 0x285, 0x314, 0x39b, 0x36,  0xb9,  0x128, 0x1a7, 0x2fd, 0x272, 0x3e3,
    0x36c, 0xc1,  0x4e,  0x1df, 0x150, 0x3e4, 0x36b, 0x2fa, 0x275, 0x1d8, 0x157, 0xc6,  0x49,  0x313, 0x39c, 0x20d,
    0x282, 0x12f, 0x1a0, 0x31,  0xbe};

/*============================================================================
 * Weak Platform Functions (Override These)
 *============================================================================*/

__attribute__((weak)) ADBMS_PlatformStatus_t ADBMS_Platform_SPI_Write(const uint8_t *data, uint16_t len)
{
  (void)data;
  (void)len;
  return ADBMS_PLATFORM_OK;
}

__attribute__((weak)) ADBMS_PlatformStatus_t ADBMS_Platform_SPI_Read(uint8_t *data, uint16_t len)
{
  (void)data;
  (void)len;
  return ADBMS_PLATFORM_OK;
}

__attribute__((weak)) ADBMS_PlatformStatus_t ADBMS_Platform_SPI_WriteRead(const uint8_t *tx_data, uint16_t tx_len, uint8_t *rx_data,
                                                        uint16_t rx_len)
{
  (void)tx_data;
  (void)tx_len;
  (void)rx_data;
  (void)rx_len;
  return ADBMS_PLATFORM_OK;
}

__attribute__((weak)) void ADBMS_Platform_CS_Low(void) {}

__attribute__((weak)) void ADBMS_Platform_CS_High(void) {}

__attribute__((weak)) void ADBMS_Platform_DelayUs(uint32_t us)
{
  (void)us;
}

__attribute__((weak)) void ADBMS_Platform_DelayMs(uint32_t ms)
{
  (void)ms;
}

__attribute__((weak)) uint32_t ADBMS_Platform_GetTickMs(void)
{
  return 0;
}

/*============================================================================
 * PEC Calculation Functions
 *============================================================================*/

uint16_t ADBMS_CalcPEC15(const uint8_t *data, uint8_t len)
{
  uint16_t remainder = 16; /* PEC seed */

  for (uint8_t i = 0; i < len; i++)
  {
    uint8_t addr = ((remainder >> 7) ^ data[i]) & 0xFF;
    remainder = (remainder << 8) ^ s_pec15_table[addr];
  }

  return (remainder << 1); /* CRC15 LSB is always 0 */
}

uint16_t ADBMS_CalcPEC10(const uint8_t *data, uint8_t len)
{
  uint16_t remainder = 16; /* PEC seed */
  const uint16_t polynomial = 0x8F;

  for (uint8_t i = 0; i < len; i++)
  {
    remainder ^= ((uint16_t)data[i] << 2);
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      if (remainder & 0x200)
      {
        remainder = (remainder << 1) ^ polynomial;
      }
      else
      {
        remainder <<= 1;
      }
    }
  }

  return remainder & 0x3FF; /* 10-bit result */
}

/*============================================================================
 * Internal: Wake-Up Sequence
 *============================================================================*/

/* Track last activity to avoid unnecessary wakeups */
static uint32_t s_last_activity_ms = 0;
#define ADBMS_SLEEP_THRESHOLD_MS 4 /* Wake if idle > 4ms (t_IDLE is ~5.4ms) */

static void _adbms_wakeup(void)
{
  /* isoSPI daisy chain: each IC consumes one pulse and propagates the next.
   * Send num_ics pulses to wake the entire chain. */
  uint8_t num_pulses = g_adbms.num_ics > 0 ? g_adbms.num_ics : 1;
  for (uint8_t i = 0; i < num_pulses; i++)
  {
    ADBMS_Platform_CS_Low();
    ADBMS_Platform_DelayUs(10);  /* t_CSB_low min */
    ADBMS_Platform_CS_High();
    ADBMS_Platform_DelayUs(400); /* t_WAKE per part */
  }
  ADBMS_Platform_DelayMs(1); /* Final guard for ref start-up */
  s_last_activity_ms = ADBMS_Platform_GetTickMs();
}

/**
 * @brief Conditionally wake the IC if it may have entered sleep
 *
 * Only performs wakeup if idle time exceeds the sleep threshold.
 * This avoids the 1.4ms overhead on every transaction.
 */
static void _ensure_awake(void)
{
  uint32_t now = ADBMS_Platform_GetTickMs();
  if ((now - s_last_activity_ms) >= ADBMS_SLEEP_THRESHOLD_MS)
  {
    _adbms_wakeup();
  }
  s_last_activity_ms = now;
}

/*============================================================================
 * Internal: Build Command Frame
 *============================================================================*/

static void _build_cmd_frame(uint16_t cmd_code, uint8_t frame[4])
{
  frame[0] = (cmd_code >> 8) & 0xFF;
  frame[1] = cmd_code & 0xFF;
  uint16_t pec = ADBMS_CalcPEC15(frame, 2);
  frame[2] = (pec >> 8) & 0xFF;
  frame[3] = pec & 0xFF;
}

/*============================================================================
 * Internal: Send Action Command (no data)
 *============================================================================*/

static ADBMS_Error_t _transmit_action(uint16_t cmd_code)
{
  uint8_t frame[4];
  _build_cmd_frame(cmd_code, frame);

  _ensure_awake();

  ADBMS_Platform_CS_Low();
  ADBMS_PlatformStatus_t status = ADBMS_Platform_SPI_Write(frame, 4);
  ADBMS_Platform_CS_High();

  if (status != ADBMS_PLATFORM_OK)
  {
    return ADBMS_ERR_SPI;
  }

  return ADBMS_OK;
}

/*============================================================================
 * Internal: Read Command
 *============================================================================*/

static ADBMS_Error_t _transmit_read(uint16_t cmd_code, uint8_t bytes_per_ic, uint8_t *dest_offsets[])
{
  if (!g_adbms.initialized)
    return ADBMS_ERR_NOT_INITIALIZED;

  uint8_t cmd_frame[4];
  _build_cmd_frame(cmd_code, cmd_frame);

  /* RX buffer: (data + PEC) per IC */
  uint16_t rx_size = (bytes_per_ic + ADBMS_PEC10_SIZE) * g_adbms.num_ics;

  _ensure_awake();

  ADBMS_Platform_CS_Low();
  ADBMS_PlatformStatus_t spi_status = ADBMS_Platform_SPI_WriteRead(cmd_frame, 4, s_rx_buf, rx_size);
  ADBMS_Platform_CS_High();

  if (spi_status != ADBMS_PLATFORM_OK)
  {
    return ADBMS_ERR_SPI;
  }

  /* Parse received data for each IC */
  ADBMS_Error_t result = ADBMS_OK;
  uint8_t *ptr = s_rx_buf;

  for (uint8_t ic = 0; ic < g_adbms.num_ics; ic++)
  {
    g_adbms.ics[ic].status.tx_count++;

    /* Data is bytes_per_ic long */
    uint8_t *data = ptr;
    ptr += bytes_per_ic;

    /* PEC-10 encoding (per ADBMS6830B datasheet, Table 67):
     * The 10-bit PEC is packed into 2 bytes as follows:
     *   Byte 0: PEC[9:2] (upper 8 bits of PEC)
     *   Byte 1: PEC[1:0] in bits 7:6, CC[5:0] in bits 5:0
     *
     * CC = Command Counter (6 bits), increments with each command.
     * Current implementation extracts PEC only; CC is not validated.
     * TODO: Add CC validation for enhanced communication integrity.
     */
    uint16_t received_pec = ((uint16_t)ptr[0] << 2) | (ptr[1] >> 6);
    ptr += ADBMS_PEC10_SIZE;

    /* Verify PEC */
    uint16_t calc_pec = ADBMS_CalcPEC10(data, bytes_per_ic);
    if (calc_pec != received_pec)
    {
      g_adbms.ics[ic].status.pec_error_count++;
      g_adbms.ics[ic].status.comm_ok = false;
      result = ADBMS_ERR_PEC;
    }
    else
    {
      g_adbms.ics[ic].status.comm_ok = true;
      /* Copy to destination if provided */
      if (dest_offsets != NULL && dest_offsets[ic] != NULL)
      {
        memcpy(dest_offsets[ic], data, bytes_per_ic);
      }
    }
  }

  return result;
}

/*============================================================================
 * Internal: Write Command
 *============================================================================*/

static ADBMS_Error_t _transmit_write(uint16_t cmd_code, uint8_t bytes_per_ic, const uint8_t *src_offsets[])
{
  if (!g_adbms.initialized)
    return ADBMS_ERR_NOT_INITIALIZED;

  uint8_t cmd_frame[4];
  _build_cmd_frame(cmd_code, cmd_frame);

  /* Build TX buffer: cmd + (data + PEC) per IC */
  /* Note: Data is sent in reverse order (last IC first on wire) */
  memcpy(s_tx_buf, cmd_frame, 4);

  uint8_t *ptr = s_tx_buf + 4;
  for (int ic = g_adbms.num_ics - 1; ic >= 0; ic--)
  {
    /* Copy data */
    if (src_offsets != NULL && src_offsets[ic] != NULL)
    {
      memcpy(ptr, src_offsets[ic], bytes_per_ic);
    }
    else
    {
      memset(ptr, 0, bytes_per_ic);
    }

    /* Calculate and append PEC-10 */
    uint16_t pec = ADBMS_CalcPEC10(ptr, bytes_per_ic);
    ptr += bytes_per_ic;
    *ptr++ = (pec >> 2) & 0xFF;
    *ptr++ = (pec << 6) & 0xC0;
  }

  uint16_t tx_len = 4 + g_adbms.num_ics * (bytes_per_ic + ADBMS_PEC10_SIZE);

  _ensure_awake();

  ADBMS_Platform_CS_Low();
  ADBMS_PlatformStatus_t status = ADBMS_Platform_SPI_Write(s_tx_buf, tx_len);
  ADBMS_Platform_CS_High();

  if (status != ADBMS_PLATFORM_OK)
  {
    return ADBMS_ERR_SPI;
  }

  return ADBMS_OK;
}

/*============================================================================
 * Register Metadata Table
 *============================================================================*/

typedef struct
{
  uint16_t read_cmd;
  uint16_t write_cmd; /* 0 if read-only */
  uint8_t data_size;
  size_t memory_offset;
} RegMeta_t;

static const RegMeta_t s_reg_meta[ADBMS_REG_COUNT] = {
    /* Configuration */
    [ADBMS_REG_CFGA] = {RDCFGA, WRCFGA, 6, offsetof(ADBMS_Memory_t, cfga)},
    [ADBMS_REG_CFGB] = {RDCFGB, WRCFGB, 6, offsetof(ADBMS_Memory_t, cfgb)},

    /* Cell Voltages */
    [ADBMS_REG_CVA] = {RDCVA, 0, 6, offsetof(ADBMS_Memory_t, cv.groups.a)},
    [ADBMS_REG_CVB] = {RDCVB, 0, 6, offsetof(ADBMS_Memory_t, cv.groups.b)},
    [ADBMS_REG_CVC] = {RDCVC, 0, 6, offsetof(ADBMS_Memory_t, cv.groups.c)},
    [ADBMS_REG_CVD] = {RDCVD, 0, 6, offsetof(ADBMS_Memory_t, cv.groups.d)},
    [ADBMS_REG_CVE] = {RDCVE, 0, 6, offsetof(ADBMS_Memory_t, cv.groups.e)},
    [ADBMS_REG_CVF] = {RDCVF, 0, 6, offsetof(ADBMS_Memory_t, cv.groups.f)},
    [ADBMS_REG_CVALL] = {RDCVALL, 0, 36, offsetof(ADBMS_Memory_t, cv.all_raw)},

    /* Averaged Cell Voltages */
    [ADBMS_REG_ACA] = {RDACA, 0, 6, offsetof(ADBMS_Memory_t, acv.groups.a)},
    [ADBMS_REG_ACB] = {RDACB, 0, 6, offsetof(ADBMS_Memory_t, acv.groups.b)},
    [ADBMS_REG_ACC] = {RDACC, 0, 6, offsetof(ADBMS_Memory_t, acv.groups.c)},
    [ADBMS_REG_ACD] = {RDACD, 0, 6, offsetof(ADBMS_Memory_t, acv.groups.d)},
    [ADBMS_REG_ACE] = {RDACE, 0, 6, offsetof(ADBMS_Memory_t, acv.groups.e)},
    [ADBMS_REG_ACF] = {RDACF, 0, 6, offsetof(ADBMS_Memory_t, acv.groups.f)},
    [ADBMS_REG_ACALL] = {RDACALL, 0, 36, offsetof(ADBMS_Memory_t, acv.all_raw)},

    /* S-Voltages */
    [ADBMS_REG_SVA] = {RDSVA, 0, 6, offsetof(ADBMS_Memory_t, sv.groups.a)},
    [ADBMS_REG_SVB] = {RDSVB, 0, 6, offsetof(ADBMS_Memory_t, sv.groups.b)},
    [ADBMS_REG_SVC] = {RDSVC, 0, 6, offsetof(ADBMS_Memory_t, sv.groups.c)},
    [ADBMS_REG_SVD] = {RDSVD, 0, 6, offsetof(ADBMS_Memory_t, sv.groups.d)},
    [ADBMS_REG_SVE] = {RDSVE, 0, 6, offsetof(ADBMS_Memory_t, sv.groups.e)},
    [ADBMS_REG_SVF] = {RDSVF, 0, 6, offsetof(ADBMS_Memory_t, sv.groups.f)},
    [ADBMS_REG_SVALL] = {RDSALL, 0, 36, offsetof(ADBMS_Memory_t, sv.all_raw)},

    /* Filtered Cell Voltages */
    [ADBMS_REG_FCA] = {RDFCA, 0, 6, offsetof(ADBMS_Memory_t, fcv.groups.a)},
    [ADBMS_REG_FCB] = {RDFCB, 0, 6, offsetof(ADBMS_Memory_t, fcv.groups.b)},
    [ADBMS_REG_FCC] = {RDFCC, 0, 6, offsetof(ADBMS_Memory_t, fcv.groups.c)},
    [ADBMS_REG_FCD] = {RDFCD, 0, 6, offsetof(ADBMS_Memory_t, fcv.groups.d)},
    [ADBMS_REG_FCE] = {RDFCE, 0, 6, offsetof(ADBMS_Memory_t, fcv.groups.e)},
    [ADBMS_REG_FCF] = {RDFCF, 0, 6, offsetof(ADBMS_Memory_t, fcv.groups.f)},
    [ADBMS_REG_FCALL] = {RDFCALL, 0, 36, offsetof(ADBMS_Memory_t, fcv.all_raw)},

    /* Auxiliary */
    [ADBMS_REG_AUXA] = {RDAUXA, 0, 6, offsetof(ADBMS_Memory_t, aux.groups.a)},
    [ADBMS_REG_AUXB] = {RDAUXB, 0, 6, offsetof(ADBMS_Memory_t, aux.groups.b)},
    [ADBMS_REG_AUXC] = {RDAUXC, 0, 6, offsetof(ADBMS_Memory_t, aux.groups.c)},
    [ADBMS_REG_AUXD] = {RDAUXD, 0, 6, offsetof(ADBMS_Memory_t, aux.groups.d)},

    /* Redundant Auxiliary */
    [ADBMS_REG_RAXA] = {RDRAXA, 0, 6, offsetof(ADBMS_Memory_t, raux.groups.a)},
    [ADBMS_REG_RAXB] = {RDRAXB, 0, 6, offsetof(ADBMS_Memory_t, raux.groups.b)},
    [ADBMS_REG_RAXC] = {RDRAXC, 0, 6, offsetof(ADBMS_Memory_t, raux.groups.c)},
    [ADBMS_REG_RAXD] = {RDRAXD, 0, 6, offsetof(ADBMS_Memory_t, raux.groups.d)},

    /* Status */
    [ADBMS_REG_STATA] = {RDSTATA, 0, 6, offsetof(ADBMS_Memory_t, stat.groups.a)},
    [ADBMS_REG_STATB] = {RDSTATB, 0, 6, offsetof(ADBMS_Memory_t, stat.groups.b)},
    [ADBMS_REG_STATC] = {RDSTATC(0), 0, 6, offsetof(ADBMS_Memory_t, stat.groups.c)},
    [ADBMS_REG_STATD] = {RDSTATD, 0, 6, offsetof(ADBMS_Memory_t, stat.groups.d)},
    [ADBMS_REG_STATE] = {RDSTATE, 0, 6, offsetof(ADBMS_Memory_t, stat.groups.e)},

    /* PWM */
    [ADBMS_REG_PWMA] = {RDPWMA, WRPWMA, 6, offsetof(ADBMS_Memory_t, pwm.groups.a)},
    [ADBMS_REG_PWMB] = {RDPWMB, WRPWMB, 6, offsetof(ADBMS_Memory_t, pwm.groups.b)},

    /* COMM */
    [ADBMS_REG_COMM] = {RDCOMM, WRCOMM, 6, offsetof(ADBMS_Memory_t, comm)},

    /* LPCM */
    [ADBMS_REG_CMCFG] = {RDCMCFG, WRCMCFG, 6, offsetof(ADBMS_Memory_t, cmcfg)},
    [ADBMS_REG_CMCELLT] = {RDCMCELLT, WRCMCELLT, 6, offsetof(ADBMS_Memory_t, cmcellt)},
    [ADBMS_REG_CMGPIOT] = {RDCMGPIOT, WRCMGPIOT, 6, offsetof(ADBMS_Memory_t, cmgpiot)},
    [ADBMS_REG_CMFLAG] = {RDCMFLAG, 0, 6, offsetof(ADBMS_Memory_t, cmflag)},

    /* Serial ID */
    [ADBMS_REG_SID] = {RDSID, 0, 6, offsetof(ADBMS_Memory_t, sid)},

    /* Retention */
    [ADBMS_REG_RR] = {RDRR, WRRR, 6, offsetof(ADBMS_Memory_t, retention)},
};

/*============================================================================
 * Initialization
 *============================================================================*/

ADBMS_Error_t ADBMS_Init(uint8_t num_ics)
{
  if (num_ics == 0 || num_ics > ADBMS_MAX_ICS)
  {
    return ADBMS_ERR_INVALID_PARAM;
  }

  memset(&g_adbms, 0, sizeof(g_adbms));
  g_adbms.num_ics = num_ics;
  g_adbms.active_cell_mask = 0xFFFF; /* Default: all 16 cells active */

  for (uint8_t i = 0; i < num_ics; i++)
  {
    g_adbms.ics[i].index = i;
  }

  g_adbms.initialized = true;
  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_WakeUp(void)
{
  _adbms_wakeup();
  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SoftReset(void)
{
  return _transmit_action(SRST);
}

ADBMS_Memory_t *ADBMS_GetMemory(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return NULL;
  return &g_adbms.ics[ic_index].memory;
}

ADBMS_ICStatus_t *ADBMS_GetStatus(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return NULL;
  return &g_adbms.ics[ic_index].status;
}

void ADBMS_SetActiveCellMask(uint16_t mask)
{
  g_adbms.active_cell_mask = mask;
}

uint16_t ADBMS_GetActiveCellMask(void)
{
  return g_adbms.active_cell_mask;
}

/*============================================================================
 * Register Access
 *============================================================================*/

/*============================================================================
 * Internal: Seqlock helpers (chain-wide per-group)
 *
 * Writers (the acquisition task, via ADBMS_ReadRegister / ADBMS_WriteRegister)
 * bracket their mutation with _seq_begin / _seq_end, which toggles the
 * counter odd -> even. Readers spin on an even seq for a consistent copy.
 *
 * A full memory barrier (__DMB) enforces ordering against memcpy-style
 * writes on the Cortex-M4. No atomic CAS is needed because there is a
 * single writer per register group (the acq task).
 *============================================================================*/

static inline void _seq_begin(ADBMS_RegGroup_t reg)
{
  if ((unsigned)reg < ADBMS_REG_COUNT)
  {
    g_adbms.reg_seq[reg]++; /* even -> odd */
    __asm volatile("dmb" ::: "memory");
  }
}

static inline void _seq_end(ADBMS_RegGroup_t reg)
{
  if ((unsigned)reg < ADBMS_REG_COUNT)
  {
    __asm volatile("dmb" ::: "memory");
    g_adbms.reg_seq[reg]++; /* odd -> even (+1 generation) */
    g_adbms.reg_last_tick_ms[reg] = ADBMS_Platform_GetTickMs();
  }
}

/* Bulk register groups cover several sub-groups in a single SPI read.
 * To keep the seqlock semantics useful for consumers that iterate the
 * individual groups (e.g. cells 1-3 via CVA) after a CVALL read, bump
 * every sub-group's seq alongside the aggregate. */
static void _seq_begin_bulk(ADBMS_RegGroup_t reg)
{
  _seq_begin(reg);
  switch (reg)
  {
  case ADBMS_REG_CVALL:
    for (int r = ADBMS_REG_CVA; r <= ADBMS_REG_CVF; r++) _seq_begin((ADBMS_RegGroup_t)r);
    break;
  case ADBMS_REG_ACALL:
    for (int r = ADBMS_REG_ACA; r <= ADBMS_REG_ACF; r++) _seq_begin((ADBMS_RegGroup_t)r);
    break;
  case ADBMS_REG_SVALL:
    for (int r = ADBMS_REG_SVA; r <= ADBMS_REG_SVF; r++) _seq_begin((ADBMS_RegGroup_t)r);
    break;
  case ADBMS_REG_FCALL:
    for (int r = ADBMS_REG_FCA; r <= ADBMS_REG_FCF; r++) _seq_begin((ADBMS_RegGroup_t)r);
    break;
  default:
    break;
  }
}

static void _seq_end_bulk(ADBMS_RegGroup_t reg)
{
  switch (reg)
  {
  case ADBMS_REG_CVALL:
    for (int r = ADBMS_REG_CVA; r <= ADBMS_REG_CVF; r++) _seq_end((ADBMS_RegGroup_t)r);
    break;
  case ADBMS_REG_ACALL:
    for (int r = ADBMS_REG_ACA; r <= ADBMS_REG_ACF; r++) _seq_end((ADBMS_RegGroup_t)r);
    break;
  case ADBMS_REG_SVALL:
    for (int r = ADBMS_REG_SVA; r <= ADBMS_REG_SVF; r++) _seq_end((ADBMS_RegGroup_t)r);
    break;
  case ADBMS_REG_FCALL:
    for (int r = ADBMS_REG_FCA; r <= ADBMS_REG_FCF; r++) _seq_end((ADBMS_RegGroup_t)r);
    break;
  default:
    break;
  }
  _seq_end(reg);
}

ADBMS_Error_t ADBMS_ReadRegister(ADBMS_RegGroup_t reg)
{
  if (reg >= ADBMS_REG_COUNT)
    return ADBMS_ERR_INVALID_PARAM;

  const RegMeta_t *meta = &s_reg_meta[reg];
  uint8_t *dest_ptrs[ADBMS_MAX_ICS];

  for (uint8_t i = 0; i < g_adbms.num_ics; i++)
  {
    dest_ptrs[i] = ((uint8_t *)&g_adbms.ics[i].memory) + meta->memory_offset;
  }

  _seq_begin_bulk(reg);
  ADBMS_Error_t rc = _transmit_read(meta->read_cmd, meta->data_size, dest_ptrs);
  _seq_end_bulk(reg);
  return rc;
}

ADBMS_Error_t ADBMS_WriteRegister(ADBMS_RegGroup_t reg)
{
  if (reg >= ADBMS_REG_COUNT)
    return ADBMS_ERR_INVALID_PARAM;

  const RegMeta_t *meta = &s_reg_meta[reg];
  if (meta->write_cmd == 0)
    return ADBMS_ERR_INVALID_PARAM; /* Read-only */

  const uint8_t *src_ptrs[ADBMS_MAX_ICS];

  for (uint8_t i = 0; i < g_adbms.num_ics; i++)
  {
    src_ptrs[i] = ((const uint8_t *)&g_adbms.ics[i].memory) + meta->memory_offset;
  }

  _seq_begin(reg);
  ADBMS_Error_t rc = _transmit_write(meta->write_cmd, meta->data_size, src_ptrs);
  _seq_end(reg);
  return rc;
}

/*============================================================================
 * Public Seqlock + Pending-Write API
 *============================================================================*/

uint32_t ADBMS_SeqBegin(ADBMS_RegGroup_t reg)
{
  if ((unsigned)reg >= ADBMS_REG_COUNT) return 0;
  /* Spin until we observe an even seq (no writer in progress). */
  uint32_t s;
  do
  {
    s = g_adbms.reg_seq[reg];
  } while (s & 1u);
  __asm volatile("dmb" ::: "memory");
  return s;
}

bool ADBMS_SeqRetry(ADBMS_RegGroup_t reg, uint32_t begin_seq)
{
  if ((unsigned)reg >= ADBMS_REG_COUNT) return false;
  __asm volatile("dmb" ::: "memory");
  return (g_adbms.reg_seq[reg] != begin_seq);
}

uint32_t ADBMS_GetRegisterLastTickMs(ADBMS_RegGroup_t reg)
{
  if ((unsigned)reg >= ADBMS_REG_COUNT) return 0;
  return g_adbms.reg_last_tick_ms[reg];
}

bool ADBMS_SnapshotRegisterGroup(ADBMS_RegGroup_t reg, uint8_t ic_index, void *dst, size_t size, uint32_t max_retries)
{
  if ((unsigned)reg >= ADBMS_REG_COUNT || ic_index >= g_adbms.num_ics || dst == NULL || size == 0)
  {
    return false;
  }

  const RegMeta_t *meta = &s_reg_meta[reg];
  const uint8_t *src = ((const uint8_t *)&g_adbms.ics[ic_index].memory) + meta->memory_offset;
  size_t copy_len = (size < meta->data_size) ? size : meta->data_size;

  for (uint32_t attempt = 0; attempt < max_retries; attempt++)
  {
    uint32_t s = ADBMS_SeqBegin(reg);
    memcpy(dst, src, copy_len);
    if (!ADBMS_SeqRetry(reg, s))
    {
      return true;
    }
  }

  /* Exhausted retries: return best-effort copy. */
  memcpy(dst, src, copy_len);
  return false;
}

bool ADBMS_SnapshotMemory(uint8_t ic_index, ADBMS_Memory_t *out, uint32_t max_retries)
{
  if (ic_index >= g_adbms.num_ics || out == NULL) return false;

  bool all_consistent = true;
  for (int r = 0; r < ADBMS_REG_COUNT; r++)
  {
    const RegMeta_t *meta = &s_reg_meta[r];
    if (meta->data_size == 0) continue;

    uint8_t *dst = ((uint8_t *)out) + meta->memory_offset;
    if (!ADBMS_SnapshotRegisterGroup((ADBMS_RegGroup_t)r, ic_index, dst, meta->data_size, max_retries))
    {
      all_consistent = false;
    }
  }
  return all_consistent;
}

void ADBMS_RequestWrite(ADBMS_RegGroup_t reg)
{
  if ((unsigned)reg >= ADBMS_REG_COUNT) return;
  if (s_reg_meta[reg].write_cmd == 0) return; /* read-only */
  __atomic_fetch_or(&g_adbms.pending_writes, (uint32_t)1u << (unsigned)reg, __ATOMIC_ACQ_REL);
}

uint32_t ADBMS_ConsumePendingWrites(void)
{
  return __atomic_exchange_n(&g_adbms.pending_writes, 0u, __ATOMIC_ACQ_REL);
}

uint32_t ADBMS_GetPendingWrites(void)
{
  return __atomic_load_n(&g_adbms.pending_writes, __ATOMIC_ACQUIRE);
}

/*============================================================================
 * ADC Control
 *============================================================================*/

ADBMS_Error_t ADBMS_StartCellADC(uint8_t rd, uint8_t cont, uint8_t dcp, uint8_t rstf, uint8_t ow)
{
  uint16_t cmd = ADCV(rd, cont, dcp, rstf, ow);
  return _transmit_action(cmd);
}

ADBMS_Error_t ADBMS_StartSADC(uint8_t cont, uint8_t dcp, uint8_t ow)
{
  uint16_t cmd = ADSV(cont, dcp, ow);
  return _transmit_action(cmd);
}

ADBMS_Error_t ADBMS_StartAuxADC(uint8_t ow, uint8_t pup, uint8_t ch)
{
  uint16_t cmd = ADAX(ow, pup, ch);
  return _transmit_action(cmd);
}

static ADBMS_Error_t _poll_adc_internal(uint16_t poll_cmd, uint32_t timeout_ms)
{
  uint8_t cmd_frame[4];
  _build_cmd_frame(poll_cmd, cmd_frame);

  /* Ensure IC is awake before polling loop (only once, not every iteration) */
  _ensure_awake();

  uint32_t start = ADBMS_Platform_GetTickMs();

  while ((ADBMS_Platform_GetTickMs() - start) < timeout_ms)
  {
    uint8_t rx;

    ADBMS_Platform_CS_Low();
    ADBMS_Platform_SPI_WriteRead(cmd_frame, 4, &rx, 1);
    ADBMS_Platform_CS_High();

    if (rx == 0xFF)
    {
      s_last_activity_ms = ADBMS_Platform_GetTickMs();
      return ADBMS_OK; /* Conversion complete */
    }

    ADBMS_Platform_DelayMs(1);
  }

  return ADBMS_ERR_TIMEOUT;
}

ADBMS_Error_t ADBMS_PollADC(uint32_t timeout_ms)
{
  return _poll_adc_internal(PLADC, timeout_ms);
}

ADBMS_Error_t ADBMS_PollCADC(uint32_t timeout_ms)
{
  return _poll_adc_internal(PLCADC, timeout_ms);
}

ADBMS_Error_t ADBMS_PollSADC(uint32_t timeout_ms)
{
  return _poll_adc_internal(PLSADC, timeout_ms);
}

ADBMS_Error_t ADBMS_PollAuxADC(uint32_t timeout_ms)
{
  return _poll_adc_internal(PLAUX, timeout_ms);
}

/*============================================================================
 * Clear & Control Commands
 *============================================================================*/

ADBMS_Error_t ADBMS_ClearCellVoltages(void)
{
  return _transmit_action(CLRCELL);
}
ADBMS_Error_t ADBMS_ClearFilteredVoltages(void)
{
  return _transmit_action(CLRFC);
}
ADBMS_Error_t ADBMS_ClearAuxVoltages(void)
{
  return _transmit_action(CLRAUX);
}
ADBMS_Error_t ADBMS_ClearSVoltages(void)
{
  return _transmit_action(CLRSPIN);
}
ADBMS_Error_t ADBMS_ClearFlags(void)
{
  return _transmit_action(CLRFLAG);
}
ADBMS_Error_t ADBMS_ClearOVUV(void)
{
  return _transmit_action(CLOVUV);
}

ADBMS_Error_t ADBMS_MuteDischarge(void)
{
  return _transmit_action(MUTE);
}
ADBMS_Error_t ADBMS_UnmuteDischarge(void)
{
  return _transmit_action(UNMUTE);
}
ADBMS_Error_t ADBMS_Snapshot(void)
{
  return _transmit_action(SNAP);
}
ADBMS_Error_t ADBMS_ReleaseSnapshot(void)
{
  return _transmit_action(UNSNAP);
}
ADBMS_Error_t ADBMS_ResetCommandCounter(void)
{
  return _transmit_action(RSTCC);
}

/*============================================================================
 * High-Level Convenience Functions
 *============================================================================*/

ADBMS_Error_t ADBMS_ReadAllCellVoltages(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CVALL);
}

ADBMS_Error_t ADBMS_ReadAllAveragedVoltages(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_ACALL);
}

ADBMS_Error_t ADBMS_ReadAllSVoltages(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_SVALL);
}

ADBMS_Error_t ADBMS_ReadAllFilteredVoltages(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_FCALL);
}

ADBMS_Error_t ADBMS_ReadAllAux(void)
{
  ADBMS_Error_t err;
  err = ADBMS_ReadRegister(ADBMS_REG_AUXA);
  if (err != ADBMS_OK)
    return err;
  err = ADBMS_ReadRegister(ADBMS_REG_AUXB);
  if (err != ADBMS_OK)
    return err;
  err = ADBMS_ReadRegister(ADBMS_REG_AUXC);
  if (err != ADBMS_OK)
    return err;
  return ADBMS_ReadRegister(ADBMS_REG_AUXD);
}

ADBMS_Error_t ADBMS_ReadAllStatus(void)
{
  ADBMS_Error_t err;
  err = ADBMS_ReadRegister(ADBMS_REG_STATA);
  if (err != ADBMS_OK)
    return err;
  err = ADBMS_ReadRegister(ADBMS_REG_STATB);
  if (err != ADBMS_OK)
    return err;
  err = ADBMS_ReadRegister(ADBMS_REG_STATC);
  if (err != ADBMS_OK)
    return err;
  err = ADBMS_ReadRegister(ADBMS_REG_STATD);
  if (err != ADBMS_OK)
    return err;
  return ADBMS_ReadRegister(ADBMS_REG_STATE);
}

ADBMS_Error_t ADBMS_WriteConfig(void)
{
  ADBMS_Error_t err;
  err = ADBMS_WriteRegister(ADBMS_REG_CFGA);
  if (err != ADBMS_OK)
    return err;
  return ADBMS_WriteRegister(ADBMS_REG_CFGB);
}

ADBMS_Error_t ADBMS_ReadConfig(void)
{
  ADBMS_Error_t err;
  err = ADBMS_ReadRegister(ADBMS_REG_CFGA);
  if (err != ADBMS_OK)
    return err;
  return ADBMS_ReadRegister(ADBMS_REG_CFGB);
}

/*============================================================================
 * Internal: Get Raw Cell Voltage Code
 *============================================================================*/

static uint16_t _get_cell_voltage_code(uint8_t ic_index, uint8_t cell_index)
{
  if (ic_index >= g_adbms.num_ics || cell_index >= ADBMS_NUM_CELLS)
    return 0;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  uint8_t *raw = mem->cv.all_raw;

  /* Each cell is 2 bytes, little-endian */
  uint16_t offset = cell_index * 2;
  return (uint16_t)raw[offset] | ((uint16_t)raw[offset + 1] << 8);
}

/*============================================================================
 * Parsed Value Getters
 *============================================================================*/

int32_t ADBMS_GetCellVoltage_mV(uint8_t ic_index, uint8_t cell_index)
{
  if (ic_index >= g_adbms.num_ics || cell_index >= ADBMS_NUM_CELLS)
    return -1;

  uint16_t code = _get_cell_voltage_code(ic_index, cell_index);
  int16_t signed_code = (int16_t)code;
  /* Voltage = signed_code * 150uV + 1.5V offset
   * 0x0000 = 1.5V, 0x7FFF = 6.415V, 0x8000 = -3.415V */
  return ((int32_t)signed_code * 150) / 1000 + 1500;
}

float ADBMS_GetCellVoltage_V(uint8_t ic_index, uint8_t cell_index)
{
  int32_t mv = ADBMS_GetCellVoltage_mV(ic_index, cell_index);
  if (mv < 0)
    return -1.0f;
  return (float)mv / 1000.0f;
}

int32_t ADBMS_GetAvgCellVoltage_mV(uint8_t ic_index, uint8_t cell_index)
{
  if (ic_index >= g_adbms.num_ics || cell_index >= ADBMS_NUM_CELLS)
    return -1;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  uint8_t *raw = mem->acv.all_raw;
  uint16_t offset = cell_index * 2;
  uint16_t code = (uint16_t)raw[offset] | ((uint16_t)raw[offset + 1] << 8);
  int16_t signed_code = (int16_t)code;

  return ((int32_t)signed_code * 150) / 1000 + 1500;
}

int32_t ADBMS_GetSVoltage_mV(uint8_t ic_index, uint8_t cell_index)
{
  if (ic_index >= g_adbms.num_ics || cell_index >= ADBMS_NUM_CELLS)
    return -1;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  uint8_t *raw = mem->sv.all_raw;
  uint16_t offset = cell_index * 2;
  uint16_t code = (uint16_t)raw[offset] | ((uint16_t)raw[offset + 1] << 8);
  int16_t signed_code = (int16_t)code;

  return ((int32_t)signed_code * 150) / 1000 + 1500;
}

int32_t ADBMS_GetGPIOVoltage_mV(uint8_t ic_index, uint8_t gpio_index)
{
  if (ic_index >= g_adbms.num_ics || gpio_index >= ADBMS_NUM_GPIO)
    return -1;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  uint8_t *raw = mem->aux.all_raw;
  uint16_t offset = gpio_index * 2;
  uint16_t code = (uint16_t)raw[offset] | ((uint16_t)raw[offset + 1] << 8);
  int16_t signed_code = (int16_t)code;

  return ((int32_t)signed_code * 150) / 1000 + 1500;
}

int16_t ADBMS_GetInternalTemp_dC(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return INT16_MIN;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATA_t stat_a;
  RDSTATA_DECODE(mem->stat.groups.a.raw, &stat_a);

  /* Per ADBMS6830B datasheet:
   *   T(°C) = (ITMP * 150 uV + 1.5 V) / 7.5 mV/°C  -  273 °C
   *         = ITMP * 0.02  -  73
   * In deci-Celsius:
   *   T(dC)  = ITMP * 0.2  -  730
   *          = (ITMP * 2 - 7300) / 10
   * ITMP is read as the same 150 uV/LSB signed code format used for
   * cell voltages. */
  int16_t signed_code = (int16_t)stat_a.ITMP;
  return (int16_t)(((int32_t)signed_code * 2 - 7300) / 10);
}

float ADBMS_GetInternalTemp_C(uint8_t ic_index)
{
  int16_t dc = ADBMS_GetInternalTemp_dC(ic_index);
  if (dc == INT16_MIN)
    return -999.0f;
  return (float)dc / 10.0f;
}

bool ADBMS_GetCellUVFlag(uint8_t ic_index, uint8_t cell_index)
{
  if (ic_index >= g_adbms.num_ics || cell_index >= ADBMS_NUM_CELLS)
    return false;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATD_t stat_d;
  RDSTATD_DECODE(mem->stat.groups.d.raw, &stat_d);

  return (stat_d.C_UV >> cell_index) & 0x01;
}

bool ADBMS_GetCellOVFlag(uint8_t ic_index, uint8_t cell_index)
{
  if (ic_index >= g_adbms.num_ics || cell_index >= ADBMS_NUM_CELLS)
    return false;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATD_t stat_d;
  RDSTATD_DECODE(mem->stat.groups.d.raw, &stat_d);

  return (stat_d.C_OV >> cell_index) & 0x01;
}

int32_t ADBMS_GetPackVoltage_mV(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return -1;

  int32_t sum = 0;
  uint16_t mask = g_adbms.active_cell_mask;
  for (uint8_t i = 0; i < ADBMS_NUM_CELLS; i++)
  {
    if (!(mask & (1u << i)))
      continue; /* Skip masked-out cells */
    int32_t v = ADBMS_GetCellVoltage_mV(ic_index, i);
    if (v > 0)
      sum += v;
  }
  return sum;
}

int32_t ADBMS_GetMinCellVoltage_mV(uint8_t ic_index, uint8_t *min_cell_index)
{
  if (ic_index >= g_adbms.num_ics)
    return -1;

  int32_t min_v = INT32_MAX;
  uint8_t min_idx = 0;
  uint16_t mask = g_adbms.active_cell_mask;

  for (uint8_t i = 0; i < ADBMS_NUM_CELLS; i++)
  {
    if (!(mask & (1u << i)))
      continue; /* Skip masked-out cells */
    int32_t v = ADBMS_GetCellVoltage_mV(ic_index, i);
    if (v > 0 && v < min_v)
    {
      min_v = v;
      min_idx = i;
    }
  }

  if (min_cell_index != NULL)
    *min_cell_index = min_idx;
  return (min_v == INT32_MAX) ? -1 : min_v;
}

int32_t ADBMS_GetMaxCellVoltage_mV(uint8_t ic_index, uint8_t *max_cell_index)
{
  if (ic_index >= g_adbms.num_ics)
    return -1;

  int32_t max_v = 0;
  uint8_t max_idx = 0;
  uint16_t mask = g_adbms.active_cell_mask;

  for (uint8_t i = 0; i < ADBMS_NUM_CELLS; i++)
  {
    if (!(mask & (1u << i)))
      continue; /* Skip masked-out cells */
    int32_t v = ADBMS_GetCellVoltage_mV(ic_index, i);
    if (v > max_v)
    {
      max_v = v;
      max_idx = i;
    }
  }

  if (max_cell_index != NULL)
    *max_cell_index = max_idx;
  return max_v;
}

/*============================================================================
 * Configuration Setters
 *============================================================================*/

ADBMS_Error_t ADBMS_SetUVThreshold(uint8_t ic_index, uint16_t threshold_mV)
{
  /* VUV = (V - 1.5V) / 2.4mV - 1, per datasheet Table 107 */
  uint16_t code = ADBMS_MVToThresholdCode(threshold_mV);
  if (code > 0)
    code--;

  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGBR_t cfg;
      CFGBR_DECODE(g_adbms.ics[i].memory.cfgb.raw, &cfg);
      cfg.VUV = code;
      CFGBR_ENCODE(&cfg, g_adbms.ics[i].memory.cfgb.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGBR_t cfg;
    CFGBR_DECODE(g_adbms.ics[ic_index].memory.cfgb.raw, &cfg);
    cfg.VUV = code;
    CFGBR_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfgb.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }

  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetOVThreshold(uint8_t ic_index, uint16_t threshold_mV)
{
  /* VOV = (V - 1.5V) / 2.4mV, per datasheet Table 107 */
  uint16_t code = ADBMS_MVToThresholdCode(threshold_mV);

  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGBR_t cfg;
      CFGBR_DECODE(g_adbms.ics[i].memory.cfgb.raw, &cfg);
      cfg.VOV = code;
      CFGBR_ENCODE(&cfg, g_adbms.ics[i].memory.cfgb.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGBR_t cfg;
    CFGBR_DECODE(g_adbms.ics[ic_index].memory.cfgb.raw, &cfg);
    cfg.VOV = code;
    CFGBR_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfgb.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }

  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetDischarge(uint8_t ic_index, uint16_t cell_mask)
{
  if (ic_index >= g_adbms.num_ics)
    return ADBMS_ERR_INVALID_PARAM;

  CFGBR_t cfg;
  CFGBR_DECODE(g_adbms.ics[ic_index].memory.cfgb.raw, &cfg);
  cfg.DCC = cell_mask;
  CFGBR_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfgb.raw);

  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetDischargeTimeout(uint8_t ic_index, uint8_t timeout_code)
{
  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGBR_t cfg;
      CFGBR_DECODE(g_adbms.ics[i].memory.cfgb.raw, &cfg);
      cfg.DCTO = timeout_code & 0x3F;
      CFGBR_ENCODE(&cfg, g_adbms.ics[i].memory.cfgb.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGBR_t cfg;
    CFGBR_DECODE(g_adbms.ics[ic_index].memory.cfgb.raw, &cfg);
    cfg.DCTO = timeout_code & 0x3F;
    CFGBR_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfgb.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }

  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetGPO(uint8_t ic_index, uint16_t gpio_mask)
{
  if (ic_index >= g_adbms.num_ics)
    return ADBMS_ERR_INVALID_PARAM;

  CFGARA_t cfg;
  CFGARA_DECODE(g_adbms.ics[ic_index].memory.cfga.raw, &cfg);
  cfg.GPO = gpio_mask & 0x7FF;
  CFGARA_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfga.raw);

  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetRefOn(uint8_t ic_index, bool enable)
{
  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGARA_t cfg;
      CFGARA_DECODE(g_adbms.ics[i].memory.cfga.raw, &cfg);
      cfg.REFON = enable ? 1 : 0;
      CFGARA_ENCODE(&cfg, g_adbms.ics[i].memory.cfga.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGARA_t cfg;
    CFGARA_DECODE(g_adbms.ics[ic_index].memory.cfga.raw, &cfg);
    cfg.REFON = enable ? 1 : 0;
    CFGARA_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfga.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }

  return ADBMS_OK;
}

/*============================================================================
 * PWM Control
 *============================================================================*/

ADBMS_Error_t ADBMS_SetPWM(uint8_t ic_index, uint8_t cell_index, uint8_t duty)
{
  if (ic_index >= g_adbms.num_ics || cell_index >= ADBMS_NUM_CELLS)
    return ADBMS_ERR_INVALID_PARAM;

  duty &= 0x0F; /* 4-bit duty cycle */

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;

  if (cell_index < 12)
  {
    /* PWMA handles cells 0-11 */
    PWMA_t pwm;
    PWMA_DECODE(mem->pwm.groups.a.raw, &pwm);

    switch (cell_index)
    {
    case 0:
      pwm.PWM1 = duty;
      break;
    case 1:
      pwm.PWM2 = duty;
      break;
    case 2:
      pwm.PWM3 = duty;
      break;
    case 3:
      pwm.PWM4 = duty;
      break;
    case 4:
      pwm.PWM5 = duty;
      break;
    case 5:
      pwm.PWM6 = duty;
      break;
    case 6:
      pwm.PWM7 = duty;
      break;
    case 7:
      pwm.PWM8 = duty;
      break;
    case 8:
      pwm.PWM9 = duty;
      break;
    case 9:
      pwm.PWM10 = duty;
      break;
    case 10:
      pwm.PWM11 = duty;
      break;
    case 11:
      pwm.PWM12 = duty;
      break;
    }

    PWMA_ENCODE(&pwm, mem->pwm.groups.a.raw);
  }
  else
  {
    /* PWMB handles cells 12-15 */
    PWMB_t pwm;
    PWMB_DECODE(mem->pwm.groups.b.raw, &pwm);

    switch (cell_index)
    {
    case 12:
      pwm.PWM13 = duty;
      break;
    case 13:
      pwm.PWM14 = duty;
      break;
    case 14:
      pwm.PWM15 = duty;
      break;
    case 15:
      pwm.PWM16 = duty;
      break;
    }

    PWMB_ENCODE(&pwm, mem->pwm.groups.b.raw);
  }

  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_WritePWM(void)
{
  ADBMS_Error_t err;
  err = ADBMS_WriteRegister(ADBMS_REG_PWMA);
  if (err != ADBMS_OK)
    return err;
  return ADBMS_WriteRegister(ADBMS_REG_PWMB);
}

/*============================================================================
 * Individual Register Read Functions
 *============================================================================*/

/* Cell Voltages */
ADBMS_Error_t ADBMS_ReadCellVoltagesA(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CVA);
}
ADBMS_Error_t ADBMS_ReadCellVoltagesB(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CVB);
}
ADBMS_Error_t ADBMS_ReadCellVoltagesC(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CVC);
}
ADBMS_Error_t ADBMS_ReadCellVoltagesD(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CVD);
}
ADBMS_Error_t ADBMS_ReadCellVoltagesE(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CVE);
}
ADBMS_Error_t ADBMS_ReadCellVoltagesF(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CVF);
}

/* S-Voltages (Redundant ADC) */
ADBMS_Error_t ADBMS_ReadSVoltagesA(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_SVA);
}
ADBMS_Error_t ADBMS_ReadSVoltagesB(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_SVB);
}
ADBMS_Error_t ADBMS_ReadSVoltagesC(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_SVC);
}
ADBMS_Error_t ADBMS_ReadSVoltagesD(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_SVD);
}
ADBMS_Error_t ADBMS_ReadSVoltagesE(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_SVE);
}
ADBMS_Error_t ADBMS_ReadSVoltagesF(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_SVF);
}

/* Auxiliary (GPIO Voltages) */
ADBMS_Error_t ADBMS_ReadAuxA(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_AUXA);
}
ADBMS_Error_t ADBMS_ReadAuxB(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_AUXB);
}
ADBMS_Error_t ADBMS_ReadAuxC(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_AUXC);
}
ADBMS_Error_t ADBMS_ReadAuxD(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_AUXD);
}

/* Redundant Auxiliary */
ADBMS_Error_t ADBMS_ReadRAuxA(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_RAXA);
}
ADBMS_Error_t ADBMS_ReadRAuxB(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_RAXB);
}
ADBMS_Error_t ADBMS_ReadRAuxC(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_RAXC);
}
ADBMS_Error_t ADBMS_ReadRAuxD(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_RAXD);
}

/* Status Registers */
ADBMS_Error_t ADBMS_ReadStatusA(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_STATA);
}
ADBMS_Error_t ADBMS_ReadStatusB(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_STATB);
}
ADBMS_Error_t ADBMS_ReadStatusC(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_STATC);
}
ADBMS_Error_t ADBMS_ReadStatusD(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_STATD);
}
ADBMS_Error_t ADBMS_ReadStatusE(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_STATE);
}

/* Configuration Registers */
ADBMS_Error_t ADBMS_ReadConfigA(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CFGA);
}
ADBMS_Error_t ADBMS_ReadConfigB(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CFGB);
}

/* Averaged Cell Voltages */
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesA(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_ACA);
}
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesB(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_ACB);
}
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesC(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_ACC);
}
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesD(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_ACD);
}
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesE(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_ACE);
}
ADBMS_Error_t ADBMS_ReadAvgCellVoltagesF(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_ACF);
}

/* Filtered Cell Voltages */
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesA(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_FCA);
}
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesB(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_FCB);
}
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesC(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_FCC);
}
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesD(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_FCD);
}
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesE(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_FCE);
}
ADBMS_Error_t ADBMS_ReadFilteredCellVoltagesF(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_FCF);
}

/* Serial ID */
ADBMS_Error_t ADBMS_ReadSerialID(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_SID);
}

/*============================================================================
 * Additional Getter Functions
 *============================================================================*/

int32_t ADBMS_GetFilteredCellVoltage_mV(uint8_t ic_index, uint8_t cell_index)
{
  if (ic_index >= g_adbms.num_ics || cell_index >= ADBMS_NUM_CELLS)
    return -1;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  uint8_t *raw = mem->fcv.all_raw;
  uint16_t offset = cell_index * 2;
  uint16_t code = (uint16_t)raw[offset] | ((uint16_t)raw[offset + 1] << 8);
  int16_t signed_code = (int16_t)code;

  return ((int32_t)signed_code * 150) / 1000 + 1500;
}

int32_t ADBMS_GetVREF2_mV(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return -1;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATA_t stat_a;
  RDSTATA_DECODE(mem->stat.groups.a.raw, &stat_a);

  /* VREF2: signed 16-bit code * 150uV/LSB + 1.5V offset */
  int16_t signed_code = (int16_t)stat_a.VREF2;
  return ((int32_t)signed_code * 150) / 1000 + 1500;
}

int32_t ADBMS_GetVD_mV(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return -1;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATB_t stat_b;
  RDSTATB_DECODE(mem->stat.groups.b.raw, &stat_b);

  /* VD: signed 16-bit code * 150uV/LSB + 1.5V offset */
  int16_t signed_code = (int16_t)stat_b.VD;
  return ((int32_t)signed_code * 150) / 1000 + 1500;
}

int32_t ADBMS_GetVA_mV(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return -1;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATB_t stat_b;
  RDSTATB_DECODE(mem->stat.groups.b.raw, &stat_b);

  /* VA: signed 16-bit code * 150uV/LSB + 1.5V offset */
  int16_t signed_code = (int16_t)stat_b.VA;
  return ((int32_t)signed_code * 150) / 1000 + 1500;
}

uint8_t ADBMS_GetRevisionID(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return 0xFF;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATE_t stat_e;
  RDSTATE_DECODE(mem->stat.groups.e.raw, &stat_e);

  return stat_e.REV;
}

void ADBMS_GetSerialID(uint8_t ic_index, uint8_t sid_out[6])
{
  if (ic_index >= g_adbms.num_ics || sid_out == NULL)
    return;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  memcpy(sid_out, mem->sid.raw, 6);
}

bool ADBMS_IsChainCommOk(void)
{
  for (uint8_t i = 0; i < g_adbms.num_ics; i++)
  {
    if (!g_adbms.ics[i].status.comm_ok)
      return false;
  }
  return true;
}

uint16_t ADBMS_GetFailedICMask(void)
{
  uint16_t mask = 0;
  for (uint8_t i = 0; i < g_adbms.num_ics; i++)
  {
    if (!g_adbms.ics[i].status.comm_ok)
      mask |= (1 << i);
  }
  return mask;
}

/*============================================================================
 * Register Name Table
 *============================================================================*/

static const char *s_reg_names[ADBMS_REG_COUNT] = {
    [ADBMS_REG_CFGA] = "CFGA",       [ADBMS_REG_CFGB] = "CFGB",     [ADBMS_REG_CVA] = "CVA",
    [ADBMS_REG_CVB] = "CVB",         [ADBMS_REG_CVC] = "CVC",       [ADBMS_REG_CVD] = "CVD",
    [ADBMS_REG_CVE] = "CVE",         [ADBMS_REG_CVF] = "CVF",       [ADBMS_REG_CVALL] = "CVALL",
    [ADBMS_REG_ACA] = "ACA",         [ADBMS_REG_ACB] = "ACB",       [ADBMS_REG_ACC] = "ACC",
    [ADBMS_REG_ACD] = "ACD",         [ADBMS_REG_ACE] = "ACE",       [ADBMS_REG_ACF] = "ACF",
    [ADBMS_REG_ACALL] = "ACALL",     [ADBMS_REG_SVA] = "SVA",       [ADBMS_REG_SVB] = "SVB",
    [ADBMS_REG_SVC] = "SVC",         [ADBMS_REG_SVD] = "SVD",       [ADBMS_REG_SVE] = "SVE",
    [ADBMS_REG_SVF] = "SVF",         [ADBMS_REG_SVALL] = "SVALL",   [ADBMS_REG_FCA] = "FCA",
    [ADBMS_REG_FCB] = "FCB",         [ADBMS_REG_FCC] = "FCC",       [ADBMS_REG_FCD] = "FCD",
    [ADBMS_REG_FCE] = "FCE",         [ADBMS_REG_FCF] = "FCF",       [ADBMS_REG_FCALL] = "FCALL",
    [ADBMS_REG_AUXA] = "AUXA",       [ADBMS_REG_AUXB] = "AUXB",     [ADBMS_REG_AUXC] = "AUXC",
    [ADBMS_REG_AUXD] = "AUXD",       [ADBMS_REG_RAXA] = "RAXA",     [ADBMS_REG_RAXB] = "RAXB",
    [ADBMS_REG_RAXC] = "RAXC",       [ADBMS_REG_RAXD] = "RAXD",     [ADBMS_REG_STATA] = "STATA",
    [ADBMS_REG_STATB] = "STATB",     [ADBMS_REG_STATC] = "STATC",   [ADBMS_REG_STATD] = "STATD",
    [ADBMS_REG_STATE] = "STATE",     [ADBMS_REG_PWMA] = "PWMA",     [ADBMS_REG_PWMB] = "PWMB",
    [ADBMS_REG_COMM] = "COMM",       [ADBMS_REG_CMCFG] = "CMCFG",   [ADBMS_REG_CMCELLT] = "CMCELLT",
    [ADBMS_REG_CMGPIOT] = "CMGPIOT", [ADBMS_REG_CMFLAG] = "CMFLAG", [ADBMS_REG_SID] = "SID",
    [ADBMS_REG_RR] = "RR",
};

const char *ADBMS_GetRegisterName(ADBMS_RegGroup_t reg)
{
  if (reg >= ADBMS_REG_COUNT)
    return "UNKNOWN";
  return s_reg_names[reg];
}

/*============================================================================
 * Register Dump Functions
 *============================================================================*/

void ADBMS_DumpRegister(ADBMS_RegGroup_t reg, ADBMS_PrintFunc_t print_fn)
{
  if (reg >= ADBMS_REG_COUNT || print_fn == NULL)
    return;

  const RegMeta_t *meta = &s_reg_meta[reg];

  for (uint8_t ic = 0; ic < g_adbms.num_ics; ic++)
  {
    uint8_t *raw = ((uint8_t *)&g_adbms.ics[ic].memory) + meta->memory_offset;

    print_fn("IC%d %s: ", ic, s_reg_names[reg]);
    for (uint8_t i = 0; i < meta->data_size; i++)
    {
      print_fn("%02X ", raw[i]);
    }
    print_fn("\r\n");
  }
}

void ADBMS_DumpCommStatus(ADBMS_PrintFunc_t print_fn)
{
  if (print_fn == NULL)
    return;

  print_fn("\r\n=== ADBMS Communication Status ===\r\n");
  print_fn("Chain: %d ICs, Initialized: %s\r\n", g_adbms.num_ics, g_adbms.initialized ? "YES" : "NO");
  print_fn("\r\n");
  print_fn("IC  TX Count   PEC Errors  Comm OK\r\n");
  print_fn("--  ---------  ----------  -------\r\n");

  for (uint8_t ic = 0; ic < g_adbms.num_ics; ic++)
  {
    ADBMS_ICStatus_t *status = &g_adbms.ics[ic].status;
    print_fn("%2d  %9lu  %10lu  %s\r\n", ic, (unsigned long)status->tx_count, (unsigned long)status->pec_error_count,
             status->comm_ok ? "OK" : "FAIL");
  }
}

ADBMS_Error_t ADBMS_ReadAllRegistersToCache(void)
{
  ADBMS_Error_t err;

  /* Read configuration */
  err = ADBMS_ReadConfig();
  if (err != ADBMS_OK)
    return err;

  /* Read cell voltages */
  err = ADBMS_ReadAllCellVoltages();
  if (err != ADBMS_OK)
    return err;

  /* Read S-voltages */
  err = ADBMS_ReadAllSVoltages();
  if (err != ADBMS_OK)
    return err;

  /* Read auxiliary */
  err = ADBMS_ReadAllAux();
  if (err != ADBMS_OK)
    return err;

  /* Read status */
  err = ADBMS_ReadAllStatus();
  if (err != ADBMS_OK)
    return err;

  /* Read serial ID */
  err = ADBMS_ReadSerialID();
  if (err != ADBMS_OK)
    return err;

  return ADBMS_OK;
}

void ADBMS_DumpAllRegisters(ADBMS_PrintFunc_t print_fn, bool fresh_read)
{
  if (print_fn == NULL)
    return;

  if (fresh_read)
  {
    print_fn("Reading fresh data from ICs...\r\n");
    ADBMS_Error_t err = ADBMS_ReadAllRegistersToCache();
    if (err != ADBMS_OK)
    {
      print_fn("Warning: Read error (code %d), some data may be stale\r\n", err);
    }
  }

  print_fn("\r\n=== ADBMS Register Dump ===\r\n");
  print_fn("Chain: %d ICs\r\n\r\n", g_adbms.num_ics);

  /* Configuration */
  print_fn("--- Configuration ---\r\n");
  ADBMS_DumpRegister(ADBMS_REG_CFGA, print_fn);
  ADBMS_DumpRegister(ADBMS_REG_CFGB, print_fn);

  /* Cell Voltages */
  print_fn("\r\n--- Cell Voltages (C-ADC) ---\r\n");
  for (uint8_t ic = 0; ic < g_adbms.num_ics; ic++)
  {
    print_fn("IC%d: ", ic);
    for (uint8_t cell = 0; cell < ADBMS_NUM_CELLS; cell++)
    {
      int32_t mv = ADBMS_GetCellVoltage_mV(ic, cell);
      print_fn("C%d=%ld ", cell + 1, (long)mv);
    }
    print_fn("\r\n");
  }

  /* S-Voltages */
  print_fn("\r\n--- S-Voltages (Redundant ADC) ---\r\n");
  for (uint8_t ic = 0; ic < g_adbms.num_ics; ic++)
  {
    print_fn("IC%d: ", ic);
    for (uint8_t cell = 0; cell < ADBMS_NUM_CELLS; cell++)
    {
      int32_t mv = ADBMS_GetSVoltage_mV(ic, cell);
      print_fn("S%d=%ld ", cell + 1, (long)mv);
    }
    print_fn("\r\n");
  }

  /* GPIO/Auxiliary */
  print_fn("\r\n--- GPIO Voltages ---\r\n");
  for (uint8_t ic = 0; ic < g_adbms.num_ics; ic++)
  {
    print_fn("IC%d: ", ic);
    for (uint8_t gpio = 0; gpio < ADBMS_NUM_GPIO; gpio++)
    {
      int32_t mv = ADBMS_GetGPIOVoltage_mV(ic, gpio);
      print_fn("G%d=%ld ", gpio + 1, (long)mv);
    }
    print_fn("\r\n");
  }

  /* Status */
  print_fn("\r\n--- Status ---\r\n");
  for (uint8_t ic = 0; ic < g_adbms.num_ics; ic++)
  {
    float temp = ADBMS_GetInternalTemp_C(ic);
    int32_t vref2 = ADBMS_GetVREF2_mV(ic);
    int32_t vd = ADBMS_GetVD_mV(ic);
    int32_t va = ADBMS_GetVA_mV(ic);
    uint8_t rev = ADBMS_GetRevisionID(ic);

    print_fn("IC%d: Temp=%.1fC VREF2=%ldmV VD=%ldmV VA=%ldmV REV=%d\r\n", ic, temp, (long)vref2, (long)vd, (long)va,
             rev);
  }

  /* Serial ID */
  print_fn("\r\n--- Serial ID ---\r\n");
  for (uint8_t ic = 0; ic < g_adbms.num_ics; ic++)
  {
    uint8_t sid[6];
    ADBMS_GetSerialID(ic, sid);
    print_fn("IC%d: %02X%02X%02X%02X%02X%02X\r\n", ic, sid[0], sid[1], sid[2], sid[3], sid[4], sid[5]);
  }

  /* Communication Status */
  print_fn("\r\n--- Communication ---\r\n");
  for (uint8_t ic = 0; ic < g_adbms.num_ics; ic++)
  {
    ADBMS_ICStatus_t *status = &g_adbms.ics[ic].status;
    print_fn("IC%d: TX=%lu PEC_ERR=%lu %s\r\n", ic, (unsigned long)status->tx_count,
             (unsigned long)status->pec_error_count, status->comm_ok ? "OK" : "FAIL");
  }
}

/*============================================================================
 * Configuration Register A Setters (Table 102)
 *============================================================================*/

ADBMS_Error_t ADBMS_SetCTH(uint8_t ic_index, ADBMS_CTH_t cth)
{
  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGARA_t cfg;
      CFGARA_DECODE(g_adbms.ics[i].memory.cfga.raw, &cfg);
      cfg.CTH = cth & 0x07;
      CFGARA_ENCODE(&cfg, g_adbms.ics[i].memory.cfga.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGARA_t cfg;
    CFGARA_DECODE(g_adbms.ics[ic_index].memory.cfga.raw, &cfg);
    cfg.CTH = cth & 0x07;
    CFGARA_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfga.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }
  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetFlagD(uint8_t ic_index, uint8_t flag_d)
{
  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGARA_t cfg;
      CFGARA_DECODE(g_adbms.ics[i].memory.cfga.raw, &cfg);
      cfg.FLAG_D = flag_d;
      CFGARA_ENCODE(&cfg, g_adbms.ics[i].memory.cfga.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGARA_t cfg;
    CFGARA_DECODE(g_adbms.ics[ic_index].memory.cfga.raw, &cfg);
    cfg.FLAG_D = flag_d;
    CFGARA_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfga.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }
  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetSoakOn(uint8_t ic_index, bool enable)
{
  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGARA_t cfg;
      CFGARA_DECODE(g_adbms.ics[i].memory.cfga.raw, &cfg);
      cfg.SOAKON = enable ? 1 : 0;
      CFGARA_ENCODE(&cfg, g_adbms.ics[i].memory.cfga.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGARA_t cfg;
    CFGARA_DECODE(g_adbms.ics[ic_index].memory.cfga.raw, &cfg);
    cfg.SOAKON = enable ? 1 : 0;
    CFGARA_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfga.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }
  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetOwrng(uint8_t ic_index, bool long_range)
{
  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGARA_t cfg;
      CFGARA_DECODE(g_adbms.ics[i].memory.cfga.raw, &cfg);
      cfg.OWRNG = long_range ? 1 : 0;
      CFGARA_ENCODE(&cfg, g_adbms.ics[i].memory.cfga.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGARA_t cfg;
    CFGARA_DECODE(g_adbms.ics[ic_index].memory.cfga.raw, &cfg);
    cfg.OWRNG = long_range ? 1 : 0;
    CFGARA_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfga.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }
  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetOWA(uint8_t ic_index, uint8_t owa)
{
  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGARA_t cfg;
      CFGARA_DECODE(g_adbms.ics[i].memory.cfga.raw, &cfg);
      cfg.OWA = owa & 0x07;
      CFGARA_ENCODE(&cfg, g_adbms.ics[i].memory.cfga.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGARA_t cfg;
    CFGARA_DECODE(g_adbms.ics[ic_index].memory.cfga.raw, &cfg);
    cfg.OWA = owa & 0x07;
    CFGARA_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfga.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }
  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetFC(uint8_t ic_index, ADBMS_FC_t fc)
{
  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGARA_t cfg;
      CFGARA_DECODE(g_adbms.ics[i].memory.cfga.raw, &cfg);
      cfg.FC = fc & 0x07;
      CFGARA_ENCODE(&cfg, g_adbms.ics[i].memory.cfga.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGARA_t cfg;
    CFGARA_DECODE(g_adbms.ics[ic_index].memory.cfga.raw, &cfg);
    cfg.FC = fc & 0x07;
    CFGARA_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfga.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }
  return ADBMS_OK;
}

ADBMS_Error_t ADBMS_SetCommBreak(uint8_t ic_index, bool enable)
{
  if (ic_index == 0xFF)
  {
    for (uint8_t i = 0; i < g_adbms.num_ics; i++)
    {
      CFGARA_t cfg;
      CFGARA_DECODE(g_adbms.ics[i].memory.cfga.raw, &cfg);
      cfg.COMM_BK = enable ? 1 : 0;
      CFGARA_ENCODE(&cfg, g_adbms.ics[i].memory.cfga.raw);
    }
  }
  else if (ic_index < g_adbms.num_ics)
  {
    CFGARA_t cfg;
    CFGARA_DECODE(g_adbms.ics[ic_index].memory.cfga.raw, &cfg);
    cfg.COMM_BK = enable ? 1 : 0;
    CFGARA_ENCODE(&cfg, g_adbms.ics[ic_index].memory.cfga.raw);
  }
  else
  {
    return ADBMS_ERR_INVALID_PARAM;
  }
  return ADBMS_OK;
}

/*============================================================================
 * Bulk Read Commands
 *============================================================================*/

ADBMS_Error_t ADBMS_ReadAllCellAndSVoltages(void)
{
  /* Read both C-voltages and S-voltages */
  ADBMS_Error_t err = ADBMS_ReadAllCellVoltages();
  if (err != ADBMS_OK)
    return err;
  return ADBMS_ReadAllSVoltages();
}

ADBMS_Error_t ADBMS_ReadAllAvgCellAndSVoltages(void)
{
  /* Read both averaged C-voltages and S-voltages */
  ADBMS_Error_t err = ADBMS_ReadAllAveragedVoltages();
  if (err != ADBMS_OK)
    return err;
  return ADBMS_ReadAllSVoltages();
}

ADBMS_Error_t ADBMS_ReadAllAuxAndStatus(void)
{
  /* Read both auxiliary and status registers */
  ADBMS_Error_t err = ADBMS_ReadAllAux();
  if (err != ADBMS_OK)
    return err;
  return ADBMS_ReadAllStatus();
}

/*============================================================================
 * LPCM Control Commands
 *============================================================================*/

ADBMS_Error_t ADBMS_LPCMDisable(void)
{
  return _transmit_action(CMDIS);
}

ADBMS_Error_t ADBMS_LPCMEnable(void)
{
  return _transmit_action(CMEN);
}

ADBMS_Error_t ADBMS_LPCMHeartbeat(void)
{
  return _transmit_action(CMHB);
}

ADBMS_Error_t ADBMS_ClearLPCMFlags(void)
{
  return _transmit_action(CLRCMFLAG);
}

ADBMS_Error_t ADBMS_ReadLPCMFlags(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CMFLAG);
}

ADBMS_Error_t ADBMS_WriteLPCMConfig(void)
{
  return ADBMS_WriteRegister(ADBMS_REG_CMCFG);
}

ADBMS_Error_t ADBMS_ReadLPCMConfig(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CMCFG);
}

ADBMS_Error_t ADBMS_WriteLPCMCellThresholds(void)
{
  return ADBMS_WriteRegister(ADBMS_REG_CMCELLT);
}

ADBMS_Error_t ADBMS_ReadLPCMCellThresholds(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CMCELLT);
}

ADBMS_Error_t ADBMS_WriteLPCMGPIOThresholds(void)
{
  return ADBMS_WriteRegister(ADBMS_REG_CMGPIOT);
}

ADBMS_Error_t ADBMS_ReadLPCMGPIOThresholds(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_CMGPIOT);
}

/*============================================================================
 * Additional ADC Commands
 *============================================================================*/

ADBMS_Error_t ADBMS_StartAux2ADC(uint8_t ch)
{
  return _transmit_action(ADAX2(ch));
}

ADBMS_Error_t ADBMS_PollAux2ADC(uint32_t timeout_ms)
{
  /* Build command frame once before loop */
  uint8_t cmd_buf[4];
  cmd_buf[0] = (PLAUX2 >> 8) & 0xFF;
  cmd_buf[1] = PLAUX2 & 0xFF;
  uint16_t pec = ADBMS_CalcPEC15(cmd_buf, 2);
  cmd_buf[2] = (pec >> 8) & 0xFF;
  cmd_buf[3] = pec & 0xFF;

  /* Ensure IC is awake before polling loop (only once) */
  _ensure_awake();

  uint32_t start = ADBMS_Platform_GetTickMs();
  uint8_t rx_byte;

  while ((ADBMS_Platform_GetTickMs() - start) < timeout_ms)
  {
    ADBMS_Platform_CS_Low();
    ADBMS_Platform_SPI_WriteRead(cmd_buf, 4, &rx_byte, 1);
    ADBMS_Platform_CS_High();

    if (rx_byte == 0xFF)
    {
      s_last_activity_ms = ADBMS_Platform_GetTickMs();
      return ADBMS_OK; /* Conversion complete */
    }

    ADBMS_Platform_DelayMs(1);
  }

  return ADBMS_ERR_TIMEOUT;
}

/*============================================================================
 * Communication Register Commands
 *============================================================================*/

ADBMS_Error_t ADBMS_WriteComm(void)
{
  return ADBMS_WriteRegister(ADBMS_REG_COMM);
}

ADBMS_Error_t ADBMS_ReadComm(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_COMM);
}

ADBMS_Error_t ADBMS_StartComm(void)
{
  return _transmit_action(STCOMM);
}

/*============================================================================
 * Retention Register Commands
 *============================================================================*/

ADBMS_Error_t ADBMS_UnlockRetention(void)
{
  return _transmit_action(ULRR);
}

ADBMS_Error_t ADBMS_WriteRetention(void)
{
  return ADBMS_WriteRegister(ADBMS_REG_RR);
}

ADBMS_Error_t ADBMS_ReadRetention(void)
{
  return ADBMS_ReadRegister(ADBMS_REG_RR);
}

/*============================================================================
 * Additional Status Getters
 *============================================================================*/

int32_t ADBMS_GetVRES_mV(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return -1;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATB_t stat_b;
  RDSTATB_DECODE(mem->stat.groups.b.raw, &stat_b);

  /* VRES: signed 16-bit code * 150uV/LSB + 1.5V offset */
  int16_t signed_code = (int16_t)stat_b.VRES;
  return ((int32_t)signed_code * 150) / 1000 + 1500;
}

uint8_t ADBMS_GetOscillatorCount(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return 0xFF;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATD_t stat_d;
  RDSTATD_DECODE(mem->stat.groups.d.raw, &stat_d);

  return stat_d.OC_CNTR;
}

uint16_t ADBMS_GetConversionCount(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return 0xFFFF;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATC_t stat_c;
  RDSTATC_DECODE(mem->stat.groups.c.raw, &stat_c);

  return stat_c.CT;
}

/*============================================================================
 * Status C Flag Getters (Table 108)
 *============================================================================*/

/* Macro to generate Status C flag getters */
#define STATC_FLAG_GETTER_IMPL(func_name, field_name)                                                                  \
  bool func_name(uint8_t ic_index)                                                                                     \
  {                                                                                                                    \
    if (ic_index >= g_adbms.num_ics)                                                                                   \
      return false;                                                                                                    \
    ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;                                                               \
    RDSTATC_t stat_c;                                                                                                  \
    RDSTATC_DECODE(mem->stat.groups.c.raw, &stat_c);                                                                   \
    return stat_c.field_name != 0;                                                                                     \
  }

STATC_FLAG_GETTER_IMPL(ADBMS_GetTHSDFlag, THSD)
STATC_FLAG_GETTER_IMPL(ADBMS_GetSleepFlag, SLEEP)
STATC_FLAG_GETTER_IMPL(ADBMS_GetCompFlag, COMP)
STATC_FLAG_GETTER_IMPL(ADBMS_GetSPIFaultFlag, SPIFLT)
STATC_FLAG_GETTER_IMPL(ADBMS_GetOscCheckFlag, OSCCHK)
STATC_FLAG_GETTER_IMPL(ADBMS_GetTestModeFlag, TMODCHK)
STATC_FLAG_GETTER_IMPL(ADBMS_GetVDEFlag, VDE)
STATC_FLAG_GETTER_IMPL(ADBMS_GetVDELFlag, VDEL)
STATC_FLAG_GETTER_IMPL(ADBMS_GetVA_OVFlag, VA_OV)
STATC_FLAG_GETTER_IMPL(ADBMS_GetVA_UVFlag, VA_UV)
STATC_FLAG_GETTER_IMPL(ADBMS_GetVD_OVFlag, VD_OV)
STATC_FLAG_GETTER_IMPL(ADBMS_GetVD_UVFlag, VD_UV)
STATC_FLAG_GETTER_IMPL(ADBMS_GetCEDFlag, CED)
STATC_FLAG_GETTER_IMPL(ADBMS_GetCMEDFlag, CMED)
STATC_FLAG_GETTER_IMPL(ADBMS_GetSEDFlag, SED)
STATC_FLAG_GETTER_IMPL(ADBMS_GetSMEDFlag, SMED)

uint16_t ADBMS_GetCellShortFaultMask(uint8_t ic_index)
{
  if (ic_index >= g_adbms.num_ics)
    return 0xFFFF;

  ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;
  RDSTATC_t stat_c;
  RDSTATC_DECODE(mem->stat.groups.c.raw, &stat_c);

  return stat_c.CS_FLT;
}

/*============================================================================
 * LPCM Flag Getters (Table 115)
 *============================================================================*/

/* Macro to generate LPCM flag getters */
#define CMFLAG_GETTER_IMPL(func_name, field_name)                                                                      \
  bool func_name(uint8_t ic_index)                                                                                     \
  {                                                                                                                    \
    if (ic_index >= g_adbms.num_ics)                                                                                   \
      return false;                                                                                                    \
    ADBMS_Memory_t *mem = &g_adbms.ics[ic_index].memory;                                                               \
    CMFLAG_t flags;                                                                                                    \
    CMFLAG_DECODE(mem->cmflag.raw, &flags);                                                                            \
    return flags.field_name != 0;                                                                                      \
  }

CMFLAG_GETTER_IMPL(ADBMS_GetLPCMEnabled, CMC_EN)
CMFLAG_GETTER_IMPL(ADBMS_GetLPCM_CUVFlag, CMF_CUV)
CMFLAG_GETTER_IMPL(ADBMS_GetLPCM_COVFlag, CMF_COV)
CMFLAG_GETTER_IMPL(ADBMS_GetLPCM_CDVPFlag, CMF_CDVP)
CMFLAG_GETTER_IMPL(ADBMS_GetLPCM_CDVNFlag, CMF_CDVN)
CMFLAG_GETTER_IMPL(ADBMS_GetLPCM_GUVFlag, CMF_GUV)
CMFLAG_GETTER_IMPL(ADBMS_GetLPCM_GOVFlag, CMF_GOV)
CMFLAG_GETTER_IMPL(ADBMS_GetLPCM_BTMWDFlag, CMF_BTMWD)
CMFLAG_GETTER_IMPL(ADBMS_GetLPCM_BTMCMPFlag, CMF_BTMCMP)

/*============================================================================
 * Threshold Conversion Utilities (Table 107)
 *============================================================================*/

uint16_t ADBMS_ThresholdCodeToMV(uint16_t code)
{
  /* V = code * 16 * 150uV + 1.5V = code * 2.4mV + 1500mV */
  return (uint16_t)(((uint32_t)code * 24) / 10 + 1500);
}

uint16_t ADBMS_MVToThresholdCode(uint16_t voltage_mV)
{
  /* code = (V - 1.5V) / 2.4mV */
  if (voltage_mV < 1500)
    return 0;
  return (uint16_t)(((uint32_t)(voltage_mV - 1500) * 10) / 24);
}
