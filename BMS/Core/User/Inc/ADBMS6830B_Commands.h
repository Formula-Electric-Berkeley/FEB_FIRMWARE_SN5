/**
 * @file ADBMS6830B_Commands.h
 * @brief ADBMS6830B Command Codes - Complete Table 50 from Datasheet
 *
 * All command names match the datasheet exactly for easy cross-reference.
 * Command codes are 11-bit values (CC[10:0]) as specified in Table 50.
 */

#ifndef ADBMS6830B_COMMANDS_H
#define ADBMS6830B_COMMANDS_H

/*============================================================================
 * Configuration Register Commands
 *============================================================================*/
#define WRCFGA 0x0001 // Write Configuration Register Group A
#define WRCFGB 0x0024 // Write Configuration Register Group B
#define RDCFGA 0x0002 // Read Configuration Register Group A
#define RDCFGB 0x0026 // Read Configuration Register Group B

/*============================================================================
 * Cell Voltage Register Commands (C-ADC Results)
 *============================================================================*/
#define RDCVA 0x0004   // Read Cell Voltage Register Group A (C1-C3)
#define RDCVB 0x0006   // Read Cell Voltage Register Group B (C4-C6)
#define RDCVC 0x0008   // Read Cell Voltage Register Group C (C7-C9)
#define RDCVD 0x000A   // Read Cell Voltage Register Group D (C10-C12)
#define RDCVE 0x0009   // Read Cell Voltage Register Group E (C13-C15)
#define RDCVF 0x000B   // Read Cell Voltage Register Group F (C16-C18)
#define RDCVALL 0x000C // Read All Cell Results

/*============================================================================
 * Averaged Cell Voltage Register Commands (A-ADC Results)
 *============================================================================*/
#define RDACA 0x0044   // Read Averaged Cell Voltage Register Group A
#define RDACB 0x0046   // Read Averaged Cell Voltage Register Group B
#define RDACC 0x0048   // Read Averaged Cell Voltage Register Group C
#define RDACD 0x004A   // Read Averaged Cell Voltage Register Group D
#define RDACE 0x0049   // Read Averaged Cell Voltage Register Group E
#define RDACF 0x004B   // Read Averaged Cell Voltage Register Group F
#define RDACALL 0x004C // Read All Avg Cell Results

/*============================================================================
 * S-Voltage Register Commands (Redundant ADC Results)
 *============================================================================*/
#define RDSVA 0x0003  // Read S Voltage Register Group A (S1-S3)
#define RDSVB 0x0005  // Read S Voltage Register Group B (S4-S6)
#define RDSVC 0x0007  // Read S Voltage Register Group C (S7-S9)
#define RDSVD 0x000D  // Read S Voltage Register Group D (S10-S12)
#define RDSVE 0x000E  // Read S Voltage Register Group E (S13-S15)
#define RDSVF 0x000F  // Read S Voltage Register Group F (S16-S18)
#define RDSALL 0x0010 // Read All S Results

/*============================================================================
 * Combined Cell and S-Voltage Commands
 *============================================================================*/
#define RDCSALL 0x0011  // Read all C and S Results
#define RDACSALL 0x0051 // Read all Average C and S Results

/*============================================================================
 * Filtered Cell Voltage Register Commands (F-ADC Results)
 *============================================================================*/
#define RDFCA 0x0012   // Read Filter Cell Voltage Register Group A
#define RDFCB 0x0013   // Read Filter Cell Voltage Register Group B
#define RDFCC 0x0014   // Read Filter Cell Voltage Register Group C
#define RDFCD 0x0015   // Read Filter Cell Voltage Register Group D
#define RDFCE 0x0016   // Read Filter Cell Voltage Register Group E
#define RDFCF 0x0017   // Read Filter Cell Voltage Register Group F
#define RDFCALL 0x0018 // Read All Filter Cell Results

/*============================================================================
 * Auxiliary Register Commands (GPIO/Temperature)
 *============================================================================*/
#define RDAUXA 0x0019 // Read Auxiliary Register Group A
#define RDAUXB 0x001A // Read Auxiliary Register Group B
#define RDAUXC 0x001B // Read Auxiliary Register Group C
#define RDAUXD 0x001F // Read Auxiliary Register Group D

/*============================================================================
 * Redundant Auxiliary Register Commands
 *============================================================================*/
#define RDRAXA 0x001C // Read Redundant Auxiliary Register Group A
#define RDRAXB 0x001D // Read Redundant Auxiliary Register Group B
#define RDRAXC 0x001E // Read Auxiliary Redundant Register Group C
#define RDRAXD 0x0025 // Read Auxiliary Redundant Register Group D

/*============================================================================
 * Status Register Commands
 * err: 0: Reading Status Register C without error injection
 *      1: Reading Status Register C with error injection for latent fault detection, Bit SPIFLT must be set
 *============================================================================*/
#define RDSTATA 0x0030                              // Read Status Register Group A
#define RDSTATB 0x0031                              // Read Status Register Group B
#define RDSTATC(err) (0x0032 | ((err & 0x01) << 6)) // Read Status Register Group C
#define RDSTATD 0x0033                              // Read Status Register Group D
#define RDSTATE 0x0034                              // Read Status Register Group E
#define RDASALL 0x0035                              // Read all AUX/Status Registers

