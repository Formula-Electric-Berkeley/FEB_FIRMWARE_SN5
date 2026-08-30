/**
 * @file ADBMS6830B_Registers.c
 * @brief ADBMS6830B Register Access API
 *
 * Provides a clean interface for direct register access to the ADBMS6830B
 * battery monitor IC.
 */

#include "ADBMS6830B_Registers.h"
#include "FEB_AD68xx_Interface.h"
#include "FEB_Const.h"
#include "feb_string_utils.h"
#include <string.h>

/*============================================================================
 * Complete Command Table - All ADBMS6830B Commands from Datasheet Table 50
 *============================================================================*/
static const ADBMS_CmdInfo_t cmd_table[] = {
    /* Configuration Registers */
    {WRCFGA, ADBMS_CMD_WRITE, 6, "WRCFGA", "Write Configuration A"},
    {RDCFGA, ADBMS_CMD_READ, 6, "RDCFGA", "Read Configuration A"},
    {WRCFGB, ADBMS_CMD_WRITE, 6, "WRCFGB", "Write Configuration B"},
    {RDCFGB, ADBMS_CMD_READ, 6, "RDCFGB", "Read Configuration B"},

    /* Cell Voltage Registers (C-ADC) */
    {RDCVA, ADBMS_CMD_READ, 6, "RDCVA", "Read Cell Voltage A (C1-3)"},
    {RDCVB, ADBMS_CMD_READ, 6, "RDCVB", "Read Cell Voltage B (C4-6)"},
    {RDCVC, ADBMS_CMD_READ, 6, "RDCVC", "Read Cell Voltage C (C7-9)"},
    {RDCVD, ADBMS_CMD_READ, 6, "RDCVD", "Read Cell Voltage D (C10-12)"},
    {RDCVE, ADBMS_CMD_READ, 6, "RDCVE", "Read Cell Voltage E (C13-15)"},
    {RDCVF, ADBMS_CMD_READ, 6, "RDCVF", "Read Cell Voltage F (C16-18)"},
    {RDCVALL, ADBMS_CMD_READ, 36, "RDCVALL", "Read All Cell Voltages"},

    /* Averaged Cell Voltage Registers */
    {RDACA, ADBMS_CMD_READ, 6, "RDACA", "Read Averaged Cell A"},
    {RDACB, ADBMS_CMD_READ, 6, "RDACB", "Read Averaged Cell B"},
    {RDACC, ADBMS_CMD_READ, 6, "RDACC", "Read Averaged Cell C"},
    {RDACD, ADBMS_CMD_READ, 6, "RDACD", "Read Averaged Cell D"},
    {RDACE, ADBMS_CMD_READ, 6, "RDACE", "Read Averaged Cell E"},
    {RDACF, ADBMS_CMD_READ, 6, "RDACF", "Read Averaged Cell F"},
    {RDACALL, ADBMS_CMD_READ, 36, "RDACALL", "Read All Averaged Cells"},

    /* S-Voltage Registers */
    {RDSVA, ADBMS_CMD_READ, 6, "RDSVA", "Read S-Voltage A (S1-3)"},
    {RDSVB, ADBMS_CMD_READ, 6, "RDSVB", "Read S-Voltage B (S4-6)"},
    {RDSVC, ADBMS_CMD_READ, 6, "RDSVC", "Read S-Voltage C (S7-9)"},
    {RDSVD, ADBMS_CMD_READ, 6, "RDSVD", "Read S-Voltage D (S10-12)"},
    {RDSVE, ADBMS_CMD_READ, 6, "RDSVE", "Read S-Voltage E (S13-15)"},
    {RDSVF, ADBMS_CMD_READ, 6, "RDSVF", "Read S-Voltage F (S16-18)"},
    {RDSALL, ADBMS_CMD_READ, 36, "RDSALL", "Read All S-Voltages"},

    /* Filtered Cell Voltage Registers */
    {RDFCA, ADBMS_CMD_READ, 6, "RDFCA", "Read Filtered Cell A"},
    {RDFCB, ADBMS_CMD_READ, 6, "RDFCB", "Read Filtered Cell B"},
    {RDFCC, ADBMS_CMD_READ, 6, "RDFCC", "Read Filtered Cell C"},
    {RDFCD, ADBMS_CMD_READ, 6, "RDFCD", "Read Filtered Cell D"},
    {RDFCE, ADBMS_CMD_READ, 6, "RDFCE", "Read Filtered Cell E"},
    {RDFCF, ADBMS_CMD_READ, 6, "RDFCF", "Read Filtered Cell F"},
    {RDFCALL, ADBMS_CMD_READ, 36, "RDFCALL", "Read All Filtered Cells"},

    /* Combined Reads */
    {RDCSALL, ADBMS_CMD_READ, 72, "RDCSALL", "Read All C and S"},
    {RDACSALL, ADBMS_CMD_READ, 72, "RDACSALL", "Read All Averaged C and S"},

    /* Auxiliary Registers */
    {RDAUXA, ADBMS_CMD_READ, 6, "RDAUXA", "Read Auxiliary A (GPIO1-3)"},
    {RDAUXB, ADBMS_CMD_READ, 6, "RDAUXB", "Read Auxiliary B (GPIO4-6)"},
    {RDAUXC, ADBMS_CMD_READ, 6, "RDAUXC", "Read Auxiliary C (GPIO7-9)"},
    {RDAUXD, ADBMS_CMD_READ, 6, "RDAUXD", "Read Auxiliary D (GPIO10)"},

    /* Redundant Auxiliary Registers */
    {RDRAXA, ADBMS_CMD_READ, 6, "RDRAXA", "Read Redundant Aux A"},
    {RDRAXB, ADBMS_CMD_READ, 6, "RDRAXB", "Read Redundant Aux B"},
    {RDRAXC, ADBMS_CMD_READ, 6, "RDRAXC", "Read Redundant Aux C"},
    {RDRAXD, ADBMS_CMD_READ, 6, "RDRAXD", "Read Redundant Aux D"},

    /* Status Registers */
    {RDSTATA, ADBMS_CMD_READ, 6, "RDSTATA", "Read Status A (VREF2/ITMP/VA)"},
    {RDSTATB, ADBMS_CMD_READ, 6, "RDSTATB", "Read Status B (VD/UV/OV)"},
    {RDSTATC, ADBMS_CMD_READ, 6, "RDSTATC", "Read Status C"},
    {RDSTATD, ADBMS_CMD_READ, 6, "RDSTATD", "Read Status D"},
    {RDSTATE, ADBMS_CMD_READ, 6, "RDSTATE", "Read Status E"},
    {RDASALL, ADBMS_CMD_READ, 30, "RDASALL", "Read All Status"},

    /* PWM Registers */
    {WRPWMA, ADBMS_CMD_WRITE, 6, "WRPWMA", "Write PWM A"},
    {RDPWMA, ADBMS_CMD_READ, 6, "RDPWMA", "Read PWM A"},
    {WRPWMB, ADBMS_CMD_WRITE, 6, "WRPWMB", "Write PWM B"},
    {RDPWMB, ADBMS_CMD_READ, 6, "RDPWMB", "Read PWM B"},

    /* LPCM Commands */
    {CMDIS, ADBMS_CMD_ACTION, 0, "CMDIS", "LPCM Disable"},
    {CMEN, ADBMS_CMD_ACTION, 0, "CMEN", "LPCM Enable"},
    {CMHB, ADBMS_CMD_ACTION, 0, "CMHB", "LPCM Heartbeat"},
    {WRCMCFG, ADBMS_CMD_WRITE, 6, "WRCMCFG", "Write LPCM Config"},
    {RDCMCFG, ADBMS_CMD_READ, 6, "RDCMCFG", "Read LPCM Config"},
    {WRCMCELLT, ADBMS_CMD_WRITE, 6, "WRCMCELLT", "Write LPCM Cell Thresh"},
    {RDCMCELLT, ADBMS_CMD_READ, 6, "RDCMCELLT", "Read LPCM Cell Thresh"},
    {WRCMGPIOT, ADBMS_CMD_WRITE, 6, "WRCMGPIOT", "Write LPCM GPIO Thresh"},
    {RDCMGPIOT, ADBMS_CMD_READ, 6, "RDCMGPIOT", "Read LPCM GPIO Thresh"},
    {CLRCMFLAG, ADBMS_CMD_ACTION, 0, "CLRCMFLAG", "Clear LPCM Flags"},
    {RDCMFLAG, ADBMS_CMD_READ, 6, "RDCMFLAG", "Read LPCM Flags"},

    /* ADC Conversion Commands */
    {ADCV, ADBMS_CMD_ACTION, 0, "ADCV", "Start Cell Voltage ADC"},
    {ADSV, ADBMS_CMD_ACTION, 0, "ADSV", "Start S-Voltage ADC"},
    {ADAX, ADBMS_CMD_ACTION, 0, "ADAX", "Start Auxiliary ADC"},
    {ADAX2, ADBMS_CMD_ACTION, 0, "ADAX2", "Start AUX2 ADC"},

    /* Poll Commands */
    {PLADC, ADBMS_CMD_POLL, 1, "PLADC", "Poll Any ADC"},
    {PLCADC, ADBMS_CMD_POLL, 1, "PLCADC", "Poll C-ADC"},
    {PLSADC, ADBMS_CMD_POLL, 1, "PLSADC", "Poll S-ADC"},
    {PLAUX, ADBMS_CMD_POLL, 1, "PLAUX", "Poll AUX ADC"},
    {PLAUX2, ADBMS_CMD_POLL, 1, "PLAUX2", "Poll AUX2 ADC"},

    /* Clear Commands */
    {CLRCELL, ADBMS_CMD_ACTION, 0, "CLRCELL", "Clear Cell Registers"},
    {CLRFC, ADBMS_CMD_ACTION, 0, "CLRFC", "Clear Filtered Cells"},
    {CLRAUX, ADBMS_CMD_ACTION, 0, "CLRAUX", "Clear Auxiliary Regs"},
    {CLRSPIN, ADBMS_CMD_ACTION, 0, "CLRSPIN", "Clear S-Voltage Regs"},
    {CLRFLAG, ADBMS_CMD_ACTION, 0, "CLRFLAG", "Clear All Flags"},
    {CLOVUV, ADBMS_CMD_ACTION, 0, "CLOVUV", "Clear OV/UV Flags"},

    /* Control Commands */
    {MUTE, ADBMS_CMD_ACTION, 0, "MUTE", "Mute Discharge"},
    {UNMUTE, ADBMS_CMD_ACTION, 0, "UNMUTE", "Unmute Discharge"},
    {SNAP, ADBMS_CMD_ACTION, 0, "SNAP", "Snapshot Voltages"},
    {UNSNAP, ADBMS_CMD_ACTION, 0, "UNSNAP", "Release Snapshot"},
    {SRST, ADBMS_CMD_ACTION, 0, "SRST", "Soft Reset"},

    /* Communication Commands */
    {WRCOMM, ADBMS_CMD_WRITE, 6, "WRCOMM", "Write COMM Register"},
    {RDCOMM, ADBMS_CMD_READ, 6, "RDCOMM", "Read COMM Register"},
    {STCOMM, ADBMS_CMD_ACTION, 0, "STCOMM", "Start I2C/SPI Comm"},

    /* ID and Counter Commands */
    {RDSID, ADBMS_CMD_READ, 6, "RDSID", "Read Serial ID"},
    {RSTCC, ADBMS_CMD_ACTION, 0, "RSTCC", "Reset Command Counter"},

    /* Retention Register Commands */
    {ULRR, ADBMS_CMD_ACTION, 0, "ULRR", "Unlock Retention Reg"},
    {WRRR, ADBMS_CMD_WRITE, 6, "WRRR", "Write Retention Reg"},
    {RDRR, ADBMS_CMD_READ, 6, "RDRR", "Read Retention Reg"},

    /* Sentinel */
    {0, (ADBMS_CmdType_t)0, 0, NULL, NULL}};

