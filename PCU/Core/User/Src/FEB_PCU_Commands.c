/**
 ******************************************************************************
 * @file           : FEB_PCU_Commands.c
 * @brief          : PCU Custom Console Command Implementations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_PCU_Commands.h"
#include "FEB_ADC.h"
#include "FEB_CAN_BMS.h"
#include "FEB_CAN_RMS.h"
#include "FEB_CAN_TPS.h"
#include "FEB_RMS.h"
#include "feb_console.h"
#include "feb_string_utils.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Subcommand Handlers
 * ============================================================================ */

static void print_pcu_help(void)
{
  FEB_Console_Printf("PCU Commands:\r\n");
  FEB_Console_Printf("  PCU|status   - Show overall PCU status\r\n");
  FEB_Console_Printf("  PCU|apps     - Show APPS sensor values and plausibility\r\n");
  FEB_Console_Printf("  PCU|brake    - Show brake sensor values and status\r\n");
  FEB_Console_Printf("  PCU|rms      - Show RMS motor controller status\r\n");
  FEB_Console_Printf("  PCU|tps      - Show TPS2482 voltage/current monitoring\r\n");
  FEB_Console_Printf("  PCU|bms      - Show BMS state information\r\n");
}

static void cmd_status(void)
{
  APPS_DataTypeDef apps_data;
  Brake_DataTypeDef brake_data;

  FEB_ADC_GetAPPSData(&apps_data);
  FEB_ADC_GetBrakeData(&brake_data);

  FEB_Console_Printf("=== PCU Status ===\r\n");
  FEB_Console_Printf("\r\n");

  // APPS Status
  FEB_Console_Printf("APPS: %.1f%% (Avg) | %s\r\n", apps_data.acceleration,
                     apps_data.plausible ? "PLAUSIBLE" : "IMPLAUSIBLE");

  // Brake Status
  FEB_Console_Printf("Brake: %.1f%% | %s\r\n", brake_data.brake_position,
                     brake_data.brake_pressed ? "PRESSED" : "RELEASED");

  // BMS State
  FEB_SM_ST_t bms_state = FEB_CAN_BMS_getState();
  const char *state_str;
  switch (bms_state)
  {
  case FEB_SM_ST_BOOT:
    state_str = "BOOT";
    break;
  case FEB_SM_ST_LV_POWER:
    state_str = "LV_POWER";
    break;
  case FEB_SM_ST_BUS_HEALTH_CHECK:
    state_str = "HEALTH_CHECK";
    break;
  case FEB_SM_ST_PRECHARGE:
    state_str = "PRECHARGE";
    break;
  case FEB_SM_ST_ENERGIZED:
    state_str = "ENERGIZED";
    break;
  case FEB_SM_ST_DRIVE:
    state_str = "DRIVE";
    break;
  case FEB_SM_ST_BATTERY_FREE:
    state_str = "BATTERY_FREE";
    break;
  case FEB_SM_ST_CHARGER_PRECHARGE:
    state_str = "CHARGER_PRECHARGE";
    break;
  case FEB_SM_ST_CHARGING:
    state_str = "CHARGING";
    break;
  case FEB_SM_ST_BALANCE:
    state_str = "BALANCE";
    break;
  case FEB_SM_ST_FAULT_BMS:
    state_str = "FAULT_BMS";
    break;
  case FEB_SM_ST_FAULT_BSPD:
    state_str = "FAULT_BSPD";
    break;
  case FEB_SM_ST_FAULT_IMD:
    state_str = "FAULT_IMD";
    break;
  case FEB_SM_ST_FAULT_CHARGING:
    state_str = "FAULT_CHARGING";
    break;
  default:
    state_str = "UNKNOWN";
    break;
  }
  FEB_Console_Printf("BMS State: %s\r\n", state_str);

  // TPS Status
  FEB_CAN_TPS_Data_t tps_data;
  FEB_CAN_TPS_GetData(&tps_data);
  FEB_Console_Printf("12V Rail: %u mV, %d mA\r\n", tps_data.bus_voltage_mv, tps_data.current_ma);
}