/*============================================================================
 * PWM Register Commands
 *============================================================================*/
#define WRPWMA 0x0020 // Write PWM Register Group A
#define RDPWMA 0x0022 // Read PWM Register Group A
#define WRPWMB 0x0021 // Write PWM Register Group B
#define RDPWMB 0x0023 // Read PWM Register Group B

/*============================================================================
 * LPCM (Low Power Cell Monitor) Commands
 *============================================================================*/
#define CMDIS 0x0040     // LPCM Disable
#define CMEN 0x0041      // LPCM Enable
#define CMHB 0x0043      // LPCM Heartbeat
#define WRCMCFG 0x0058   // Write LPCM Configuration Register
#define RDCMCFG 0x0059   // Read LPCM Configuration Register
#define WRCMCELLT 0x005A // Write LPCM Cell Threshold
#define RDCMCELLT 0x005B // Read LPCM Cell Threshold
#define WRCMGPIOT 0x005C // Write LPCM GPIO Threshold
#define RDCMGPIOT 0x005D // Read LPCM GPIO Threshold
#define CLRCMFLAG 0x005E // Clear LPCM Flags
#define RDCMFLAG 0x005F  // Read LPCM Flags

/*============================================================================
 * ADC Conversion Commands
 *
 * Note: ADCV, ADSV, ADAX, ADAX2 have configurable bits for mode selection.
 * The base values below are starting points - OR with flag bits as needed.
 *
 * ADCV bits:
 * ADSV bits:
 * ADAX bits:
 *============================================================================*/
// Start Cell Voltage ADC Conversion and Poll Status
#define ADCV(rd, cont, dcp, rstf, ow)                                                                                  \
  (0x0260 | ((rd & 0x01) << 8) | ((cont & 0x01) << 7) | ((dcp & 0x01) << 4) | ((rstf & 0x01) << 2) | ((ow & 0x03) << 0))

// Start S-ADC Conversion and Poll Status
#define ADSV(cont, dcp, ow) (0x0168 | ((cont & 0x01) << 7) | ((dcp & 0x01) << 4) | ((ow & 0x03) << 0))

// Start AUX ADC Conversions and Poll Status
#define ADAX(ow, pup, ch) (0x0410 | ((ow & 0x01) << 8) | ((pup & 0x01) << 7) | ((ch & 0x10) << 6) | ((ch & 0x0F) << 0))

// Start AUX2 ADC Conversions and Poll Status
#define ADAX2(ch) (0x0400 | ((ch & 0x0F) << 0))

/*============================================================================
 * Clear Register Commands
 *============================================================================*/
#define CLRCELL 0x0711 // Clear Cell Voltage Register Groups
#define CLRFC 0x0714   // Clear Filtered Cell Voltage Register Groups
#define CLRAUX 0x0712  // Clear Auxiliary Register Groups
#define CLRSPIN 0x0716 // Clear S-Voltage Register Groups
#define CLRFLAG 0x0717 // Clear Flags
#define CLOVUV 0x0715  // Clear OVUV

/*============================================================================
 * Poll Commands
 *============================================================================*/
#define PLADC 0x0718  // Poll Any ADC Status
#define PLCADC 0x071C // Poll C-ADC
#define PLSADC 0x071D // Poll S-ADC
#define PLAUX 0x071E  // Poll AUX ADC
#define PLAUX2 0x071F // Poll AUX2 ADC

/*============================================================================
 * Communication Commands (I2C/SPI Master)
 *============================================================================*/
#define WRCOMM 0x0721 // Write COMM Register Group
#define RDCOMM 0x0722 // Read COMM Register Group
#define STCOMM 0x0723 // Start I2C/SPI Communication

/*============================================================================
 * Control Commands
 *============================================================================*/
#define MUTE 0x0028   // Mute Discharge
#define UNMUTE 0x0029 // Unmute Discharge
#define SNAP 0x002D   // Snapshot
#define UNSNAP 0x002F // Release Snapshot
#define SRST 0x0027   // Soft Reset

/*============================================================================
 * Serial ID and Command Counter
 *============================================================================*/
#define RDSID 0x002C // Read Serial ID Register Group
#define RSTCC 0x002E // Reset Command Counter

/*============================================================================
 * Retention Register Commands
 *============================================================================*/
#define ULRR 0x0038 // Unlock Retention Register
#define WRRR 0x0039 // Write Retention Registers
#define RDRR 0x003A // Read Retention Registers

/*============================================================================
 * ADC Mode Bit Flags (for building ADCV/ADSV/ADAX commands) — Table 51 & 52
 *============================================================================*/

/**
 * @brief ADC Channel Selection for ADAX/ADAX2 commands.
 *
 * Channel definitions:
 *   0   : All channels
 *   1   : GPIO1 only
 *   2   : GPIO2 only
 *   3   : GPIO3 only
 *   4   : GPIO4 only
 *   5   : GPIO5 only
 *   6   : GPIO6 only
 *   7   : GPIO7 only
 *   8   : GPIO8 only
 *   9   : GPIO9 only
 *   10  : GPIO10 only
 *   11  : VREF2 only
 *   12  : VD only
 *   13  : VA only
 *   14  : ITEMP only
 *   15  : VPV only
 *   16  : VMV only
 *   17  : RES only
 */
