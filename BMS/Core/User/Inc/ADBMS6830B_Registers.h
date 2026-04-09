/**
 * @file ADBMS6830B_Registers.h
 * @brief ADBMS6830B Register Structures and API
 *
 * Provides typed access to ADBMS6830B registers with bit-field definitions
 * matching the datasheet. All register groups are 6 bytes.
 */

#ifndef ADBMS6830B_REGISTERS_H
#define ADBMS6830B_REGISTERS_H

#include <stdint.h>
#include <stdbool.h>
#include "ADBMS6830B_Commands.h"

/*============================================================================
 * Constants
 *============================================================================*/
#define ADBMS_REG_SIZE 6      // All registers are 6 bytes
#define ADBMS_REG_PEC_SIZE 8  // Register + 2-byte PEC
#define ADBMS_SID_SIZE 6      // Serial ID is 6 bytes (48 bits)
#define ADBMS_CELLS_PER_REG 3 // 3 cell voltages per register group
#define ADBMS_TOTAL_CELLS 18  // Max cells per IC
#define ADBMS_TOTAL_GPIO 10   // 10 GPIO channels

/*============================================================================
 * Command Type Classification
 *============================================================================*/
typedef enum
{
  ADBMS_CMD_WRITE,  // Write data to device
  ADBMS_CMD_READ,   // Read data from device
  ADBMS_CMD_ACTION, // Action command (no data transfer)
  ADBMS_CMD_POLL    // Poll command (returns status byte)
} ADBMS_CmdType_t;

/*============================================================================
 * Command Information Structure
 *============================================================================*/
typedef struct
{
  uint16_t code;        // 11-bit command code
  ADBMS_CmdType_t type; // Command type
  uint8_t len;          // Data length (0 for action commands)
  const char *name;     // Datasheet command name
  const char *desc;     // Brief description
} ADBMS_CmdInfo_t;

/*============================================================================
 * Configuration Register A (CFGA) - 6 bytes
 *
 * Byte 0: CTH[2:0], REFON, FLAG_D[3:0]
 * Byte 1: FC[7:0]
 * Byte 2: SOAKON, OWRNG, Reserved[5:0]
 * Byte 3: GPO[7:0] (GPIO 1-8 pull-down)
 * Byte 4: GPO[9:8], Reserved[5:0]
 * Byte 5: Reserved
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    /* Byte 0 */
    uint8_t CTH : 3;    // Comparison threshold (000-111)
    uint8_t REFON : 1;  // Reference powered on
    uint8_t FLAG_D : 4; // Flag D clear bits
    /* Byte 1 */
    uint8_t FC : 8; // Fault clear bits
    /* Byte 2 */
    uint8_t SOAKON : 1; // S-ADC on during soak time
    uint8_t OWRNG : 1;  // Open-wire ranging
    uint8_t _rsvd0 : 6; // Reserved
    /* Byte 3 */
    uint8_t GPO_1_8 : 8; // GPIO 1-8 pull-down control
    /* Byte 4 */
    uint8_t GPO_9_10 : 2; // GPIO 9-10 pull-down control
    uint8_t _rsvd1 : 6;   // Reserved
    /* Byte 5 */
    uint8_t _rsvd2 : 8; // Reserved
  } bits;
} ADBMS_CFGA_t;

/*============================================================================
 * Configuration Register B (CFGB) - 6 bytes
 *
 * Bytes 0-2: VUV[11:0], VOV[11:0] (packed 12-bit thresholds)
 * Byte 3: DCTO[3:0], Reserved[3:0]
 * Bytes 4-5: DCC[15:0] (discharge cell control)
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    /* Byte 0: VUV[7:0] */
    uint8_t VUV_LO : 8;
    /* Byte 1: VUV[11:8], VOV[3:0] */
    uint8_t VUV_HI : 4;
    uint8_t VOV_LO : 4;
    /* Byte 2: VOV[11:4] */
    uint8_t VOV_HI : 8;
    /* Byte 3: DCTO, Reserved */
    uint8_t DCTO : 4; // Discharge timeout
    uint8_t _rsvd : 4;
    /* Byte 4: DCC[7:0] - cells 1-8 */
    uint8_t DCC_LO : 8;
    /* Byte 5: DCC[15:8] - cells 9-16 */
    uint8_t DCC_HI : 8;
  } bits;
} ADBMS_CFGB_t;