/*============================================================================
 * API: Find Command by Name
 *============================================================================*/
const ADBMS_CmdInfo_t *ADBMS_FindCmdByName(const char *name)
{
  if (name == NULL)
    return NULL;

  for (const ADBMS_CmdInfo_t *cmd = cmd_table; cmd->name != NULL; cmd++)
  {
    if (FEB_strcasecmp(name, cmd->name) == 0)
    {
      return cmd;
    }
  }
  return NULL;
}

/*============================================================================
 * API: Find Command by Code
 *============================================================================*/
const ADBMS_CmdInfo_t *ADBMS_FindCmdByCode(uint16_t code)
{
  for (const ADBMS_CmdInfo_t *cmd = cmd_table; cmd->name != NULL; cmd++)
  {
    if (cmd->code == code)
    {
      return cmd;
    }
  }
  return NULL;
}

/*============================================================================
 * API: Read Register
 *============================================================================*/
int ADBMS_ReadReg(uint16_t cmd, uint8_t ic, uint8_t data[6])
{
  (void)ic; /* For single IC, ignore index */

  /* Buffer for all ICs (8 bytes each: 6 data + 2 PEC) */
  uint8_t rx_buf[FEB_NUM_IC * 8];

  transmitCMDR(cmd, rx_buf, FEB_NUM_IC * 8);

  /* Copy data for requested IC (IC 0 is last in buffer due to daisy chain) */
  uint8_t offset = (FEB_NUM_IC - 1 - ic) * 8;
  memcpy(data, &rx_buf[offset], 6);

  /* TODO: Validate PEC */
  return 0;
}

