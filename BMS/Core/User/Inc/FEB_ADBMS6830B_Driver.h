#ifndef INC_FEB_ADBMS6830B_DRIVER_H_
#define INC_FEB_ADBMS6830B_DRIVER_H_

// ********************************** Includes ***********************************

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// ********************************** Macros *************************************

#define NUM_RX_BYT 8
#define REG_DATA_SIZE 6
#define REG_WITH_PEC_SIZE 8
#define MAX_WRITE_BUFFER_SIZE 256
#define ADC_POLL_TIMEOUT 200000
#define ADC_POLL_INCREMENT 10

#define CFGRA 0
#define CFGRB 1
#define CELL 2
#define AUX 3
#define STAT 4

// Bit masks for configuration
#define REFON_MASK 0x80
#define REFON_CLEAR_MASK 0x7F
#define DCC_LOW_MASK 0xFF
#define DCC_HIGH_MASK 0xFF
#define BYTE_MASK 0xFF
#define NIBBLE_MASK 0x0F
#define HALF_BYTE_SHIFT 4

// ********************************** Structs ************************************

/*! Cell Voltage data structure. */
typedef struct
{
  uint16_t c_codes[18]; //!< Cell Voltage Codes
  uint16_t s_codes[18]; //!< Cell Voltage Codes
  uint8_t pec_match[6]; //!< If a PEC error was detected during most recent read cmd
} cv;

/*! AUX Reg Voltage Data structure */
typedef struct
{
  uint16_t a_codes[10]; //!< Aux Voltage Codes
  uint8_t pec_match[4]; //!< If a PEC error was detected during most recent read cmd
} ax;

/*! Status Reg data structure. */
typedef struct
{
  uint16_t stat_codes[4]; //!< Status codes.
  uint8_t flags[3];       //!< Byte array that contains the uv/ov flag data
  uint8_t mux_fail[1];    //!< Mux self test status flag
  uint8_t thsd[1];        //!< Thermal shutdown status
  uint8_t pec_match[2];   //!< If a PEC error was detected during most recent read cmd
} st;

/*! IC register structure. */
typedef struct
{
  uint8_t tx_data[6];   //!< Stores data to be transmitted
  uint8_t rx_data[8];   //!< Stores received data
  uint8_t rx_pec_match; //!< If a PEC error was detected during most recent read cmd
} ic_register;

/*! PEC error counter structure. */
typedef struct
{
  uint16_t pec_count;   //!< Overall PEC error count
  uint16_t cfgr_pec;    //!< Configuration register data PEC error count
  uint16_t cell_pec[6]; //!< Cell voltage register data PEC error count
  uint16_t aux_pec[4];  //!< Aux register data PEC error count
  uint16_t stat_pec[2]; //!< Status register data PEC error count
} pec_counter;

/*! Register configuration structure */
typedef struct
{
  uint8_t cell_channels; //!< Number of Cell channels
  uint8_t stat_channels; //!< Number of Stat channels
  uint8_t aux_channels;  //!< Number of Aux channels
  uint8_t num_cv_reg;    //!< Number of Cell voltage register
  uint8_t num_gpio_reg;  //!< Number of Aux register
  uint8_t num_stat_reg;  //!< Number of  Status register
} register_cfg;

/*! Cell variable structure */
typedef struct
{
  ic_register configa;
  ic_register configb;
  cv cells;
  ax aux;
  st stat;
  ic_register com;
  ic_register pwm;
  ic_register pwmb;
  ic_register sctrl;
  ic_register sctrlb;
  uint8_t sid[6];
  bool isospi_reverse;
  pec_counter crc_count;
  register_cfg ic_reg;
  long system_open_wire;
} cell_asic;

// ********************************** Initialization *****************************

/*!
 Helper Function to initialize the CFGR data structures
 @return void
 */
void ADBMS6830B_init_cfg(uint8_t total_ic, //!< Number of ICs in the daisy chain
                         cell_asic *ic     //!< A two dimensional array that will store the data
);

/* Initialize the Register limits */
void ADBMS6830B_init_reg_limits(uint8_t total_ic, // The number of ICs in the system
                                cell_asic *ic     // A two dimensional array where data will be written
);

/*!
 Helper Function that resets the PEC error counters
 @return void
 */
