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
 * Command Registration
 * ============================================================================ */

void BMS_RegisterCommands(void)
{
  FEB_Console_Register(&bms_cmd_status);
  FEB_Console_Register(&bms_cmd_cells);
  FEB_Console_Register(&bms_cmd_temps);
}