/*============================================================================
 * Status Register A (STATA) - 6 bytes
 *
 * Bytes 0-1: VREF2 (16-bit, 150µV/LSB)
 * Bytes 2-3: ITMP (16-bit, 7.5µV/LSB, offset for 27°C = 9315)
 * Bytes 4-5: VA (16-bit analog supply, 150µV/LSB)
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    uint16_t VREF2; // Reference voltage 2
    uint16_t ITMP;  // Internal die temperature
    uint16_t VA;    // Analog supply voltage
  } values;
} ADBMS_STATA_t;

/*============================================================================
 * Status Register B (STATB) - 6 bytes
 *
 * Bytes 0-1: VD (16-bit digital supply, 150µV/LSB)
 * Bytes 2-3: C_UV flags
 * Bytes 4-5: C_OV flags
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    uint16_t VD; // Digital supply voltage
    /* Bytes 2-3: Undervoltage flags */
    uint8_t C_UV_LO; // UV flags cells 1-8
    uint8_t C_UV_HI; // UV flags cells 9-16 (only [1:0] used for 10 cells)
    /* Bytes 4-5: Overvoltage flags */
    uint8_t C_OV_LO; // OV flags cells 1-8
    uint8_t C_OV_HI; // OV flags cells 9-16
  } bits;
} ADBMS_STATB_t;

/*============================================================================
 * Status Register C (STATC) - 6 bytes
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    uint16_t CS_FLT; // C/S fault flags
    uint16_t VA_OV;  // VA overvoltage
    uint16_t VA_UV;  // VA undervoltage
  } values;
} ADBMS_STATC_t;

/*============================================================================
 * Status Register D (STATD) - 6 bytes
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    uint16_t VD_OV; // VD overvoltage
    uint16_t VD_UV; // VD undervoltage
    uint8_t THSD;   // Thermal shutdown
    uint8_t SLEEP;  // Sleep status
  } values;
} ADBMS_STATD_t;

/*============================================================================
 * Status Register E (STATE) - 6 bytes
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    uint8_t GPI;    // GPIO input state
    uint8_t REV;    // Revision ID
    uint32_t _rsvd; // Reserved
  } values;
} ADBMS_STATE_t;

/*============================================================================
 * PWM Register (PWMA/PWMB) - 6 bytes
 *
 * Each nibble (4 bits) controls PWM duty for one cell's discharge FET.
 * 0x0 = 0% duty, 0xF = 100% duty
 * PWMA: Cells 1-12, PWMB: Cells 13-18 (if applicable)
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    uint8_t PWM1 : 4;
    uint8_t PWM2 : 4;
    uint8_t PWM3 : 4;
    uint8_t PWM4 : 4;
    uint8_t PWM5 : 4;
    uint8_t PWM6 : 4;
    uint8_t PWM7 : 4;
    uint8_t PWM8 : 4;
    uint8_t PWM9 : 4;
    uint8_t PWM10 : 4;
    uint8_t PWM11 : 4;
    uint8_t PWM12 : 4;
  } bits;
} ADBMS_PWM_t;

/*============================================================================
 * Cell Voltage Register Group (CVA-CVF) - 6 bytes
 *
 * Each group contains 3 cell voltage measurements (16-bit each).
 * Resolution: 150µV/LSB, Offset: 0V
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    uint16_t cell1; // First cell in group
    uint16_t cell2; // Second cell in group
    uint16_t cell3; // Third cell in group
  } values;
} ADBMS_CVReg_t;

/*============================================================================
 * Auxiliary Register Group (AUXA-AUXD) - 6 bytes
 *
 * Contains GPIO/auxiliary voltage measurements (16-bit each).
 * Resolution: 150µV/LSB
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    uint16_t aux1; // First aux in group
    uint16_t aux2; // Second aux in group
    uint16_t aux3; // Third aux in group
  } values;
} ADBMS_AUXReg_t;

/*============================================================================
 * COMM Register - 6 bytes
 *
 * For I2C/SPI master communication
 *============================================================================*/
