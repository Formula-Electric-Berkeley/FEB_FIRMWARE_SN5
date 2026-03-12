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
  */
uint16_t Pec10_calc(bool bIsRxCmd, uint8_t nLength, uint8_t *pDataBuf);

/*!
 Calculates and returns the CRC10 (legacy wrapper, assumes RX command)
 @deprecated Use Pec10_calc() instead
 @returns The calculated pec10 as an unsigned int
  */
uint16_t pec10_calc(uint8_t len,  //!< The length of the data array being passed to the function
                    uint8_t *data //!< The array of data that the PEC will be generated from
);

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