static void cmd_apps(void)
{
  APPS_DataTypeDef apps_data;
  FEB_ADC_GetAPPSData(&apps_data);

  FEB_Console_Printf("=== APPS Sensor Data ===\r\n");
  FEB_Console_Printf("\r\n");

  FEB_Console_Printf("APPS1:\r\n");
  FEB_Console_Printf("  Raw ADC:  %d\r\n", FEB_ADC_GetAccelPedal1Raw());
  FEB_Console_Printf("  Voltage:  %.3f V\r\n", FEB_ADC_GetAccelPedal1Voltage());
  FEB_Console_Printf("  Position: %.1f%%\r\n", apps_data.position1);
  FEB_Console_Printf("\r\n");

  FEB_Console_Printf("APPS2:\r\n");
  FEB_Console_Printf("  Raw ADC:  %d\r\n", FEB_ADC_GetAccelPedal2Raw());
  FEB_Console_Printf("  Voltage:  %.3f V\r\n", FEB_ADC_GetAccelPedal2Voltage());
  FEB_Console_Printf("  Position: %.1f%%\r\n", apps_data.position2);
  FEB_Console_Printf("\r\n");

  FEB_Console_Printf("Combined:\r\n");
  FEB_Console_Printf("  Acceleration: %.1f%%\r\n", apps_data.acceleration);
  FEB_Console_Printf("  Plausibility: %s\r\n", apps_data.plausible ? "OK" : "FAILED");
}

static void cmd_brake(void)
{
  Brake_DataTypeDef brake_data;
  FEB_ADC_GetBrakeData(&brake_data);

  FEB_Console_Printf("=== Brake Sensor Data ===\r\n");
  FEB_Console_Printf("\r\n");

  FEB_Console_Printf("Brake 1:\r\n");
  FEB_Console_Printf("  Raw ADC:  %d\r\n", FEB_ADC_GetBrakePressure1Raw());
  FEB_Console_Printf("  Voltage:  %.3f V\r\n", FEB_ADC_GetBrakePressure1Voltage());
  FEB_Console_Printf("  Pressure: %.1f%%\r\n", brake_data.pressure1_percent);
  FEB_Console_Printf("\r\n");

  FEB_Console_Printf("Brake 2:\r\n");
  FEB_Console_Printf("  Raw ADC:  %d\r\n", FEB_ADC_GetBrakePressure2Raw());
  FEB_Console_Printf("  Voltage:  %.3f V\r\n", FEB_ADC_GetBrakePressure2Voltage());
  FEB_Console_Printf("  Pressure: %.1f%%\r\n", brake_data.pressure2_percent);
  FEB_Console_Printf("\r\n");

  FEB_Console_Printf("Combined:\r\n");
  FEB_Console_Printf("  Position: %.1f%%\r\n", brake_data.brake_position);
  FEB_Console_Printf("  Pressed:  %s\r\n", brake_data.brake_pressed ? "YES" : "NO");
}

static void cmd_rms(void)
{
  FEB_Console_Printf("=== RMS Motor Controller Status ===\r\n");
  FEB_Console_Printf("\r\n");

  // Get RMS data (these functions may need to be added to FEB_CAN_RMS.h)
  FEB_Console_Printf("DC Bus Voltage:  %.1f V\r\n", FEB_CAN_RMS_getDCBusVoltage());
  FEB_Console_Printf("Motor Speed:     %d RPM\r\n", FEB_CAN_RMS_getMotorSpeed());
  FEB_Console_Printf("Motor Angle:     %d deg\r\n", FEB_CAN_RMS_getMotorAngle());
  FEB_Console_Printf("Commanded Torque: %.1f Nm\r\n", FEB_CAN_RMS_getTorqueCommand());
  FEB_Console_Printf("Feedback Torque:  %.1f Nm\r\n", FEB_CAN_RMS_getTorqueFeedback());
}

static void cmd_tps(void)
{
  FEB_CAN_TPS_Data_t tps_data;
  FEB_CAN_TPS_GetData(&tps_data);

  FEB_Console_Printf("=== TPS2482 Power Monitor ===\r\n");
  FEB_Console_Printf("\r\n");

  FEB_Console_Printf("12V Rail:\r\n");
  FEB_Console_Printf("  Bus Voltage:  %u mV\r\n", tps_data.bus_voltage_mv);
  FEB_Console_Printf("  Current:      %d mA\r\n", tps_data.current_ma);
  FEB_Console_Printf("  Shunt Voltage: %ld uV\r\n", tps_data.shunt_voltage_uv);
}

