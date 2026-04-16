/**
 ******************************************************************************
 * @file           : FEB_Commands.c
 * @brief          : BMS-specific console commands
 * @author         : Formula Electric @ Berkeley
 *
 * Commands follow hierarchical pattern: BMS|subcommand|args
 ******************************************************************************
 */

#include "FEB_Commands.h"
#include "feb_console.h"
#include "feb_log.h"
#include "feb_string_utils.h"
#include "FEB_ADBMS_App.h"
#include "FEB_BMS_Acquisition.h"
#include "FEB_BMS_Processing.h"
#include "BMS_HW_Config.h"
#include "ADBMS6830B_Registers.h"
#include "FEB_CAN_IVT.h"
#include "FEB_CAN_PingPong.h"
#include "FEB_CAN_State.h"
#include "FEB_Const.h"
#include "FEB_HW_Relay.h"
#include "FEB_SM.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Logging tag */
#define TAG_BMS "[BMS]"

/* External task handles for stack monitoring */
extern osThreadId_t uartRxTaskHandle;
extern osThreadId_t ADBMSTaskHandle;
extern osThreadId_t TPSTaskHandle;
extern osThreadId_t BMSTaskRxHandle;
extern osThreadId_t BMSTaskTxHandle;

/* ============================================================================
 * BMS Status Command
 * ============================================================================ */

static void cmd_bms_status(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  const BMS_PackData_t *pack = BMS_App_GetPackData();

  FEB_Console_Printf("=== BMS Status ===\r\n");
  FEB_Console_Printf("Initialized: %s\r\n", pack->initialized ? "YES" : "NO");
  FEB_Console_Printf("Mode: %s\r\n", pack->mode == BMS_MODE_NORMAL     ? "NORMAL"
                                     : pack->mode == BMS_MODE_CHARGING ? "CHARGING"
                                                                       : "BALANCING");

  if (!pack->initialized)
  {
    FEB_Console_Printf("System not initialized - no data available\r\n");
    return;
  }

  FEB_Console_Printf("\r\n--- Pack Voltages ---\r\n");
  FEB_Console_Printf("Total: %.2f V\r\n", (double)pack->pack_voltage_V);
  FEB_Console_Printf("Min Cell: %.3f V\r\n", (double)pack->pack_min_cell_V);
  FEB_Console_Printf("Max Cell: %.3f V\r\n", (double)pack->pack_max_cell_V);
  FEB_Console_Printf("Delta: %.1f mV\r\n", (double)((pack->pack_max_cell_V - pack->pack_min_cell_V) * 1000.0f));

  FEB_Console_Printf("\r\n--- Temperatures ---\r\n");
  FEB_Console_Printf("Min: %.1f C\r\n", (double)pack->pack_min_temp_C);
  FEB_Console_Printf("Max: %.1f C\r\n", (double)pack->pack_max_temp_C);
  FEB_Console_Printf("Avg: %.1f C\r\n", (double)pack->pack_avg_temp_C);

  FEB_Console_Printf("\r\n--- Errors ---\r\n");
  FEB_Console_Printf("Last Error: %d\r\n", pack->last_error);
  FEB_Console_Printf("Total PEC Errors: %lu\r\n", (unsigned long)pack->total_pec_errors);
  FEB_Console_Printf("Voltage Valid: %s\r\n", pack->voltage_valid ? "YES" : "NO");
  FEB_Console_Printf("Temp Valid: %s\r\n", pack->temp_valid ? "YES" : "NO");
}

static const FEB_Console_Cmd_t bms_cmd_status = {
    .name = "bms",
    .help = "BMS status: bms|status",
    .handler = cmd_bms_status,
};

/* ============================================================================
 * BMS Cells Command
 * ============================================================================ */

