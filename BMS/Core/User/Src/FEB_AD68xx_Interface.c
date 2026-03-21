// ********************************** Includes & Externs *************************

#include "FEB_AD68xx_Interface.h"
#include "FEB_HW.h"
#include "FEB_Const.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

// ******************************** CRC TABLES ********************************
const uint16_t crc15Table[256] = {
    0x0,    0xc599, 0xceab, 0xb32,  0xd8cf, 0x1d56, 0x1664, 0xd3fd, 0xf407, 0x319e, 0x3aac, // precomputed CRC15 Table
    0xff35, 0x2cc8, 0xe951, 0xe263, 0x27fa, 0xad97, 0x680e, 0x633c, 0xa6a5, 0x7558, 0xb0c1, 0xbbf3, 0x7e6a, 0x5990,
    0x9c09, 0x973b, 0x52a2, 0x815f, 0x44c6, 0x4ff4, 0x8a6d, 0x5b2e, 0x9eb7, 0x9585, 0x501c, 0x83e1, 0x4678, 0x4d4a,
    0x88d3, 0xaf29, 0x6ab0, 0x6182, 0xa41b, 0x77e6, 0xb27f, 0xb94d, 0x7cd4, 0xf6b9, 0x3320, 0x3812, 0xfd8b, 0x2e76,
    0xebef, 0xe0dd, 0x2544, 0x2be,  0xc727, 0xcc15, 0x98c,  0xda71, 0x1fe8, 0x14da, 0xd143, 0xf3c5, 0x365c, 0x3d6e,
    0xf8f7, 0x2b0a, 0xee93, 0xe5a1, 0x2038, 0x7c2,  0xc25b, 0xc969, 0xcf0,  0xdf0d, 0x1a94, 0x11a6, 0xd43f, 0x5e52,
    0x9bcb, 0x90f9, 0x5560, 0x869d, 0x4304, 0x4836, 0x8daf, 0xaa55, 0x6fcc, 0x64fe, 0xa167, 0x729a, 0xb703, 0xbc31,
    0x79a8, 0xa8eb, 0x6d72, 0x6640, 0xa3d9, 0x7024, 0xb5bd, 0xbe8f, 0x7b16, 0x5cec, 0x9975, 0x9247, 0x57de, 0x8423,
    0x41ba, 0x4a88, 0x8f11, 0x57c,  0xc0e5, 0xcbd7, 0xe4e,  0xddb3, 0x182a, 0x1318, 0xd681, 0xf17b, 0x34e2, 0x3fd0,
    0xfa49, 0x29b4, 0xec2d, 0xe71f, 0x2286, 0xa213, 0x678a, 0x6cb8, 0xa921, 0x7adc, 0xbf45, 0xb477, 0x71ee, 0x5614,
    0x938d, 0x98bf, 0x5d26, 0x8edb, 0x4b42, 0x4070, 0x85e9, 0xf84,  0xca1d, 0xc12f, 0x4b6,  0xd74b, 0x12d2, 0x19e0,
    0xdc79, 0xfb83, 0x3e1a, 0x3528, 0xf0b1, 0x234c, 0xe6d5, 0xede7, 0x287e, 0xf93d, 0x3ca4, 0x3796, 0xf20f, 0x21f2,
    0xe46b, 0xef59, 0x2ac0, 0xd3a,  0xc8a3, 0xc391, 0x608,  0xd5f5, 0x106c, 0x1b5e, 0xdec7, 0x54aa, 0x9133, 0x9a01,
    0x5f98, 0x8c65, 0x49fc, 0x42ce, 0x8757, 0xa0ad, 0x6534, 0x6e06, 0xab9f, 0x7862, 0xbdfb, 0xb6c9, 0x7350, 0x51d6,
    0x944f, 0x9f7d, 0x5ae4, 0x8919, 0x4c80, 0x47b2, 0x822b, 0xa5d1, 0x6048, 0x6b7a, 0xaee3, 0x7d1e, 0xb887, 0xb3b5,
    0x762c, 0xfc41, 0x39d8, 0x32ea, 0xf773, 0x248e, 0xe117, 0xea25, 0x2fbc, 0x846,  0xcddf, 0xc6ed, 0x374,  0xd089,
    0x1510, 0x1e22, 0xdbbb, 0xaf8,  0xcf61, 0xc453, 0x1ca,  0xd237, 0x17ae, 0x1c9c, 0xd905, 0xfeff, 0x3b66, 0x3054,
    0xf5cd, 0x2630, 0xe3a9, 0xe89b, 0x2d02, 0xa76f, 0x62f6, 0x69c4, 0xac5d, 0x7fa0, 0xba39, 0xb10b, 0x7492, 0x5368,
    0x96f1, 0x9dc3, 0x585a, 0x8ba7, 0x4e3e, 0x450c, 0x8095};

