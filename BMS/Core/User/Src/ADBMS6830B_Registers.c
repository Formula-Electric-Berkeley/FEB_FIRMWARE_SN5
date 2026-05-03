/**
 * @file ADBMS6830B_Registers.c
 * @brief ADBMS6830B Register Access API and Console Commands
 *
 * Provides a clean interface for direct register access to the ADBMS6830B
 * battery monitor IC, with serial console integration.
 */

#include "ADBMS6830B_Registers.h"
#include "FEB_AD68xx_Interface.h"
#include "FEB_Const.h"
#include "feb_console.h"
#include "feb_string_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
    {0, 0, 0, NULL, NULL}};

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

/*============================================================================
 * Console: List all commands
 *============================================================================*/
static void subcmd_list(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== ADBMS6830B Commands ===\r\n");
  FEB_Console_Printf("%-10s %-6s %-4s %s\r\n", "Name", "Code", "Type", "Description");
  FEB_Console_Printf("---------- ------ ---- ---------------------\r\n");

  const char *type_str[] = {"WR", "RD", "ACT", "POLL"};

  for (const ADBMS_CmdInfo_t *cmd = cmd_table; cmd->name != NULL; cmd++)
  {
    FEB_Console_Printf("%-10s 0x%04X %-4s %s\r\n", cmd->name, cmd->code, type_str[cmd->type], cmd->desc);
  }
}

/*============================================================================
 * Console: Read register
 *============================================================================*/
static void subcmd_read(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: BMS|reg|read|<name> [ic]\r\n");
    FEB_Console_Printf("Example: BMS|reg|read|RDCFGA\r\n");
    return;
  }

  const ADBMS_CmdInfo_t *cmd = ADBMS_FindCmdByName(argv[1]);
  if (cmd == NULL)
  {
    FEB_Console_Printf("Unknown command: %s\r\n", argv[1]);
    return;
  }

  if (cmd->type != ADBMS_CMD_READ)
  {
    FEB_Console_Printf("%s is not a read command\r\n", cmd->name);
    return;
  }

  uint8_t ic_start = 0;
  uint8_t ic_end = FEB_NUM_IC;

  if (argc >= 3)
  {
    int ic = atoi(argv[2]);
    if (ic < 0 || ic >= FEB_NUM_IC)
    {
      FEB_Console_Printf("IC must be 0-%d\r\n", FEB_NUM_IC - 1);
      return;
    }
    ic_start = (uint8_t)ic;
    ic_end = ic_start + 1;
  }

  FEB_Console_Printf("Reading %s:\r\n", cmd->name);

  for (uint8_t ic = ic_start; ic < ic_end; ic++)
  {
    uint8_t data[6];
    int err = ADBMS_ReadReg(cmd->code, ic, data);

    FEB_Console_Printf("  IC%d: ", ic);
    if (err < 0)
    {
      FEB_Console_Printf("ERROR\r\n");
    }
    else
    {
      for (int i = 0; i < 6; i++)
      {
        FEB_Console_Printf("%02X ", data[i]);
      }
      FEB_Console_Printf("\r\n");
    }
  }
}

/*============================================================================
 * Console: Write register
 *============================================================================*/
static void subcmd_write(int argc, char *argv[])
{
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: BMS|reg|write|<name>|<hex_data>\r\n");
    FEB_Console_Printf("Example: BMS|reg|write|WRCFGA|010203040506\r\n");
    return;
  }

  const ADBMS_CmdInfo_t *cmd = ADBMS_FindCmdByName(argv[1]);
  if (cmd == NULL)
  {
    FEB_Console_Printf("Unknown command: %s\r\n", argv[1]);
    return;
  }

  if (cmd->type != ADBMS_CMD_WRITE)
  {
    FEB_Console_Printf("%s is not a write command\r\n", cmd->name);
    return;
  }

  /* Parse hex string */
  const char *hex = argv[2];
  size_t hex_len = strlen(hex);
  if (hex_len != 12)
  {
    FEB_Console_Printf("Data must be 12 hex chars (6 bytes)\r\n");
    return;
  }

  uint8_t data[6];
  for (int i = 0; i < 6; i++)
  {
    char byte_str[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
    data[i] = (uint8_t)strtoul(byte_str, NULL, 16);
  }

  FEB_Console_Printf("Writing %s: ", cmd->name);
  for (int i = 0; i < 6; i++)
  {
    FEB_Console_Printf("%02X ", data[i]);
  }
  FEB_Console_Printf("\r\n");

  int err = ADBMS_WriteReg(cmd->code, 0, data);
  if (err < 0)
  {
    FEB_Console_Printf("Write failed\r\n");
  }
  else
  {
    FEB_Console_Printf("OK\r\n");
  }
}

/*============================================================================
 * Console: Send command
 *============================================================================*/
static void subcmd_cmd(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: BMS|reg|cmd|<name>\r\n");
    FEB_Console_Printf("Example: BMS|reg|cmd|ADCV\r\n");
    return;
  }

  const ADBMS_CmdInfo_t *cmd = ADBMS_FindCmdByName(argv[1]);
  if (cmd == NULL)
  {
    FEB_Console_Printf("Unknown command: %s\r\n", argv[1]);
    return;
  }

  if (cmd->type != ADBMS_CMD_ACTION && cmd->type != ADBMS_CMD_POLL)
  {
    FEB_Console_Printf("%s is not an action command\r\n", cmd->name);
    return;
  }

  FEB_Console_Printf("Sending %s (0x%04X)...\r\n", cmd->name, cmd->code);
  int err = ADBMS_SendCmd(cmd->code);
  if (err < 0)
  {
    FEB_Console_Printf("Failed\r\n");
  }
  else
  {
    FEB_Console_Printf("OK\r\n");
  }
}