static void cmd_bms_cells(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  const BMS_PackData_t *pack = BMS_App_GetPackData();

  if (!pack->initialized)
  {
    FEB_Console_Printf("BMS not initialized\r\n");
    return;
  }

  FEB_Console_Printf("=== Cell Voltages ===\r\n");

  for (uint8_t bank = 0; bank < BMS_NUM_BANKS; bank++)
  {
    for (uint8_t ic = 0; ic < BMS_ICS_PER_BANK; ic++)
    {
      FEB_Console_Printf("Bank %d IC %d:\r\n", bank, ic);
      const BMS_ICData_t *ic_data = &pack->banks[bank].ics[ic];

      for (uint8_t cell = 0; cell < BMS_CELLS_PER_IC; cell++)
      {
        const BMS_CellData_t *c = &ic_data->cells[cell];
        FEB_Console_Printf("  C%02d: %.3fV (S:%.3fV)%s\r\n", cell + 1, (double)c->voltage_C_V, (double)c->voltage_S_V,
                           c->is_discharging ? " [BAL]" : "");
      }
    }
  }
}

static const FEB_Console_Cmd_t bms_cmd_cells = {
    .name = "cells",
    .help = "Cell voltages: cells",
    .handler = cmd_bms_cells,
};

/* ============================================================================
 * BMS Temps Command
 * ============================================================================ */

static void cmd_bms_temps(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  const BMS_PackData_t *pack = BMS_App_GetPackData();

  if (!pack->initialized)
  {
    FEB_Console_Printf("BMS not initialized\r\n");
    return;
  }

  FEB_Console_Printf("=== Temperature Sensors ===\r\n");

  for (uint8_t bank = 0; bank < BMS_NUM_BANKS; bank++)
  {
    FEB_Console_Printf("Bank %d:\r\n", bank);
    for (uint8_t i = 0; i < BMS_TEMP_TOTAL_SENSORS; i++)
    {
      float temp = pack->banks[bank].temp_sensors_C[i];
      FEB_Console_Printf("  T%02d: %.1f C\r\n", i + 1, (double)temp);
    }
  }
}

static const FEB_Console_Cmd_t bms_cmd_temps = {
    .name = "temps",
    .help = "Temperature sensors: temps",
    .handler = cmd_bms_temps,
};

/* ============================================================================
 * bms|regs  -  Register mirror inspection
 *   bms|regs                      - summary of all register groups + freshness
 *   bms|regs|<group>              - dump a specific register group (hex)
 *   bms|regs|fresh <max_age_ms>   - show groups fresher than max_age_ms
 *
 * Group names are the ADBMS_REG_* suffixes in lowercase (e.g. "cfga", "cvall").
 * ============================================================================ */

static int _find_reg_group_by_name(const char *name)
{
  for (int i = 0; i < (int)ADBMS_REG_COUNT; i++)
  {
    const char *rn = ADBMS_GetRegisterName((ADBMS_RegGroup_t)i);
    if (rn == NULL)
      continue;
    if (FEB_strcasecmp(name, rn) == 0)
      return i;
  }
  return -1;
}

static void cmd_bms_regs(int argc, char *argv[])
{
  uint32_t now = osKernelGetTickCount();

  if (argc >= 2 && FEB_strcasecmp(argv[1], "fresh") == 0)
  {
    uint32_t max_age = (argc >= 3) ? (uint32_t)atoi(argv[2]) : 500u;
    FEB_Console_Printf("Register groups fresh within %lums:\r\n", (unsigned long)max_age);
    for (int i = 0; i < (int)ADBMS_REG_COUNT; i++)
    {
      uint32_t t = ADBMS_GetRegisterLastTickMs((ADBMS_RegGroup_t)i);
      if (t == 0)
        continue;
      uint32_t age = now - t;
      if (age <= max_age)
      {
        FEB_Console_Printf("  %-7s  age=%lums\r\n", ADBMS_GetRegisterName((ADBMS_RegGroup_t)i), (unsigned long)age);
      }
    }
    return;
  }

  if (argc >= 2)
  {
    int grp = _find_reg_group_by_name(argv[1]);
    if (grp < 0)
    {
      FEB_Console_Printf("Unknown register group: %s\r\n", argv[1]);
      return;
    }
    FEB_Console_Printf("=== Register %s ===\r\n", ADBMS_GetRegisterName((ADBMS_RegGroup_t)grp));
    uint32_t t = ADBMS_GetRegisterLastTickMs((ADBMS_RegGroup_t)grp);
    FEB_Console_Printf("Last update: %lums (age %lums)\r\n", (unsigned long)t, t == 0 ? 0UL : (unsigned long)(now - t));
    ADBMS_DumpRegister((ADBMS_RegGroup_t)grp, (ADBMS_PrintFunc_t)FEB_Console_Printf);
    return;
  }

  FEB_Console_Printf("=== ADBMS Register Mirror ===\r\n");
  FEB_Console_Printf("Tick now: %lums\r\n", (unsigned long)now);
  FEB_Console_Printf("%-8s %-12s %s\r\n", "group", "last_tick", "age_ms");
  for (int i = 0; i < (int)ADBMS_REG_COUNT; i++)
  {
    uint32_t t = ADBMS_GetRegisterLastTickMs((ADBMS_RegGroup_t)i);
    const char *name = ADBMS_GetRegisterName((ADBMS_RegGroup_t)i);
    if (t == 0)
    {
      FEB_Console_Printf("%-8s %-12s %s\r\n", name ? name : "?", "-", "never");
    }
    else
    {
      FEB_Console_Printf("%-8s %-12lu %lu\r\n", name ? name : "?", (unsigned long)t, (unsigned long)(now - t));
    }
  }
}