typedef union
{
  uint8_t raw[6];
  struct __attribute__((packed))
  {
    uint8_t ICOM0 : 4;
    uint8_t FCOM0 : 4;
    uint8_t D0;
    uint8_t ICOM1 : 4;
    uint8_t FCOM1 : 4;
    uint8_t D1;
    uint8_t ICOM2 : 4;
    uint8_t FCOM2 : 4;
    uint8_t D2;
  } bits;
} ADBMS_COMM_t;

/*============================================================================
 * Conversion Functions
 *============================================================================*/

/**
 * @brief Convert raw 16-bit cell voltage code to voltage in mV
 * @param code Raw 16-bit ADC code
 * @return Voltage in millivolts
 */
static inline float ADBMS_CodeToVoltage_mV(uint16_t code)
{
  return (float)code * 0.150f; // 150µV/LSB
}

/**
 * @brief Convert raw 16-bit internal temp code to temperature in °C
 * @param code Raw ITMP code from STATA
 * @return Temperature in degrees Celsius
 */
static inline float ADBMS_CodeToTemp_C(uint16_t code)
{
  // ITMP = (T + 276°C) / 0.0075°C/LSB
  // T = ITMP * 0.0075 - 276
  return (float)code * 0.0075f - 276.0f;
}

/**
 * @brief Convert voltage threshold in mV to VUV/VOV code
 * @param voltage_mV Threshold voltage in millivolts
 * @return 12-bit threshold code
 */
static inline uint16_t ADBMS_VoltageToThreshold(float voltage_mV)
{
  // Threshold = (Code + 1) * 16 * 150µV = (Code + 1) * 2.4mV
  // Code = (Threshold / 2.4) - 1
  return (uint16_t)(voltage_mV / 2.4f) - 1;
}

/*============================================================================
 * API Function Declarations
 *============================================================================*/

/**
 * @brief Find command info by name (case-insensitive)
 * @param name Command name (e.g., "RDCFGA")
 * @return Pointer to command info, or NULL if not found
 */
const ADBMS_CmdInfo_t *ADBMS_FindCmdByName(const char *name);

/**
 * @brief Find command info by code
 * @param code 11-bit command code
 * @return Pointer to command info, or NULL if not found
 */
const ADBMS_CmdInfo_t *ADBMS_FindCmdByCode(uint16_t code);

/**
 * @brief Read a register from specific IC
 * @param cmd Read command code (e.g., RDCFGA)
 * @param ic IC index in daisy chain (0 = first)
 * @param data Output buffer (6 bytes)
 * @return 0 on success, 1 if PEC error, negative on failure
 */
int ADBMS_ReadReg(uint16_t cmd, uint8_t ic, uint8_t data[6]);

/**
 * @brief Write a register to specific IC
 * @param cmd Write command code (e.g., WRCFGA)
 * @param ic IC index in daisy chain
 * @param data Data to write (6 bytes)
 * @return 0 on success, negative on failure
 */
int ADBMS_WriteReg(uint16_t cmd, uint8_t ic, const uint8_t data[6]);

/**
 * @brief Send an action command (no data)
 * @param cmd Command code (e.g., ADCV, CLRCELL)
 * @return 0 on success, negative on failure
 */
int ADBMS_SendCmd(uint16_t cmd);

/**
 * @brief Poll ADC conversion status
 * @param cmd Poll command (PLADC, PLCADC, etc.)
 * @param timeout_us Timeout in microseconds
 * @return 0 if complete, 1 if still converting, negative on error
 */
int ADBMS_Poll(uint16_t cmd, uint32_t timeout_us);

/**
 * @brief Register console commands for register access
 * Call after console initialization
 */
void ADBMS_RegisterConsoleCommands(void);

/**
 * @brief Console subcommand handler for BMS|reg|*
 * @param argc Argument count (includes "reg")
 * @param argv Argument vector
 */
void ADBMS_RegSubcmd(int argc, char *argv[]);

#endif /* ADBMS6830B_REGISTERS_H */
