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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Case-insensitive string comparison
 */
static int strcasecmp_local(const char *s1, const char *s2)
{
  while (*s1 && *s2)
  {
    int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
    if (diff != 0)
      return diff;
    s1++;
    s2++;
  }
  return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

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
  case FEB_SM_ST_OFF:
    state_str = "OFF";
    break;
  case FEB_SM_ST_IDLE:
    state_str = "IDLE";
    break;
  case FEB_SM_ST_PRECHARGE:
    state_str = "PRECHARGE";
    break;
  case FEB_SM_ST_DRIVE:
    state_str = "DRIVE";
    break;
  case FEB_SM_ST_CHARGE:
    state_str = "CHARGE";
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
  case FEB_SM_ST_HEALTH_CHECK:
    state_str = "HEALTH_CHECK";
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
  case FEB_SM_ST_OFF:
    FEB_Console_Printf("OFF\r\n");
    break;
  case FEB_SM_ST_IDLE:
    FEB_Console_Printf("IDLE\r\n");
    break;
  case FEB_SM_ST_PRECHARGE:
    FEB_Console_Printf("PRECHARGE\r\n");
    break;
  case FEB_SM_ST_DRIVE:
    FEB_Console_Printf("DRIVE\r\n");
    break;
  case FEB_SM_ST_CHARGE:
    FEB_Console_Printf("CHARGE\r\n");
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
  case FEB_SM_ST_HEALTH_CHECK:
    FEB_Console_Printf("HEALTH_CHECK\r\n");
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

  if (strcasecmp_local(subcmd, "status") == 0)
  {
    cmd_status();
  }
  else if (strcasecmp_local(subcmd, "apps") == 0)
  {
    cmd_apps();
  }
  else if (strcasecmp_local(subcmd, "brake") == 0)
  {
    cmd_brake();
  }
  else if (strcasecmp_local(subcmd, "rms") == 0)
  {
    cmd_rms();
  }
  else if (strcasecmp_local(subcmd, "tps") == 0)
  {
    cmd_tps();
  }
  else if (strcasecmp_local(subcmd, "bms") == 0)
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
};

/* ============================================================================
 * Registration Function
 * ============================================================================ */

void PCU_RegisterCommands(void)
{
  FEB_Console_Register(&pcu_cmd);
}