static const FEB_Console_Cmd_t bms_cmd_regs = {
    .name = "regs",
    .help = "ADBMS register mirror: regs | regs|<name> | regs|fresh|<max_age_ms>",
    .handler = cmd_bms_regs,
};

/* ============================================================================
 * bms|cfg  -  Show / modify runtime BMS configuration
 *   cfg                         - print all cfg values
 *   cfg|fc|<0..7>               - set IIR filter coefficient (CFGA.FC)
 *   cfg|cth|<0..7>              - set C/S threshold (CFGA.CTH)
 *   cfg|rd|<0|1>                - enable C-ADC redundancy
 *   cfg|ow|<0..3>               - open-wire test mode on C-ADC
 *   cfg|owrng|<0|1>             - AUX open-wire range (CFGA.OWRNG)
 *   cfg|owa|<0..7>              - AUX open-wire soak (CFGA.OWA)
 *   cfg|soak|<0|1>              - AUX soak-on (CFGA.SOAKON)
 *   cfg|bal|<0|1>               - enable/disable balancing output
 * ============================================================================ */

static void _print_cfg(void)
{
  uint8_t rd, dcp, ow;
  BMS_Acq_GetCellADCOptions(&rd, &dcp, &ow);

  FEB_Console_Printf("=== BMS Configuration ===\r\n");
  FEB_Console_Printf("  fc     = %d (IIR filter coefficient)\r\n", BMS_Proc_GetIIRFilterCoeff());
  FEB_Console_Printf("  cth    = %d (C/S threshold)\r\n", BMS_Proc_GetCSThreshold());
  FEB_Console_Printf("  rd     = %u (redundancy)\r\n", (unsigned)rd);
  FEB_Console_Printf("  ow     = %u (C-ADC open-wire mode)\r\n", (unsigned)ow);
  FEB_Console_Printf("  dcp    = %u (discharge permitted during ADCV)\r\n", (unsigned)dcp);
  FEB_Console_Printf("  owrng  = %d (AUX open-wire range)\r\n", BMS_Proc_GetOpenWireRange() ? 1 : 0);
  FEB_Console_Printf("  owa    = %u (AUX open-wire soak)\r\n", (unsigned)BMS_Proc_GetOpenWireTime());
  FEB_Console_Printf("  soakon = %d (AUX soak-on)\r\n", BMS_Proc_GetSoakOn() ? 1 : 0);
  FEB_Console_Printf("  bal    = %d (balancing enabled)\r\n", BMS_Proc_IsBalancingEnabled() ? 1 : 0);
}