static void cmd_bms(void)
{
  FEB_Console_Printf("=== BMS State Information ===\r\n");
  FEB_Console_Printf("\r\n");

  FEB_SM_ST_t bms_state = FEB_CAN_BMS_getState();

  FEB_Console_Printf("State: ");
  switch (bms_state)
  {
  case FEB_SM_ST_BOOT:
    FEB_Console_Printf("BOOT\r\n");
    break;
  case FEB_SM_ST_LV_POWER:
    FEB_Console_Printf("LV_POWER\r\n");
    break;
  case FEB_SM_ST_BUS_HEALTH_CHECK:
    FEB_Console_Printf("HEALTH_CHECK\r\n");
    break;
  case FEB_SM_ST_PRECHARGE:
    FEB_Console_Printf("PRECHARGE\r\n");
    break;
  case FEB_SM_ST_ENERGIZED:
    FEB_Console_Printf("ENERGIZED\r\n");
    break;
  case FEB_SM_ST_DRIVE:
    FEB_Console_Printf("DRIVE\r\n");
    break;
  case FEB_SM_ST_BATTERY_FREE:
    FEB_Console_Printf("BATTERY_FREE\r\n");
    break;
  case FEB_SM_ST_CHARGER_PRECHARGE:
    FEB_Console_Printf("CHARGER_PRECHARGE\r\n");
    break;
  case FEB_SM_ST_CHARGING:
    FEB_Console_Printf("CHARGING\r\n");
    break;
  case FEB_SM_ST_BALANCE:
    FEB_Console_Printf("BALANCE\r\n");
    break;
  case FEB_SM_ST_FAULT_BMS:
    FEB_Console_Printf("FAULT_BMS\r\n");
    break;
  case FEB_SM_ST_FAULT_BSPD:
    FEB_Console_Printf("FAULT_BSPD\r\n");
    break;
  case FEB_SM_ST_FAULT_IMD:
    FEB_Console_Printf("FAULT_IMD\r\n");
    break;
  case FEB_SM_ST_FAULT_CHARGING:
    FEB_Console_Printf("FAULT_CHARGING\r\n");
    break;
  default:
    FEB_Console_Printf("UNKNOWN (%d)\r\n", bms_state);
    break;
  }

  // Get additional BMS data
  FEB_Console_Printf("Accumulator Voltage: %.1f V\r\n", FEB_CAN_BMS_getAccumulatorVoltage());
  FEB_Console_Printf("Max Temperature:     %.1f C\r\n", FEB_CAN_BMS_getMaxTemperature());
}

/* ============================================================================
 * CSV Helpers
 * ============================================================================ */

static const char *bms_state_name(FEB_SM_ST_t s)
{
  switch (s)
  {
  case FEB_SM_ST_BOOT:
    return "BOOT";
  case FEB_SM_ST_LV_POWER:
    return "LV_POWER";
  case FEB_SM_ST_BUS_HEALTH_CHECK:
    return "HEALTH_CHECK";
  case FEB_SM_ST_PRECHARGE:
    return "PRECHARGE";
  case FEB_SM_ST_ENERGIZED:
    return "ENERGIZED";
  case FEB_SM_ST_DRIVE:
    return "DRIVE";
  case FEB_SM_ST_BATTERY_FREE:
    return "BATTERY_FREE";
  case FEB_SM_ST_CHARGER_PRECHARGE:
    return "CHARGER_PRECHARGE";
  case FEB_SM_ST_CHARGING:
    return "CHARGING";
  case FEB_SM_ST_BALANCE:
    return "BALANCE";
  case FEB_SM_ST_FAULT_BMS:
    return "FAULT_BMS";
  case FEB_SM_ST_FAULT_BSPD:
    return "FAULT_BSPD";
  case FEB_SM_ST_FAULT_IMD:
    return "FAULT_IMD";
  case FEB_SM_ST_FAULT_CHARGING:
    return "FAULT_CHARGING";
  default:
    return "UNKNOWN";
  }
}

static void csv_status(void)
{
  APPS_DataTypeDef apps_data;
  Brake_DataTypeDef brake_data;
  FEB_CAN_TPS_Data_t tps_data;
  FEB_ADC_GetAPPSData(&apps_data);
  FEB_ADC_GetBrakeData(&brake_data);
  FEB_CAN_TPS_GetData(&tps_data);
  FEB_SM_ST_t bms_state = FEB_CAN_BMS_getState();
  FEB_Console_CsvPrintf("pcuStat", "%.1f,%d,%.1f,%d,%s,%u,%d\r\n", apps_data.acceleration, apps_data.plausible ? 1 : 0,
                        brake_data.brake_position, brake_data.brake_pressed ? 1 : 0, bms_state_name(bms_state),
                        tps_data.bus_voltage_mv, tps_data.current_ma);
}