/*============================================================================
 * Console: Dump all readable registers
 *============================================================================*/
static void subcmd_dump(int argc, char *argv[])
{
  uint8_t ic = 0;
  if (argc >= 2)
  {
    ic = (uint8_t)atoi(argv[1]);
    if (ic >= FEB_NUM_IC)
    {
      FEB_Console_Printf("IC must be 0-%d\r\n", FEB_NUM_IC - 1);
      return;
    }
  }

  FEB_Console_Printf("\r\n=== Register Dump (IC %d) ===\r\n", ic);

  /* Dump key readable registers */
  static const uint16_t dump_cmds[] = {RDCFGA, RDCFGB, RDSTATA, RDSTATB, RDPWMA, RDAUXA, RDAUXB, RDAUXC,
                                       RDAUXD, RDCVA,  RDCVB,   RDCVC,   RDCVD,  RDCVE,  RDCVF,  RDSID};

  for (size_t i = 0; i < sizeof(dump_cmds) / sizeof(dump_cmds[0]); i++)
  {
    const ADBMS_CmdInfo_t *cmd = ADBMS_FindCmdByCode(dump_cmds[i]);
    if (cmd == NULL)
      continue;

    uint8_t data[6];
    ADBMS_ReadReg(cmd->code, ic, data);

    FEB_Console_Printf("%-10s: ", cmd->name);
    for (int j = 0; j < 6; j++)
    {
      FEB_Console_Printf("%02X ", data[j]);
    }
    FEB_Console_Printf("\r\n");
  }
}

/*============================================================================
 * Console: Show status summary
 *============================================================================*/
static void subcmd_status(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("\r\n=== ADBMS6830B Status ===\r\n");

  /* Read STATA */
  ADBMS_STATA_t stata = {0};
  ADBMS_ReadReg(RDSTATA, 0, stata.raw);

  float vref2 = ADBMS_CodeToVoltage_mV(stata.values.VREF2) / 1000.0f;
  float temp = ADBMS_CodeToTemp_C(stata.values.ITMP);
  float va = ADBMS_CodeToVoltage_mV(stata.values.VA) / 1000.0f;

  FEB_Console_Printf("VREF2:  %.3f V\r\n", vref2);
  FEB_Console_Printf("ITMP:   %.1f C\r\n", temp);
  FEB_Console_Printf("VA:     %.3f V\r\n", va);

  /* Read STATB */
  ADBMS_STATB_t statb = {0};
  ADBMS_ReadReg(RDSTATB, 0, statb.raw);

  float vd = ADBMS_CodeToVoltage_mV(statb.bits.VD) / 1000.0f;
  uint16_t uv_flags = statb.bits.C_UV_LO | ((uint16_t)statb.bits.C_UV_HI << 8);
  uint16_t ov_flags = statb.bits.C_OV_LO | ((uint16_t)statb.bits.C_OV_HI << 8);

  FEB_Console_Printf("VD:     %.3f V\r\n", vd);
  FEB_Console_Printf("UV:     0x%04X\r\n", uv_flags);
  FEB_Console_Printf("OV:     0x%04X\r\n", ov_flags);

  /* Read Serial ID */
  uint8_t sid[6];
  ADBMS_ReadReg(RDSID, 0, sid);
  FEB_Console_Printf("SID:    %02X%02X%02X%02X%02X%02X\r\n", sid[5], sid[4], sid[3], sid[2], sid[1], sid[0]);
}

/*============================================================================
 * Console: Main reg subcommand dispatcher
 *============================================================================*/
void ADBMS_RegSubcmd(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("ADBMS Register Access:\r\n");
    FEB_Console_Printf("  BMS|reg|list              - List all commands\r\n");
    FEB_Console_Printf("  BMS|reg|read|<name>       - Read register\r\n");
    FEB_Console_Printf("  BMS|reg|read|<name>|<ic>  - Read from specific IC\r\n");
    FEB_Console_Printf("  BMS|reg|write|<name>|<hex>- Write register\r\n");
    FEB_Console_Printf("  BMS|reg|cmd|<name>        - Send action command\r\n");
    FEB_Console_Printf("  BMS|reg|dump|[ic]         - Dump all registers\r\n");
    FEB_Console_Printf("  BMS|reg|status            - Show status summary\r\n");
    return;
  }

  const char *action = argv[1];

  if (FEB_strcasecmp(action, "list") == 0)
  {
    subcmd_list(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(action, "read") == 0)
  {
    subcmd_read(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(action, "write") == 0)
  {
    subcmd_write(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(action, "cmd") == 0)
  {
    subcmd_cmd(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(action, "dump") == 0)
  {
    subcmd_dump(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(action, "status") == 0)
  {
    subcmd_status(argc - 1, argv + 1);
  }
  else
  {
    FEB_Console_Printf("Unknown action: %s\r\n", action);
    FEB_Console_Printf("Use: list, read, write, cmd, dump, status\r\n");
  }
}

/*============================================================================
 * Registration (called from BMS_RegisterCommands)
 *============================================================================*/
void ADBMS_RegisterConsoleCommands(void)
{
  /* Registration is handled via FEB_Commands.c dispatch */
  /* This function exists for API completeness */
}