/*============================================================================
 * API: Write Register
 *============================================================================*/
int ADBMS_WriteReg(uint16_t cmd, uint8_t ic, const uint8_t data[6])
{
  (void)ic; /* For single IC, ignore index */

  /* For daisy chain, data is arranged last IC first */
  uint8_t tx_buf[FEB_NUM_IC * 6];

  /* Copy data to correct position for IC */
  uint8_t offset = (FEB_NUM_IC - 1 - ic) * 6;
  memcpy(&tx_buf[offset], data, 6);

  transmitCMDW(cmd, tx_buf);
  return 0;
}

/*============================================================================
 * API: Send Action Command
 *============================================================================*/
int ADBMS_SendCmd(uint16_t cmd)
{
  transmitCMD(cmd);
  return 0;
}

/*============================================================================
 * API: Poll ADC Status
 *============================================================================*/
int ADBMS_Poll(uint16_t cmd, uint32_t timeout_us)
{
  uint8_t rx_buf[4];
  uint32_t count = 0;
  uint32_t max_count = timeout_us / 10;

  while (count < max_count)
  {
    transmitCMDR(cmd, rx_buf, 4);
    /* ADC complete when all bits are 1 */
    if (rx_buf[0] == 0xFF)
    {
      return 0; /* Complete */
    }
    /* Small delay */
    for (volatile int i = 0; i < 100; i++)
      ;
    count++;
  }
  return 1; /* Timeout */
}

const ADBMS_CmdInfo_t *ADBMS_CmdTable(void)
{
  return cmd_table;
}