static void csv_apps(void)
{
  APPS_DataTypeDef apps_data;
  FEB_ADC_GetAPPSData(&apps_data);
  FEB_Console_CsvPrintf("pcuApps", "%d,%.3f,%.1f,%d,%.3f,%.1f,%.1f,%d\r\n", FEB_ADC_GetAccelPedal1Raw(),
                        FEB_ADC_GetAccelPedal1Voltage(), apps_data.position1, FEB_ADC_GetAccelPedal2Raw(),
                        FEB_ADC_GetAccelPedal2Voltage(), apps_data.position2, apps_data.acceleration,
                        apps_data.plausible ? 1 : 0);
}

static void csv_brake(void)
{
  Brake_DataTypeDef brake_data;
  FEB_ADC_GetBrakeData(&brake_data);
  FEB_Console_CsvPrintf("pcuBrake", "%d,%.3f,%.1f,%d,%.3f,%.1f,%.1f,%d\r\n", FEB_ADC_GetBrakePressure1Raw(),
                        FEB_ADC_GetBrakePressure1Voltage(), brake_data.pressure1_percent,
                        FEB_ADC_GetBrakePressure2Raw(), FEB_ADC_GetBrakePressure2Voltage(),
                        brake_data.pressure2_percent, brake_data.brake_position, brake_data.brake_pressed ? 1 : 0);
}

static void csv_rms(void)
{
  FEB_Console_CsvPrintf("pcuRms", "%.1f,%d,%d,%.1f,%.1f\r\n", FEB_CAN_RMS_getDCBusVoltage(),
                        FEB_CAN_RMS_getMotorSpeed(), FEB_CAN_RMS_getMotorAngle(), FEB_CAN_RMS_getTorqueCommand(),
                        FEB_CAN_RMS_getTorqueFeedback());
}

static void csv_tps(void)
{
  FEB_CAN_TPS_Data_t tps_data;
  FEB_CAN_TPS_GetData(&tps_data);
  FEB_Console_CsvPrintf("pcuTps", "%u,%d,%ld\r\n", tps_data.bus_voltage_mv, tps_data.current_ma,
                        (long)tps_data.shunt_voltage_uv);
}

static void csv_bms(void)
{
  FEB_SM_ST_t bms_state = FEB_CAN_BMS_getState();
  FEB_Console_CsvPrintf("pcuBms", "%s,%d,%.1f,%.1f\r\n", bms_state_name(bms_state), (int)bms_state,
                        FEB_CAN_BMS_getAccumulatorVoltage(), FEB_CAN_BMS_getMaxTemperature());
}

static void cmd_pcu_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvPrintf("csv_err", "pcu_usage,status|apps|brake|rms|tps|bms\r\n");
    return;
  }
  const char *subcmd = argv[1];
  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    csv_status();
  }
  else if (FEB_strcasecmp(subcmd, "apps") == 0)
  {
    csv_apps();
  }
  else if (FEB_strcasecmp(subcmd, "brake") == 0)
  {
    csv_brake();
  }
  else if (FEB_strcasecmp(subcmd, "rms") == 0)
  {
    csv_rms();
  }
  else if (FEB_strcasecmp(subcmd, "tps") == 0)
  {
    csv_tps();
  }
  else if (FEB_strcasecmp(subcmd, "bms") == 0)
  {
    csv_bms();
  }
  else
  {
    FEB_Console_CsvPrintf("csv_err", "pcu_mode,%s\r\n", subcmd);
  }
}

/* ============================================================================
 * Main Command Handler
 * ============================================================================ */

static void cmd_pcu(int argc, char *argv[])
{
  if (argc < 2)
  {
    // No subcommand = show PCU help
    print_pcu_help();
    return;
  }

  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    cmd_status();
  }
  else if (FEB_strcasecmp(subcmd, "apps") == 0)
  {
    cmd_apps();
  }
  else if (FEB_strcasecmp(subcmd, "brake") == 0)
  {
    cmd_brake();
  }
  else if (FEB_strcasecmp(subcmd, "rms") == 0)
  {
    cmd_rms();
  }
  else if (FEB_strcasecmp(subcmd, "tps") == 0)
  {
    cmd_tps();
  }
  else if (FEB_strcasecmp(subcmd, "bms") == 0)
  {
    cmd_bms();
  }
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_pcu_help();
  }
}

/* ============================================================================
 * Command Descriptor
 * ============================================================================ */

const FEB_Console_Cmd_t pcu_cmd = {
    .name = "PCU",
    .help = "PCU board commands (PCU|status, PCU|apps, PCU|brake, etc.)",
    .handler = cmd_pcu,
    .csv_handler = cmd_pcu_csv,
};

/* ============================================================================
 * Registration Function
 * ============================================================================ */

void PCU_RegisterCommands(void)
{
  FEB_Console_Register(&pcu_cmd);
}