const uint16_t crc10Table[256] = {
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

// ****************** Error Correction *******************
/* Calculates and returns the CRC15 */
uint16_t pec15_calc(uint8_t len,  // Number of bytes that will be used to calculate a PEC
                    uint8_t *data // Array of data that will be used to calculate a PEC
)
{
  uint16_t remainder, addr;
  remainder = 16; // initialize the PEC

  for (uint8_t i = 0; i < len; i++)
  {                                             // loops for each byte in data array
    addr = ((remainder >> 7) ^ data[i]) & 0xff; // calculate PEC table address
    remainder = (remainder << 8) ^ crc15Table[addr];
  }

  return (remainder * 2); // The CRC15 has a 0 in the LSB so the remainder must be multiplied by 2
}

/* Calculates and returns the CRC10 */
uint16_t Pec10_calc(bool bIsRxCmd, uint8_t nLength, uint8_t *pDataBuf)
{
  uint16_t nRemainder = 16u; /* PEC_SEED */
  /* x10 + x7 + x3 + x2 + x + 1 <- the CRC10 polynomial 100 1000 1111 */
  uint16_t nPolynomial = 0x8Fu;
  uint8_t nByteIndex, nBitIndex;

  for (nByteIndex = 0u; nByteIndex < nLength; ++nByteIndex)
  {
    /* Bring the next byte into the remainder. */
    nRemainder ^= (uint16_t)((uint16_t)pDataBuf[nByteIndex] << 2u);

    /* Perform modulo-2 division, a bit at a time. */
    for (nBitIndex = 8u; nBitIndex > 0u; --nBitIndex)
    {
      /* Try to divide the current data bit. */
      if ((nRemainder & 0x200u) > 0u)
      {
        nRemainder = (uint16_t)((nRemainder << 1u));
        nRemainder = (uint16_t)(nRemainder ^ nPolynomial);
      }
      else
      {
        nRemainder = (uint16_t)(nRemainder << 1u);
      }
    }
  }

  /* If array is from received buffer add command counter to crc calculation */
  if (bIsRxCmd == true)
  {
    nRemainder ^= (uint16_t)(((uint16_t)pDataBuf[nLength] & (uint8_t)0xFC) << 2u);
  }
  /* Perform modulo-2 division, a bit at a time */
  for (nBitIndex = 6u; nBitIndex > 0u; --nBitIndex)
  {
    /* Try to divide the current data bit */
    if ((nRemainder & 0x200u) > 0u)
    {
      nRemainder = (uint16_t)((nRemainder << 1u));
      nRemainder = (uint16_t)(nRemainder ^ nPolynomial);
    }
    else
    {
      nRemainder = (uint16_t)((nRemainder << 1u));
    }
  }
  return ((uint16_t)(nRemainder & 0x3FFu));
}
uint16_t pec10_calc(uint8_t nLength, uint8_t *pDataBuf)
{
  bool bIsRxCmd = true;
  uint16_t nRemainder = 16u; /* PEC_SEED */
  /* x10 + x7 + x3 + x2 + x + 1 <- the CRC10 polynomial 100 1000 1111 */
  uint16_t nPolynomial = 0x8Fu;
  uint8_t nByteIndex, nBitIndex;

  for (nByteIndex = 0u; nByteIndex < nLength; ++nByteIndex)
  {
    /* Bring the next byte into the remainder. */
    nRemainder ^= (uint16_t)((uint16_t)pDataBuf[nByteIndex] << 2u);

    /* Perform modulo-2 division, a bit at a time. */
    for (nBitIndex = 8u; nBitIndex > 0u; --nBitIndex)
    {
      /* Try to divide the current data bit. */
      if ((nRemainder & 0x200u) > 0u)
      {
        nRemainder = (uint16_t)((nRemainder << 1u));
        nRemainder = (uint16_t)(nRemainder ^ nPolynomial);
      }
      else
      {
        nRemainder = (uint16_t)(nRemainder << 1u);
      }
    }
  }

  /* If array is from received buffer add command counter to crc calculation */
  if (bIsRxCmd == true)
  {
    nRemainder ^= (uint16_t)(((uint16_t)pDataBuf[nLength] & (uint8_t)0xFC) << 2u);
  }
  /* Perform modulo-2 division, a bit at a time */
  for (nBitIndex = 6u; nBitIndex > 0u; --nBitIndex)
  {
    /* Try to divide the current data bit */
    if ((nRemainder & 0x200u) > 0u)
    {
      nRemainder = (uint16_t)((nRemainder << 1u));
      nRemainder = (uint16_t)(nRemainder ^ nPolynomial);
    }
    else
    {
      nRemainder = (uint16_t)((nRemainder << 1u));
    }
  }
  return ((uint16_t)(nRemainder & 0x3FFu));
}
//***************** Read and Write to SPI ****************
/* Generic function to write 68xx commands. Function calculates PEC for tx_cmd data. */
void cmd_68(uint8_t tx_cmd[2])
{ // The command to be transmitted
  uint8_t cmd[4];
  uint16_t cmd_pec;

  cmd[0] = tx_cmd[0];
  cmd[1] = tx_cmd[1];
  cmd_pec = pec15_calc(2, cmd);
  cmd[2] = (uint8_t)(cmd_pec >> 8);
  cmd[3] = (uint8_t)(cmd_pec);
  wakeup_sleep(FEB_NUM_IC);
  FEB_cs_low();
  FEB_spi_write_array(4, cmd);
  FEB_cs_high();
}

void cmd_68_r(uint8_t tx_cmd[2], uint8_t *data, uint8_t len)
{ // The command to be transmitted
  uint8_t cmd[4];
  uint16_t cmd_pec;

  cmd[0] = tx_cmd[0];
  cmd[1] = tx_cmd[1];
  cmd_pec = pec15_calc(2, cmd);
  cmd[2] = (uint8_t)(cmd_pec >> 8);
  cmd[3] = (uint8_t)(cmd_pec);
  wakeup_sleep(FEB_NUM_IC);
  FEB_cs_low();
  FEB_spi_write_read(cmd, 4, data, len);
  FEB_cs_high();

  // Debug: print raw received bytes (commented out to reduce log spam)
  // printf("[SPI] TX: %02X %02X, RX:", tx_cmd[0], tx_cmd[1]);
  // for (int i = 0; i < len && i < 16; i++)
  //   printf(" %02X", data[i]);
  // printf("\r\n");
}
/*
Generic function to write 68xx commands and write payload data.
Function calculates PEC for tx_cmd data and the data to be transmitted.
*/
void write_68(uint8_t total_ic,  // Number of ICs to be written to
              uint8_t tx_cmd[2], // The command to be transmitted
              uint8_t data[]     // Payload Data
)
{
  const uint8_t BYTES_IN_REG = 6;
  const uint8_t BYTES_PER_IC = BYTES_IN_REG + 2;   // data + 2 bytes PEC
  uint8_t CMD_LEN = 4 + (BYTES_PER_IC * total_ic); // 4 bytes for cmd + data for all ICs
  uint8_t *cmd = (uint8_t *)pvPortMalloc(CMD_LEN * sizeof(uint8_t));
  if (cmd == NULL)
  {
    // Could print error here if needed
    return;
  }

  // Fill the 2-byte command
  cmd[0] = tx_cmd[0];
  cmd[1] = tx_cmd[1];

  // Calculate and append the PEC for the command
  uint16_t cmd_pec = pec15_calc(2, cmd);
  cmd[2] = (uint8_t)(cmd_pec >> 8);
  cmd[3] = (uint8_t)(cmd_pec);

  uint8_t cmd_index = 4;

  // For each IC, starting from last in chain to first
  for (uint8_t ic = total_ic; ic > 0; ic--)
  {
    uint8_t src_offset = (ic - 1) * BYTES_IN_REG;

    // Copy data bytes for this IC
    for (uint8_t byte = 0; byte < BYTES_IN_REG; byte++)
    {
      cmd[cmd_index++] = data[src_offset + byte];
    }

    // Calculate and append PEC for this IC's data
    uint16_t data_pec = (uint16_t)Pec10_calc(false, BYTES_IN_REG, &data[src_offset]);
    cmd[cmd_index++] = (uint8_t)(data_pec >> 8);
    cmd[cmd_index++] = (uint8_t)data_pec;
  }

  // Send command and payload
  // taskENTER_CRITICAL();
  wakeup_sleep(FEB_NUM_IC);
  FEB_cs_low();
  FEB_spi_write_array(CMD_LEN, cmd);
  FEB_cs_high();
  // taskEXIT_CRITICAL();

  vPortFree(cmd);
}

//****************** CMD Translation ****************************
void transmitCMD(uint16_t cmdcode)
{
  uint8_t cmd[2];
  cmd[0] = (cmdcode / 0x100); // selects first byte
  cmd[1] = (cmdcode % 0x100); // selects second byte
  cmd_68(cmd);
}

void transmitCMDR(uint16_t cmdcode, uint8_t *data, uint8_t len)
{
  uint8_t cmd[2];
  cmd[0] = (cmdcode / 0x100); // selects first byte
  cmd[1] = (cmdcode % 0x100); // selects second byte
  cmd_68_r(cmd, data, len);
}

void transmitCMDW(uint16_t cmdcode, uint8_t *data)
{
  uint8_t cmd[2];
  cmd[0] = (cmdcode / 0x100); // selects first byte
  cmd[1] = (cmdcode % 0x100); // selects second byte
  write_68(FEB_NUM_IC, cmd, data);
}
