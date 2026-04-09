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
#define RDSVB 0x0007  // Read S Voltage Register Group B (S4-S6)
#define RDSVC 0x000D  // Read S Voltage Register Group C (S7-S9)
#define RDSVD 0x000F  // Read S Voltage Register Group D (S10-S12)
#define RDSVE 0x000C  // Read S Voltage Register Group E (S13-S15)
#define RDSVF 0x000E  // Read S Voltage Register Group F (S16-S18)
#define RDSALL 0x0011 // Read All S Results

/*============================================================================
 * Combined Cell and S-Voltage Commands
 *============================================================================*/
#define RDCSALL 0x0051  // Read all C and S Results
#define RDACSALL 0x0052 // Read all Average C and S Results

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
 *============================================================================*/
#define RDSTATA 0x0030 // Read Status Register Group A
#define RDSTATB 0x0031 // Read Status Register Group B
#define RDSTATC 0x0032 // Read Status Register Group C
#define RDSTATD 0x0033 // Read Status Register Group D
#define RDSTATE 0x0034 // Read Status Register Group E
#define RDASALL 0x0035 // Read all AUX/Status Registers

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
#define CMHB 0x0011      // LPCM Heartbeat
#define WRCMCFG 0x00C4   // Write LPCM Configuration Register
#define RDCMCFG 0x00C5   // Read LPCM Configuration Register
#define WRCMCELLT 0x00C6 // Write LPCM Cell Threshold
#define RDCMCELLT 0x00C7 // Read LPCM Cell Threshold
#define WRCMGPIOT 0x00C8 // Write LPCM GPIO Threshold
#define RDCMGPIOT 0x00C9 // Read LPCM GPIO Threshold
#define CLRCMFLAG 0x00CA // Clear LPCM Flags
#define RDCMFLAG 0x00CB  // Read LPCM Flags

/*============================================================================
 * ADC Conversion Commands
 *
 * Note: ADCV, ADSV, ADAX, ADAX2 have configurable bits for mode selection.
 * The base values below are starting points - OR with flag bits as needed.
 *
 * ADCV bits: CONT[7], RD[4], DCP[4], RSTF[1], OW[1:0]
 * ADSV bits: CONT[7], DCP[4], OW[1:0]
 * ADAX bits: OW[3], PUP[2], CH[4:0]
 *============================================================================*/
#define ADCV 0x0260  // Start Cell Voltage ADC Conversion and Poll Status
#define ADSV 0x0168  // Start S-ADC Conversion and Poll Status
#define ADAX 0x0410  // Start AUX ADC Conversions and Poll Status
#define ADAX2 0x0400 // Start AUX2 ADC Conversions and Poll Status

/*============================================================================
 * Poll Commands
 *============================================================================*/
#define PLADC 0x0718  // Poll Any ADC Status
#define PLCADC 0x0719 // Poll C-ADC
#define PLSADC 0x071A // Poll S-ADC
#define PLAUX 0x071B  // Poll AUX ADC
#define PLAUX2 0x071C // Poll AUX2 ADC

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
#define ULRR 0x000A // Unlock Retention Register
#define WRRR 0x000B // Write Retention Registers
#define RDRR 0x000C // Read Retention Registers

/*============================================================================
 * ADC Mode Bit Flags (for building ADCV/ADSV/ADAX commands)
 *============================================================================*/

/* ADCV Command Flags */
#define ADCV_CONT 0x0080   // Continuous mode
#define ADCV_RD 0x0010     // Redundancy bit
#define ADCV_DCP 0x0010    // Discharge permitted
#define ADCV_RSTF 0x0004   // Reset filter
#define ADCV_OW_OFF 0x0000 // Open-wire off
#define ADCV_OW_PUP 0x0001 // Open-wire pull-up
#define ADCV_OW_PDN 0x0002 // Open-wire pull-down

/* ADAX Command Flags */
#define ADAX_OW 0x0008        // Open-wire mode
#define ADAX_PUP 0x0004       // Pull-up current
#define ADAX_CH_ALL 0x0000    // All channels
#define ADAX_CH_GPIO1 0x0001  // GPIO1 only
#define ADAX_CH_GPIO2 0x0002  // GPIO2 only
#define ADAX_CH_GPIO3 0x0003  // GPIO3 only
#define ADAX_CH_GPIO4 0x0004  // GPIO4 only
#define ADAX_CH_GPIO5 0x0005  // GPIO5 only
#define ADAX_CH_GPIO6 0x0006  // GPIO6 only
#define ADAX_CH_GPIO7 0x0007  // GPIO7 only
#define ADAX_CH_GPIO8 0x0008  // GPIO8 only
#define ADAX_CH_GPIO9 0x0009  // GPIO9 only
#define ADAX_CH_GPIO10 0x000A // GPIO10 only
#define ADAX_CH_VREF2 0x000B  // VREF2 only
#define ADAX_CH_ITEMP 0x000C  // Internal temp only

#endif /* ADBMS6830B_COMMANDS_H */
