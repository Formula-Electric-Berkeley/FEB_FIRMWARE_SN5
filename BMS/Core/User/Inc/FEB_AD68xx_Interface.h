#ifndef INC_FEB_AD68XX_INTERFACE_H_
#define INC_FEB_AD68XX_INTERFACE_H_

// ********************************** Includes ***********************************

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// ********************************** Functions **********************************

// ****************** Error Correction *******************
/*!
 Calculates  and returns the CRC15
 @returns The calculated pec15 as an unsigned int
  */
uint16_t pec15_calc(uint8_t len,  //!< The length of the data array being passed to the function
                    uint8_t *data //!< The array of data that the PEC will be generated from
);

/*!
 Calculates and returns the CRC10 with optional command counter support
 @param bIsRxCmd If true, includes command counter in calculation (for RX validation)
 @param nLength The length of the data array
 @param pDataBuf The data array to calculate PEC from
 @returns The calculated pec10 as an unsigned int (10-bit, masked to 0x3FF)
 @note When bIsRxCmd is true, pDataBuf must point to at least nLength+1
       bytes — byte nLength is read as the command-counter byte.
  */
uint16_t Pec10_calc(bool bIsRxCmd, uint8_t nLength, uint8_t *pDataBuf);

//***************** Command-counter tracking ****************
// The ADBMS6830 increments its 6-bit command counter on action/conversion and
// register-write commands, but NOT on read or poll commands. The host mirrors
// it: ADBMS_CC_Advance() is called automatically by cmd_68 (actions) and
// write_68 (writes) — NOT by cmd_68_r (reads/polls). ADBMS_CC_Check(observed)
// is called once per read by the read functions, with observed extracted from
// the first IC's response as ((rx_byte_6 >> 2) & 0x3F). Mismatches indicate
// dropped commands or chip resets and are rate-limited.
//
// Caveat: ADBMS6830B_pollAdc() issues the ADSV *action* via transmitCMDR (the
// read path), so that one action goes uncounted by the mirror. It is not used
// in the main measurement loop (osDelay is used instead), so it does not
// perturb steady-state tracking.
//
// Not reentrant / not thread-safe — single shared s_expected_cc state.
// Call from one task only, or wrap in external synchronization.
void ADBMS_CC_Advance(void);
void ADBMS_CC_Reset(void);
void ADBMS_CC_Check(uint8_t observed);
uint16_t ADBMS_CC_GetMismatchCount(void);

//***************** Read and Write to SPI ****************
/*!
 Sends a command to the BMS IC. This code will calculate the PEC code for the transmitted command
 @return void
 */
void cmd_68(uint8_t tx_cmd[2]); //!< 2 byte array containing the BMS command to be sent
void cmd_68_r(uint8_t tx_cmd[2], uint8_t *data, uint8_t len);
/*!
 Writes an array of data to the daisy chain
 @return void
 */
void write_68(uint8_t total_ic,  //!< Number of ICs in the daisy chain
              uint8_t tx_cmd[2], //!< 2 byte array containing the BMS command to be sent
              uint8_t data[]     //!< Array containing the data to be written to the BMS ICs
);

//****************** CMD Translation ****************************
// params: 16b command code (and optionally a data buffer)
// returns PEC error
void transmitCMD(uint16_t cmdcode);
void transmitCMDR(uint16_t cmdcode, uint8_t *data, uint8_t len);
void transmitCMDW(uint16_t cmdcode, uint8_t *data);

#endif