static void cmd_bms_cfg(int argc, char *argv[])
{
  if (argc < 2)
  {
    _print_cfg();
    return;
  }

  if (argc < 3)
  {
    FEB_Console_Printf("Usage: cfg|<key>|<value>\r\n");
    FEB_Console_Printf("Keys: fc cth rd ow owrng owa soak bal\r\n");
    return;
  }

  const char *key = argv[1];
  int value = atoi(argv[2]);

  if (FEB_strcasecmp(key, "fc") == 0)
  {
    if (value < 0 || value > 7)
    {
      FEB_Console_Printf("fc out of range (0..7)\r\n");
      return;
    }
    BMS_Proc_SetIIRFilterCoeff((ADBMS_FC_t)value);
    FEB_Console_Printf("fc <- %d (staged; CFGA pending)\r\n", value);
  }
  else if (FEB_strcasecmp(key, "cth") == 0)
  {
    if (value < 0 || value > 7)
    {
      FEB_Console_Printf("cth out of range (0..7)\r\n");
      return;
    }
    BMS_Proc_SetCSThreshold((ADBMS_CTH_t)value);
    FEB_Console_Printf("cth <- %d (staged; CFGA pending)\r\n", value);
  }
  else if (FEB_strcasecmp(key, "rd") == 0)
  {
    if (value < 0 || value > 1)
    {
      FEB_Console_Printf("rd must be 0 or 1\r\n");
      return;
    }
    BMS_Proc_SetRedundancyMode((uint8_t)value);
    FEB_Console_Printf("rd <- %d\r\n", value);
  }
  else if (FEB_strcasecmp(key, "ow") == 0)
  {
    if (value < 0 || value > 3)
    {
      FEB_Console_Printf("ow out of range (0..3)\r\n");
      return;
    }
    BMS_Proc_SetOpenWireMode((uint8_t)value);
    FEB_Console_Printf("ow <- %d\r\n", value);
  }
  else if (FEB_strcasecmp(key, "owrng") == 0)
  {
    BMS_Proc_SetOpenWireRange(value != 0);
    FEB_Console_Printf("owrng <- %d (staged)\r\n", value != 0);
  }
  else if (FEB_strcasecmp(key, "owa") == 0)
  {
    if (value < 0 || value > 7)
    {
      FEB_Console_Printf("owa out of range (0..7)\r\n");
      return;
    }
    BMS_Proc_SetOpenWireTime((uint8_t)value);
    FEB_Console_Printf("owa <- %d (staged)\r\n", value);
  }
  else if (FEB_strcasecmp(key, "soak") == 0 || FEB_strcasecmp(key, "soakon") == 0)
  {
    BMS_Proc_SetSoakOn(value != 0);
    FEB_Console_Printf("soakon <- %d (staged)\r\n", value != 0);
  }
  else if (FEB_strcasecmp(key, "bal") == 0)
  {
    BMS_Proc_SetBalancingEnabled(value != 0);
    FEB_Console_Printf("balancing <- %d\r\n", value != 0);
  }
  else
  {
    FEB_Console_Printf("Unknown cfg key: %s\r\n", key);
  }
}

static const FEB_Console_Cmd_t bms_cmd_cfg = {
    .name = "cfg",
    .help = "BMS runtime config: cfg | cfg|<fc|cth|rd|ow|owrng|owa|soak|bal>|<val>",
    .handler = cmd_bms_cfg,
};

/* ============================================================================
 * bms|mode  -  Get/set operating mode (affects UV/OV thresholds)
 *   mode                - print current mode
 *   mode|normal         - set normal mode
 *   mode|charging       - set charging mode
 *   mode|balancing      - set balancing mode
 * ============================================================================ */

static const char *_mode_name(BMS_OpMode_t m)
{
  switch (m)
  {
  case BMS_MODE_NORMAL:
    return "normal";
  case BMS_MODE_CHARGING:
    return "charging";
  case BMS_MODE_BALANCING:
    return "balancing";
  default:
    return "?";
  }
}