typedef enum
{
  ADC_CONV_CH_ALL = 0x0000,
  ADC_CONV_CH_GPIO1 = 0x0001,
  ADC_CONV_CH_GPIO2 = 0x0002,
  ADC_CONV_CH_GPIO3 = 0x0003,
  ADC_CONV_CH_GPIO4 = 0x0004,
  ADC_CONV_CH_GPIO5 = 0x0005,
  ADC_CONV_CH_GPIO6 = 0x0006,
  ADC_CONV_CH_GPIO7 = 0x0007,
  ADC_CONV_CH_GPIO8 = 0x0008,
  ADC_CONV_CH_GPIO9 = 0x0009,
  ADC_CONV_CH_GPIO10 = 0x000A,
  ADC_CONV_CH_VREF2 = 0x0010,
  ADC_CONV_CH_VD = 0x0011,
  ADC_CONV_CH_VA = 0x0012,
  ADC_CONV_CH_ITEMP = 0x0013,
  ADC_CONV_CH_VPV = 0x0014,
  ADC_CONV_CH_VMV = 0x0015,
  ADC_CONV_CH_RES = 0x0016
} adc_conv_channel_selection_t;

/**
 * @brief ADC Conversion Mode
 *
 * - ADC_CONV_SINGLE_SHOT: Single shot conversion (0)
 * - ADC_CONV_CONTINUOUS : Continuous conversion (1)
 */
typedef enum
{
  ADC_CONV_SINGLE_SHOT = 0x0000,
  ADC_CONV_CONTINUOUS = 0x0001,
} adc_conv_mode_t;

/**
 * @brief Open-Wire Detection Modes for ADC Conversion
 *
 * OW[1:0] settings:
 *   0b00: Open-wire detection off
 *   0b01: Open-wire detection on even channels
 *   0b10: Open-wire detection on odd channels
 *   0b11: Open-wire detection on all channels
 * OW (for single-bit fields):
 *   0: Open-wire detection off
 *   1: Open-wire detection on
 */
typedef enum
{
  ADC_CONV_OW_OFF = 0x0000,     ///< Open-wire detection off (single-bit)
  ADC_CONV_OW_ON = 0x0001,      ///< Open-wire detection on (single-bit)
  ADC_CONV_OW_OFF_CH = 0x0000,  ///< 2-bit: detection off
  ADC_CONV_OW_EVEN_CH = 0x0001, ///< 2-bit: detection on even channels
  ADC_CONV_OW_ODD_CH = 0x0002,  ///< 2-bit: detection on odd channels
  ADC_CONV_OW_ALL_CH = 0x0003,  ///< 2-bit: detection on all channels
} adc_conv_open_wire_detection_mode_t;

/**
 * @brief ADC Pull Up / Pull Down Selection
 *
 * - ADC_CONV_PUP_DOWN: Pull-down selected (0)
 * - ADC_CONV_PUP_UP: Pull-up selected (1)
 */
typedef enum
{
  ADC_CONV_PUP_DOWN = 0x0000, ///< Pull-down selected
  ADC_CONV_PUP_UP = 0x0001,   ///< Pull-up selected
} adc_conv_pull_up_pull_down_t;

/**
 * @brief Discharge Permitted State for ADC Conversion
 *
 * - ADC_CONV_DCP_NOT_PERMITTED: Discharge not permitted (0)
 * - ADC_CONV_DCP_PERMITTED: Discharge permitted (1)
 */
typedef enum
{
  ADC_CONV_DCP_NOT_PERMITTED = 0x0000, ///< Discharge not permitted
  ADC_CONV_DCP_PERMITTED = 0x0001,     ///< Discharge permitted
} adc_conv_discharge_permitted_t;

/**
 * @brief ADC Reset Filter Control
 *
 * - ADC_CONV_RSTF_DO_NOT_RESET: Do not reset filter (0)
 * - ADC_CONV_RSTF_RESET: Reset digital filter (1)
 */
typedef enum
{
  ADC_CONV_RSTF_DO_NOT_RESET = 0x0000, ///< Do not reset filter
  ADC_CONV_RSTF_RESET = 0x0001,        ///< Reset digital filter
} adc_conv_reset_filter_t;

/**
 * @brief ADC Error Injection Enable/Disable
 *
 * - ADC_CONV_ERR_INJECTION_OFF: Error injection off (0)
 * - ADC_CONV_ERR_INJECTION_ON: Error injection on (1) - To test for latent fault detection - Bit SPIFLT must be set
 */
typedef enum
{
  ADC_CONV_ERR_INJECTION_OFF = 0x0000, ///< Error injection disabled
  ADC_CONV_ERR_INJECTION_ON = 0x0001,  ///< Error injection enabled
} adc_conv_error_injection_t;

#endif /* ADBMS6830B_COMMANDS_H */