void ADBMS6830B_reset_crc_count(uint8_t total_ic, //!< Number of ICs in the daisy chain
                                cell_asic *ic     //!< A two dimensional array that will store the data
);

void ADBMS6830B_set_cfgr(uint8_t nIC,   //!< Current IC
                         cell_asic *ic, //!< A two dimensional array that will store the data
                         bool refon,    //!< The REFON bit
                         bool cth[3],   //!< The CTH bits
                         bool gpio[10], //!< The GPIO bits
                         uint16_t dcc,  //!< The DCC bits
                         bool dcto[6],  //!< The Dcto bits
                         uint16_t uv,   //!< The UV value
                         uint16_t ov    //!< The OV value
);

/*!
 Helper function to turn the REFON bit HIGH or LOW
 @return void
 */
void ADBMS6830B_set_cfgr_refon(uint8_t nIC,   //!< Current IC
                               cell_asic *ic, //!< A two dimensional array that will store the data
                               bool refon     //!< The REFON bit
);

/*!
 Helper function to turn the ADCOPT bit HIGH or LOW
 @return void
 */
void ADBMS6830B_set_cfgr_cth(uint8_t nIC,   //!< Current IC
                             cell_asic *ic, //!< A two dimensional array that will store the data
                             bool cth[3]    //!< The ADCOPT bit
);

/*!
 Helper function to turn the GPIO bits HIGH or LOW
 @return void
 */
void ADBMS6830B_set_cfgr_gpio(uint8_t nIC,   //!< Current IC
                              cell_asic *ic, //!< A two dimensional array that will store the data
                              bool gpio[10]  //!< The GPIO bits
);

/*!
 Helper function to turn the DCC bits HIGH or LOW
 @return void
 */
void ADBMS6830B_set_cfgr_dis(uint8_t nIC,   //!< Current IC
                             cell_asic *ic, //!< A two dimensional array that will store the data
                             uint16_t dcc   //!< The DCC bits
);

/*!
 Helper function to control discharge time value
 @return void
 */
void ADBMS6830B_set_cfgr_dcto(uint8_t nIC,   //!< Current IC
                              cell_asic *ic, //!< A two dimensional array that will store the data
                              bool dcto[6]   //!< The Dcto bits
);

/*!
 Helper function to set uv field in CFGRA register
 @return void
 */
void ADBMS6830B_set_cfgr_uv(uint8_t nIC,   //!< Current IC
                            cell_asic *ic, //!< A two dimensional array that will store the data
                            uint16_t uv    //!< The UV value
);

/*!
 Helper function to set ov field in CFGRA register
 @return void
 */
void ADBMS6830B_set_cfgr_ov(uint8_t nIC,   //!< Current IC
                            cell_asic *ic, //!< A two dimensional array that will store the data
                            uint16_t ov    //!< The OV value
);

// ******************************** Voltage ********************************

/*!
 Starts cell voltage conversion
 Starts ADC conversions of the ADBMS6830B Cpin inputs.
 The type of ADC conversion executed can be changed by setting the following parameters:
 @return void
 */
void ADBMS6830B_adcv(uint8_t RD,   // ADC Mode
                     uint8_t DCP,  // Discharge Permit
                     uint8_t CONT, // Cell Channels to be measured
                     uint8_t RSTF, // Reset Filter
                     uint8_t OW    // Open-wire bits
);

/*!
  This function will block operation until the ADC has finished it's conversion
  @returns uint32_t counter The approximate time it took for the ADC function to complete.
  */
uint32_t ADBMS6830B_pollAdc();

/*!
 Reads and parses the LTC681x cell voltage registers.
 The function is used to read the cell codes of the LTC681x.
 This function will send the requested read commands parse the data and store the cell voltages in the cell_asic
 structure.
 @return uint8_t, PEC Status.
  0: No PEC error detected
 -1: PEC error detected, retry read
 */
uint8_t ADBMS6830B_rdcv(uint8_t total_ic, //!< The number of ICs in the system
                        cell_asic *ic     //!< Array of the parsed cell codes
);
uint8_t ADBMS6830B_rdsv(uint8_t total_ic, // The number of ICs in the system
                        cell_asic *ic     // Array of the parsed cell codes
);
/*!
 Helper function that parses voltage measurement registers
 @return int8_t, pec_error PEC Status.
  0: Data read back has matching PEC
 -1: Data read back has incorrect PEC
 */