static void cmd_bms_mode(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Mode: %s\r\n", _mode_name(BMS_App_GetMode()));
    FEB_Console_Printf("Usage: mode|<normal|charging|balancing>\r\n");
    return;
  }

  BMS_OpMode_t target;
  if (FEB_strcasecmp(argv[1], "normal") == 0)
    target = BMS_MODE_NORMAL;
  else if (FEB_strcasecmp(argv[1], "charging") == 0)
    target = BMS_MODE_CHARGING;
  else if (FEB_strcasecmp(argv[1], "balancing") == 0)
    target = BMS_MODE_BALANCING;
  else
  {
    FEB_Console_Printf("Unknown mode: %s\r\n", argv[1]);
    return;
  }

  BMS_App_SetMode(target);
  FEB_Console_Printf("mode <- %s (UV/OV thresholds re-staged)\r\n", _mode_name(target));
}

static const FEB_Console_Cmd_t bms_cmd_mode = {
    .name = "mode",
    .help = "BMS operating mode: mode | mode|<normal|charging|balancing>",
    .handler = cmd_bms_mode,
};

/* ============================================================================
 * bms|filtered  -  Show hardware-filtered voltages (RDFCALL mirror)
 * ============================================================================ */

static void cmd_bms_filtered(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("=== Filtered (IIR) Cell Voltages ===\r\n");
  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    uint8_t bank = ic / BMS_ICS_PER_BANK;
    uint8_t ic_in_bank = ic % BMS_ICS_PER_BANK;
    FEB_Console_Printf("Bank %d IC %d:\r\n", bank, ic_in_bank);
    for (uint8_t c = 0; c < BMS_CELLS_PER_IC; c++)
    {
      int32_t f_mv = ADBMS_GetFilteredCellVoltage_mV(ic, c);
      int32_t a_mv = ADBMS_GetAvgCellVoltage_mV(ic, c);
      FEB_Console_Printf("  C%02u: filt=%4dmV avg=%4dmV\r\n", (unsigned)(c + 1), (int)f_mv, (int)a_mv);
    }
  }
  uint32_t fc_tick = ADBMS_GetRegisterLastTickMs(ADBMS_REG_CVALL);
  FEB_Console_Printf("(CVALL last updated: %lums)\r\n", (unsigned long)fc_tick);
}

static const FEB_Console_Cmd_t bms_cmd_filtered = {
    .name = "filtered",
    .help = "Hardware-filtered + averaged cell voltages",
    .handler = cmd_bms_filtered,
};

/* ============================================================================
 * bms|jobs  -  Acquisition scheduler visibility
 *   jobs                            - show all job stats + periods
 *   jobs|period|<job_index>|<ms>    - change job period (0 disables)
 *   jobs|enable|<job_index>|<0|1>   - enable/disable a job
 *   jobs|run|<job_index>            - run job once synchronously
 * ============================================================================ */

static void _print_jobs(void)
{
  FEB_Console_Printf("=== Acquisition Jobs ===\r\n");
  FEB_Console_Printf("%-3s %-16s %-9s %-8s %-8s %-8s\r\n", "idx", "name", "period", "runs", "errors", "last_dt");
  for (uint8_t i = 0; i < (uint8_t)BMS_ACQ_JOB_COUNT; i++)
  {
    const BMS_Acq_JobStats_t *s = BMS_Acq_GetJobStats((BMS_Acq_Job_t)i);
    const char *name = BMS_Acq_GetJobName((BMS_Acq_Job_t)i);
    uint32_t period = BMS_Acq_GetJobPeriod((BMS_Acq_Job_t)i);
    bool on = BMS_Acq_IsJobEnabled((BMS_Acq_Job_t)i);
    FEB_Console_Printf("%-3u %-16s %4lums%-2s %-8lu %-8lu %-8lu\r\n", (unsigned)i, name ? name : "?",
                       (unsigned long)period, on ? "" : "*", (unsigned long)(s ? s->runs : 0),
                       (unsigned long)(s ? s->errors : 0), (unsigned long)(s ? s->last_duration_ticks : 0));
  }
  FEB_Console_Printf("(* = disabled)\r\n");
}

static void cmd_bms_jobs(int argc, char *argv[])
{
  if (argc < 2)
  {
    _print_jobs();
    return;
  }

  if (argc < 3)
  {
    FEB_Console_Printf("Usage: jobs | jobs|<period|enable|run>|<idx>[|val]\r\n");
    return;
  }

  if (FEB_strcasecmp(argv[1], "period") == 0 && argc >= 4)
  {
    int idx = atoi(argv[2]);
    int ms = atoi(argv[3]);
    if (idx < 0 || idx >= (int)BMS_ACQ_JOB_COUNT)
    {
      FEB_Console_Printf("idx out of range\r\n");
      return;
    }
    BMS_Acq_SetJobPeriod((BMS_Acq_Job_t)idx, (uint32_t)ms);
    FEB_Console_Printf("job[%d] period <- %dms\r\n", idx, ms);
  }
  else if (FEB_strcasecmp(argv[1], "enable") == 0 && argc >= 4)
  {
    int idx = atoi(argv[2]);
    int v = atoi(argv[3]);
    if (idx < 0 || idx >= (int)BMS_ACQ_JOB_COUNT)
    {
      FEB_Console_Printf("idx out of range\r\n");
      return;
    }
    BMS_Acq_SetJobEnabled((BMS_Acq_Job_t)idx, v != 0);
    FEB_Console_Printf("job[%d] enabled <- %d\r\n", idx, v != 0);
  }
  else if (FEB_strcasecmp(argv[1], "run") == 0)
  {
    int idx = atoi(argv[2]);
    if (idx < 0 || idx >= (int)BMS_ACQ_JOB_COUNT)
    {
      FEB_Console_Printf("idx out of range\r\n");
      return;
    }
    ADBMS_Error_t err = BMS_Acq_RunJobNow((BMS_Acq_Job_t)idx);
    FEB_Console_Printf("job[%d] run -> %d\r\n", idx, err);
  }
  else
  {
    FEB_Console_Printf("Unknown jobs subcommand\r\n");
  }
}

static const FEB_Console_Cmd_t bms_cmd_jobs = {
    .name = "jobs",
    .help = "Acquisition scheduler: jobs | jobs|<period|enable|run>|<idx>[|v]",
    .handler = cmd_bms_jobs,
};

/* ============================================================================
 * bms|pec  -  PEC-error diagnostics
 * ============================================================================ */

static void cmd_bms_pec(int argc, char *argv[])
{
  if (argc >= 2 && FEB_strcasecmp(argv[1], "reset") == 0)
  {
    BMS_Acq_ResetConsecutivePECErrors();
    BMS_App_ClearError();
    FEB_Console_Printf("PEC counters reset\r\n");
    return;
  }

  FEB_Console_Printf("=== PEC Diagnostics ===\r\n");
  FEB_Console_Printf("Consecutive (acq): %lu\r\n", (unsigned long)BMS_Acq_GetConsecutivePECErrors());
  FEB_Console_Printf("Total pack PEC  : %lu\r\n", (unsigned long)BMS_App_GetTotalPECErrors());
  for (uint8_t ic = 0; ic < BMS_TOTAL_ICS; ic++)
  {
    uint8_t b = ic / BMS_ICS_PER_BANK;
    uint8_t ii = ic % BMS_ICS_PER_BANK;
    FEB_Console_Printf("  IC[%u] (B%u/IC%u): %lu\r\n", (unsigned)ic, (unsigned)b, (unsigned)ii,
                       (unsigned long)BMS_App_GetICPECErrors(b, ii));
  }
}

static const FEB_Console_Cmd_t bms_cmd_pec = {
    .name = "pec",
    .help = "PEC error stats: pec | pec|reset",
    .handler = cmd_bms_pec,
};

/* ============================================================================
 * Command Registration
 * ============================================================================ */

void BMS_RegisterCommands(void)
{
  FEB_Console_Register(&bms_cmd_status);
  FEB_Console_Register(&bms_cmd_cells);
  FEB_Console_Register(&bms_cmd_temps);
  FEB_Console_Register(&bms_cmd_regs);
  FEB_Console_Register(&bms_cmd_cfg);
  FEB_Console_Register(&bms_cmd_mode);
  FEB_Console_Register(&bms_cmd_filtered);
  FEB_Console_Register(&bms_cmd_jobs);
  FEB_Console_Register(&bms_cmd_pec);
}