int8_t parse_cells(uint8_t current_ic,   //!< Current IC
                   uint8_t cell_reg,     //!< Type of register
                   uint8_t cell_data[],  //!< Unparsed data
                   uint16_t *cell_codes, //!< Parsed data
                   uint8_t *ic_pec       //!< PEC error
);

// ******************************** Temperature Functions ********************************
void ADBMS6830B_wrALL(uint8_t total_ic, // The number of ICs being written to
                      cell_asic ic[]    // A two dimensional array of the configuration data that will be written
);
void ADBMS6830B_rdALL(uint8_t total_ic, // The number of ICs being written to
                      cell_asic ic[]    // A two dimensional array of the configuration data that will be written
);

/*!
 Write the ADBMS6830B CFGRA register
 This command will write the configuration registers of the LTC681xs connected in a daisy chain stack.
 The configuration is written in descending order so the last device's configuration is written first.
 @return void
 */
void ADBMS6830B_wrcfga(uint8_t total_ic, //!< The number of ICs being written to
                       cell_asic *ic     //!< A two dimensional array of the configuration data that will be written
);
void ADBMS6830B_rdcfga(uint8_t total_ic, // The number of ICs being written to
                       cell_asic ic[]    // A two dimensional array of the configuration data that will be written
);
/*!
 Write the ADBMS6830B CFGRB register
 This command will write the configuration registers of the LTC681xs connected in a daisy chain stack.
 The configuration is written in descending order so the last device's configuration is written first.
 @return void
 */
void ADBMS6830B_wrcfgb(uint8_t total_ic, //!< The number of ICs being written to
                       cell_asic *ic     //!< A two dimensional array of the configuration data that will be written
);
void ADBMS6830B_rdcfgb(uint8_t total_ic, // The number of ICs being written to
                       cell_asic ic[]    // A two dimensional array of the configuration data that will be written
);

void ADBMS6830B_wrpwmga(uint8_t total_ic, // The number of ICs being written to
                        cell_asic ic[]    // A two dimensional array of the configuration data that will be written
);
void ADBMS6830B_rdpwmga(uint8_t total_ic, // The number of ICs being written to
                        cell_asic ic[]    // A two dimensional array of the configuration data that will be written
);
void ADBMS6830B_wrpwmgb(uint8_t total_ic, // The number of ICs being written to
                        cell_asic ic[]    // A two dimensional array of the configuration data that will be written
);
void ADBMS6830B_rdpwmgb(uint8_t total_ic, // The number of ICs being written to
                        cell_asic ic[]    // A two dimensional array of the configuration data that will be written
);

/* Start ADC Conversion for GPIO and Vref2  */
void ADBMS6830B_adax(uint8_t OW,  // Open Wire Detection
                     uint8_t PUP, // Pull up/pull down current sources during measurement
                     uint8_t CH   // GPIO Channels to be measured
);

/*
The function is used to read the parsed GPIO codes of the ADBMS6830B.
This function will send the requested read commands parse the data
and store the gpio voltages in a_codes variable.
*/

uint8_t ADBMS6830B_rdaux(uint8_t total_ic, // The number of ICs in the system
                         cell_asic *ic     // Array of the parsed cell codes
);

/*!
 Read the unique 48-bit Serial ID from each ADBMS6830B in the daisy chain.
 The Serial ID is factory-programmed and unique to each device.
 @return uint8_t, PEC Status (0: no error, non-zero: error)
 */
uint8_t ADBMS6830B_rdsid(uint8_t total_ic, // The number of ICs in the system
                         cell_asic *ic     // Array to store the serial IDs
);

// ******************************** Auxilary Functions ********************************
/*!
 Helper Function that counts overall PEC errors and register/IC PEC errors
 @return void
 */
void ADBMS6830B_check_pec(uint8_t total_ic, //!< Number of ICs in the daisy chain
                          uint8_t reg,      //!< Type of register
                          cell_asic *ic     //!< A two dimensional array that will store the data
);
void ADBMS6830B_CLRFLAG(uint8_t total_ic);

#endif /* INC_FEB_ADBMS6830B_DRIVER_H_ */
