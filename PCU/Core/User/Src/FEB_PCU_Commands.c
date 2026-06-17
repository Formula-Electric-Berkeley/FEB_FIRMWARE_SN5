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
#include "FEB_PCU_APPS_Commands.h"
#include "FEB_RMS.h"
#include "feb_can_lib.h"
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
  FEB_Console_Printf("  PCU|apps     - APPS summary (use PCU|apps alone for sub-commands)\r\n");
  FEB_Console_Printf("  PCU|brake    - Show brake sensor values and status\r\n");
  FEB_Console_Printf("  PCU|brake|bypass|<on|off>  - Bench brake/BSPD bypass (refused if BMS on bus)\r\n");
  FEB_Console_Printf("  PCU|rms      - Show RMS motor controller status\r\n");
  FEB_Console_Printf("  PCU|rms|raw  - Dump every inverter frame seen (raw bytes + age)\r\n");
  FEB_Console_Printf("  PCU|rms|<enable|disable>   - Manually enable/disable inverter (bench)\r\n");
  FEB_Console_Printf("  PCU|tps      - Show TPS2482 voltage/current monitoring\r\n");
  FEB_Console_Printf("  PCU|bms      - Show BMS state information\r\n");
  FEB_Console_Printf("  PCU|bms|state|<drive|0-13|off>  - Sim BMS state (bench only, refused if BMS on bus)\r\n");
  FEB_Console_Printf("  PCU|can      - CAN TX/error diagnostics (PCU|can|reset to clear)\r\n");
  FEB_Console_Printf("  PCU|faults   - Show active faults / faults|clear / faults|inject\r\n");
  FEB_Console_Printf("\r\nDeep dives:\r\n");
  FEB_Console_Printf("  PCU|apps|raw|stream|stats|cal|filter|deadzone|mode|sim\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("CSV Protocol (machine-readable):\r\n");
  FEB_Console_Printf("  PCU|csv|<tx_id>|<sub>  - any subcommand above also works as CSV\r\n");
  FEB_Console_Printf("  *|csv|<tx_id>|hello    - Discover all boards (system command)\r\n");
  FEB_Console_Printf("Each request emits: ack -> [rows] -> done\r\n");
}

static void cmd_status(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
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

static void cmd_apps(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
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

static void cmd_brake_bypass(int argc, char *argv[])
{
  /* PCU|brake|bypass|<on|off> — bench-only brake/BSPD bypass. */
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: PCU|brake|bypass|<on|off>\r\n");
    FEB_Console_Printf("  Treats brake as released+plausible and skips the BSPD check.\r\n");
    FEB_Console_Printf("  Refused if BMS is actively sending messages.\r\n");
    FEB_Console_Printf("  Current: %s\r\n", FEB_RMS_GetBrakeBypass() ? "ON" : "OFF");
    return;
  }

  const char *val = argv[1];
  bool enable;
  if (FEB_strcasecmp(val, "on") == 0)
  {
    enable = true;
  }
  else if (FEB_strcasecmp(val, "off") == 0)
  {
    enable = false;
  }
  else
  {
    FEB_Console_Printf("Error: invalid arg '%s' (use 'on' or 'off')\r\n", val);
    return;
  }

  if (!FEB_RMS_SetBrakeBypass(enable))
  {
    FEB_Console_Printf("Error: refused — BMS is active on CAN bus\r\n");
    return;
  }
  FEB_Console_Printf("Brake bypass %s\r\n", enable ? "ON" : "OFF");
}

static void cmd_brake(int argc, char *argv[])
{
  if (argc >= 2 && FEB_strcasecmp(argv[1], "bypass") == 0)
  {
    cmd_brake_bypass(argc - 1, argv + 1);
    return;
  }

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
  FEB_Console_Printf("  Bypass:   %s\r\n", FEB_RMS_GetBrakeBypass() ? "ON (bench)" : "OFF");
}

/* Decode the M170 INV_VSM_State enum to a human name. */
static const char *vsm_state_name(uint8_t s)
{
  switch (s)
  {
  case 0:
    return "VSM_START";
  case 1:
    return "PRECHARGE_INIT";
  case 2:
    return "PRECHARGE_ACTIVE";
  case 3:
    return "PRECHARGE_COMPLETE";
  case 4:
    return "VSM_WAIT";
  case 5:
    return "VSM_READY";
  case 6:
    return "MOTOR_RUNNING";
  case 7:
    return "BLINK_FAULT_CODE";
  case 14:
    return "SHUTDOWN";
  case 15:
    return "RESET";
  default:
    return "?";
  }
}

/* Short names for the inverter broadcast block, indexed by frame-table slot
 * (slot i == CAN ID 0x0A0 + i for the M160..M175 block). */
static const char *rms_frame_name(int slot)
{
  static const char *const names[FEB_CAN_RMS_FRAME_BLOCK_N] = {
      "M160 Temp1",   "M161 Temp2",      "M162 Temp3",    "M163 AnalogIn",  "M164 DigIn",  "M165 MotorPos",
      "M166 Current", "M167 Voltage",    "M168 Flux",     "M169 InternalV", "M170 States", "M171 Faults",
      "M172 Torque",  "M173 Modulation", "M174 Firmware", "M175 Diag",
  };
  if (slot >= 0 && slot < (int)FEB_CAN_RMS_FRAME_BLOCK_N)
    return names[slot];
  if (slot == FEB_CAN_RMS_FRAME_PARAM_RESP_IDX)
    return "M194 ParamResp";
  return "?";
}

/* Dump the raw per-ID capture table: every inverter frame we have received,
 * with its latest payload, receive count, and age. This is the literal "show me
 * everything coming from the inverter" view. */
static void cmd_rms_raw(void)
{
  uint32_t now = HAL_GetTick();
  FEB_Console_Printf("=== RMS Inverter Frames (raw) ===\r\n");
  FEB_Console_Printf("  ID    Name            age(ms)  count  bytes\r\n");
  int any = 0;
  for (int slot = 0; slot < FEB_CAN_RMS_FRAME_TABLE_SIZE; slot++)
  {
    const RMS_Frame_Record_t *rec = &RMS_FRAMES[slot];
    if (!rec->seen)
      continue;
    any = 1;
    uint32_t id = (slot == FEB_CAN_RMS_FRAME_PARAM_RESP_IDX) ? FEB_CAN_M194_READ_WRITE_PARAM_RESPONSE_FRAME_ID
                                                             : (FEB_CAN_RMS_FRAME_BASE_ID + (uint32_t)slot);
    FEB_Console_Printf("  0x%03X %-15s %7lu %6lu  %02X %02X %02X %02X %02X %02X %02X %02X\r\n", (unsigned)id,
                       rms_frame_name(slot), (unsigned long)(now - rec->last_rx_tick), (unsigned long)rec->count,
                       rec->data[0], rec->data[1], rec->data[2], rec->data[3], rec->data[4], rec->data[5], rec->data[6],
                       rec->data[7]);
  }
  if (!any)
    FEB_Console_Printf("  (no inverter frames seen — check bus wiring / inverter power)\r\n");
}

static void cmd_rms(int argc, char *argv[])
{
  if (argc >= 2 && FEB_strcasecmp(argv[1], "raw") == 0)
  {
    cmd_rms_raw();
    return;
  }
  if (argc >= 2 && FEB_strcasecmp(argv[1], "enable") == 0)
  {
    if (FEB_RMS_CommandEnable())
      FEB_Console_Printf("Inverter ENABLED\r\n");
    else
      FEB_Console_Printf("Inverter enable refused — BMS not in drive (use PCU|bms|state|drive on the bench)\r\n");
    return;
  }
  if (argc >= 2 && FEB_strcasecmp(argv[1], "disable") == 0)
  {
    FEB_RMS_CommandDisable();
    FEB_Console_Printf("Inverter DISABLED (forced off until PCU|rms|enable)\r\n");
    return;
  }

  FEB_Console_Printf("=== RMS Inverter Status ===\r\n\r\n");

  /* What the PCU is commanding (PCU -> INV, M192). */
  const char *inv_state;
  if (FEB_RMS_IsForceDisabled())
    inv_state = "FORCED-OFF";
  else
    inv_state = RMS_CONTROL_MESSAGE.enabled ? "ENABLED" : "DISABLED";
  FEB_Console_Printf("Command (PCU->INV):\r\n");
  FEB_Console_Printf("  Inverter enable: %s\r\n", inv_state);
  FEB_Console_Printf("  Cmd Torque:      %.1f Nm\r\n", FEB_CAN_RMS_getTorqueCommand());
  FEB_Console_Printf("\r\n");

  /* What the inverter reports back (M170 Internal States). */
  FEB_Console_Printf("Inverter state (M170):\r\n");
  if (!FEB_CAN_RMS_StatesSeen())
  {
    FEB_Console_Printf("  (no M170 broadcast — inverter silent on bus)\r\n");
  }
  else
  {
    uint8_t vsm = FEB_CAN_RMS_getVsmState();
    FEB_Console_Printf("  VSM State:       %u (%s)\r\n", vsm, vsm_state_name(vsm));
    FEB_Console_Printf("  Inverter State:  %u\r\n", FEB_CAN_RMS_getInverterState());
    FEB_Console_Printf("  Enable State:    %s\r\n", FEB_CAN_RMS_getEnableState() ? "ENABLED" : "DISABLED");
    FEB_Console_Printf("  Enable Lockout:  %s\r\n", FEB_CAN_RMS_getEnableLockout() ? "LOCKED" : "clear");
    FEB_Console_Printf("  Command Mode:    %s\r\n", FEB_CAN_RMS_getCommandModeVsm() ? "VSM" : "CAN");
    FEB_Console_Printf("  Echoed Counter:  %u\r\n", FEB_CAN_RMS_getEchoRollingCounter());
    FEB_Console_Printf("  Discharge State: %u   Run Mode: %s   Dir: %u   BMS Active: %u\r\n",
                       RMS_MESSAGE.discharge_state, RMS_MESSAGE.run_mode ? "speed" : "torque",
                       RMS_MESSAGE.direction_command, RMS_MESSAGE.bms_active);
    FEB_Console_Printf("  Relays 1-6:      %u%u%u%u%u%u   PWM: %u kHz\r\n", (RMS_MESSAGE.relay_status >> 0) & 1u,
                       (RMS_MESSAGE.relay_status >> 1) & 1u, (RMS_MESSAGE.relay_status >> 2) & 1u,
                       (RMS_MESSAGE.relay_status >> 3) & 1u, (RMS_MESSAGE.relay_status >> 4) & 1u,
                       (RMS_MESSAGE.relay_status >> 5) & 1u, RMS_MESSAGE.pwm_frequency);
  }
  FEB_Console_Printf("\r\n");

  /* Fault codes (M171). Bitfields — see PM100 manual for decode. */
  FEB_Console_Printf("Faults (M171):\r\n");
  if (!FEB_CAN_RMS_FaultsSeen())
  {
    FEB_Console_Printf("  (no M171 broadcast)\r\n");
  }
  else
  {
    FEB_Console_Printf("  POST lo=0x%04X hi=0x%04X  RUN lo=0x%04X hi=0x%04X  -> %s\r\n",
                       (unsigned)FEB_CAN_RMS_getPostFaultLo(), (unsigned)FEB_CAN_RMS_getPostFaultHi(),
                       (unsigned)FEB_CAN_RMS_getRunFaultLo(), (unsigned)FEB_CAN_RMS_getRunFaultHi(),
                       FEB_CAN_RMS_HasActiveFault() ? "ACTIVE" : "NONE");
  }
  FEB_Console_Printf("\r\n");

  /* Measured analog (M165 motor, M166 current, M167 voltage, M16x temps). */
  FEB_Console_Printf("Measured:\r\n");
  FEB_Console_Printf("  DC Bus Voltage:  %.1f V   (out %.1f / Vab %.1f / Vbc %.1f V)\r\n",
                     FEB_CAN_RMS_getDCBusVoltage(), RMS_MESSAGE.output_voltage / 10.0f,
                     RMS_MESSAGE.vab_vd_voltage / 10.0f, RMS_MESSAGE.vbc_voltage / 10.0f);
  FEB_Console_Printf("  Motor Speed:     %d RPM   Elec Freq: %.1f Hz\r\n", FEB_CAN_RMS_getMotorSpeed(),
                     RMS_MESSAGE.electrical_freq / 10.0f);
  FEB_Console_Printf("  Motor Angle:     %.1f deg\r\n", FEB_CAN_RMS_getMotorAngle() / 10.0f);
  FEB_Console_Printf("  Feedback Torque: %.1f Nm   (inv cmd echo %.1f Nm)\r\n", FEB_CAN_RMS_getTorqueFeedback(),
                     RMS_MESSAGE.inv_commanded_torque / 10.0f);
  if (RMS_MESSAGE.current_rx_timestamp != 0)
    FEB_Console_Printf("  Phase Currents:  A %.1f  B %.1f  C %.1f A   DC Bus: %.1f A\r\n",
                       RMS_MESSAGE.phase_a_current / 10.0f, RMS_MESSAGE.phase_b_current / 10.0f,
                       RMS_MESSAGE.phase_c_current / 10.0f, RMS_MESSAGE.dc_bus_current / 10.0f);
  if (RMS_MESSAGE.temps_rx_timestamp != 0)
  {
    FEB_Console_Printf("  Module Temps:    A %.1f  B %.1f  C %.1f C   Gate %.1f C\r\n",
                       RMS_MESSAGE.temp_module_a / 10.0f, RMS_MESSAGE.temp_module_b / 10.0f,
                       RMS_MESSAGE.temp_module_c / 10.0f, RMS_MESSAGE.temp_gate_driver / 10.0f);
    FEB_Console_Printf("  Motor/Ctrl Temp: motor %.1f C  control %.1f C\r\n", RMS_MESSAGE.temp_motor / 10.0f,
                       RMS_MESSAGE.temp_control_board / 10.0f);
  }
  FEB_Console_Printf("\r\n");

  /* One-line "why won't it enable" hint. */
  FEB_Console_Printf("Diagnosis: ");
  if (!FEB_CAN_RMS_StatesSeen())
    FEB_Console_Printf("no inverter broadcast seen — check bus wiring / inverter power\r\n");
  else if (FEB_CAN_RMS_getEnableLockout())
    FEB_Console_Printf("ENABLE LOCKOUT — toggle PCU|rms|disable then PCU|rms|enable\r\n");
  else if (FEB_CAN_RMS_HasActiveFault())
    FEB_Console_Printf("ACTIVE FAULT — check HV present, then clear faults\r\n");
  else if (RMS_CONTROL_MESSAGE.enabled && !FEB_CAN_RMS_getEnableState())
    FEB_Console_Printf("commanding enable but inverter not enabled yet — check HV / faults\r\n");
  else
    FEB_Console_Printf("Ready.\r\n");
}

static void cmd_tps(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_CAN_TPS_Data_t tps_data;
  FEB_CAN_TPS_GetData(&tps_data);

  FEB_Console_Printf("=== TPS2482 Power Monitor ===\r\n");
  FEB_Console_Printf("\r\n");

  FEB_Console_Printf("12V Rail:\r\n");
  FEB_Console_Printf("  Bus Voltage:  %u mV\r\n", tps_data.bus_voltage_mv);
  FEB_Console_Printf("  Current:      %d mA\r\n", tps_data.current_ma);
  FEB_Console_Printf("  Shunt Voltage: %ld uV\r\n", tps_data.shunt_voltage_uv);
}

static void cmd_bms_state(int argc, char *argv[])
{
  /* PCU|bms|state|<0-13 | drive | off> */
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: PCU|bms|state|<0-13 | drive | off>\r\n");
    FEB_Console_Printf("  Simulates BMS state when BMS is silent on CAN bus.\r\n");
    FEB_Console_Printf("  Refused if BMS is actively sending messages.\r\n");
    return;
  }

  const char *val = argv[1];

  if (FEB_strcasecmp(val, "off") == 0)
  {
    FEB_CAN_BMS_SetStateSim(false, FEB_SM_ST_BOOT);
    FEB_Console_Printf("BMS sim cleared\r\n");
    return;
  }

  FEB_SM_ST_t state;
  if (FEB_strcasecmp(val, "drive") == 0)
  {
    state = FEB_SM_ST_DRIVE;
  }
  else
  {
    char *end;
    unsigned long n = strtoul(val, &end, 10);
    if (*end != '\0' || n >= FEB_SM_ST_COUNT)
    {
      FEB_Console_Printf("Error: invalid state '%s' (use 0-%d or 'drive' or 'off')\r\n", val, (int)FEB_SM_ST_COUNT - 1);
      return;
    }
    state = (FEB_SM_ST_t)n;
  }

  if (!FEB_CAN_BMS_SetStateSim(true, state))
  {
    FEB_Console_Printf("Error: refused — BMS is active on CAN bus\r\n");
    return;
  }

  FEB_Console_Printf("BMS sim active: state=%d (%s)\r\n", (int)state,
                     state == FEB_SM_ST_DRIVE ? "DRIVE" : "see PCU|bms for name");
}

static void cmd_bms(int argc, char *argv[])
{
  if (argc >= 2 && FEB_strcasecmp(argv[1], "state") == 0)
  {
    cmd_bms_state(argc - 1, argv + 1);
    return;
  }

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

/* CSV handlers register as top-level descriptors named per spec. */

static void cmd_status_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  APPS_DataTypeDef apps_data;
  Brake_DataTypeDef brake_data;
  FEB_CAN_TPS_Data_t tps_data;
  FEB_ADC_GetAPPSData(&apps_data);
  FEB_ADC_GetBrakeData(&brake_data);
  FEB_CAN_TPS_GetData(&tps_data);
  FEB_SM_ST_t bms_state = FEB_CAN_BMS_getState();
  FEB_Console_CsvEmit("status", "%.1f,%d,%.1f,%d,%s,%u,%d", apps_data.acceleration, apps_data.plausible ? 1 : 0,
                      brake_data.brake_position, brake_data.brake_pressed ? 1 : 0, bms_state_name(bms_state),
                      tps_data.bus_voltage_mv, tps_data.current_ma);
}

static void cmd_apps_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  APPS_DataTypeDef apps_data;
  FEB_ADC_GetAPPSData(&apps_data);
  FEB_Console_CsvEmit("apps", "%d,%.3f,%.1f,%d,%.3f,%.1f,%.1f,%d", FEB_ADC_GetAccelPedal1Raw(),
                      FEB_ADC_GetAccelPedal1Voltage(), apps_data.position1, FEB_ADC_GetAccelPedal2Raw(),
                      FEB_ADC_GetAccelPedal2Voltage(), apps_data.position2, apps_data.acceleration,
                      apps_data.plausible ? 1 : 0);
}

static void cmd_brake_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  Brake_DataTypeDef brake_data;
  FEB_ADC_GetBrakeData(&brake_data);
  FEB_Console_CsvEmit("brake", "%d,%.3f,%.1f,%d,%.3f,%.1f,%.1f,%d", FEB_ADC_GetBrakePressure1Raw(),
                      FEB_ADC_GetBrakePressure1Voltage(), brake_data.pressure1_percent, FEB_ADC_GetBrakePressure2Raw(),
                      FEB_ADC_GetBrakePressure2Voltage(), brake_data.pressure2_percent, brake_data.brake_position,
                      brake_data.brake_pressed ? 1 : 0);
}

static void cmd_rms_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  /* fields: dc_v,speed,angle,cmd_nm,fb_nm,vsm_state,enable_lockout,fault_active,enable_state */
  FEB_Console_CsvEmit("rms", "%.1f,%d,%d,%.1f,%.1f,%u,%d,%d,%d", FEB_CAN_RMS_getDCBusVoltage(),
                      FEB_CAN_RMS_getMotorSpeed(), FEB_CAN_RMS_getMotorAngle(), FEB_CAN_RMS_getTorqueCommand(),
                      FEB_CAN_RMS_getTorqueFeedback(), FEB_CAN_RMS_getVsmState(),
                      FEB_CAN_RMS_getEnableLockout() ? 1 : 0, FEB_CAN_RMS_HasActiveFault() ? 1 : 0,
                      FEB_CAN_RMS_getEnableState() ? 1 : 0);
}

static void cmd_tps_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_CAN_TPS_Data_t tps_data;
  FEB_CAN_TPS_GetData(&tps_data);
  FEB_Console_CsvEmit("tps", "%u,%d,%ld", tps_data.bus_voltage_mv, tps_data.current_ma,
                      (long)tps_data.shunt_voltage_uv);
}

static void cmd_bms_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_SM_ST_t bms_state = FEB_CAN_BMS_getState();
  FEB_Console_CsvEmit("bms", "%s,%d,%.1f,%.1f", bms_state_name(bms_state), (int)bms_state,
                      FEB_CAN_BMS_getAccumulatorVoltage(), FEB_CAN_BMS_getMaxTemperature());
}

/* CAN TX/error diagnostics. `can` prints a summary; `can|reset` clears the
 * counters. Use this to confirm the bare-metal TX FIFO is absorbing bursts:
 * tx_queue_overflow should stay at 0 under load, and free mailboxes should
 * recover toward 3. The ESR decode (TEC/REC/LEC + EWGF/EPVF/BOFF) shows
 * whether the bus is healthy. */
static void cmd_can(int argc, char *argv[])
{
  if (argc >= 2 && FEB_strcasecmp(argv[1], "reset") == 0)
  {
    FEB_CAN_ResetErrorCounters();
    FEB_Console_Printf("CAN error counters reset\r\n");
    return;
  }

  uint32_t esr = FEB_CAN_GetLastErrorEsr();
  FEB_Console_Printf("=== CAN diagnostics ===\r\n");
  FEB_Console_Printf("  free mailboxes:    CAN1=%lu  CAN2=%lu\r\n",
                     (unsigned long)FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_INSTANCE_1),
                     (unsigned long)FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_INSTANCE_2));
  FEB_Console_Printf("  tx_queue_overflow: %lu  (frames dropped: software FIFO full)\r\n",
                     (unsigned long)FEB_CAN_GetTxQueueOverflowCount());
  FEB_Console_Printf("  hal_errors:        %lu\r\n", (unsigned long)FEB_CAN_GetHalErrorCount());
  FEB_Console_Printf("  error_callbacks:   %lu\r\n", (unsigned long)FEB_CAN_GetErrorCallbackCount());
  FEB_Console_Printf("  ewg/epv recover:   %lu\r\n", (unsigned long)FEB_CAN_GetEwgRecoveryCount());
  FEB_Console_Printf("  bus_off recover:   %lu\r\n", (unsigned long)FEB_CAN_GetBusOffCount());
  FEB_Console_Printf("  last ESR: 0x%08lX  TEC=%lu REC=%lu LEC=%lu%s%s%s\r\n", (unsigned long)esr,
                     (unsigned long)((esr >> 16) & 0xFFu), (unsigned long)((esr >> 24) & 0xFFu),
                     (unsigned long)((esr >> 4) & 0x7u), (esr & 0x1u) ? " EWGF" : "", (esr & 0x2u) ? " EPVF" : "",
                     (esr & 0x4u) ? " BOFF" : "");
}

static void cmd_can_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  /* fields: mb_free1,mb_free2,tx_overflow,hal_err,err_cb,ewg_recover,bus_off,esr */
  FEB_Console_CsvEmit("can", "%lu,%lu,%lu,%lu,%lu,%lu,%lu,0x%08lX",
                      (unsigned long)FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_INSTANCE_1),
                      (unsigned long)FEB_CAN_TX_GetFreeMailboxes(FEB_CAN_INSTANCE_2),
                      (unsigned long)FEB_CAN_GetTxQueueOverflowCount(), (unsigned long)FEB_CAN_GetHalErrorCount(),
                      (unsigned long)FEB_CAN_GetErrorCallbackCount(), (unsigned long)FEB_CAN_GetEwgRecoveryCount(),
                      (unsigned long)FEB_CAN_GetBusOffCount(), (unsigned long)FEB_CAN_GetLastErrorEsr());
}

/* ============================================================================
 * Command Descriptors
 *
 * One unified FEB_Console_Cmd_t per subcommand: .handler is the human text
 * impl, .csv_handler is the machine-readable CSV impl. Registered top-level
 * so `PCU|csv|<tx_id>|<sub>` resolves directly. Canonical text form:
 * `PCU|<sub>` via cmd_pcu mega-dispatcher.
 * ============================================================================ */
static const FEB_Console_Cmd_t pcu_status_cmd = {.name = "status",
                                                 .help = "PCU status summary",
                                                 .handler = cmd_status,
                                                 .csv_handler = cmd_status_csv,
                                                 .hidden = true};
static const FEB_Console_Cmd_t pcu_apps_cmd = {
    .name = "apps", .help = "APPS sensor data", .handler = cmd_apps, .csv_handler = cmd_apps_csv, .hidden = true};
static const FEB_Console_Cmd_t pcu_brake_cmd = {
    .name = "brake", .help = "Brake sensor data", .handler = cmd_brake, .csv_handler = cmd_brake_csv, .hidden = true};
static const FEB_Console_Cmd_t pcu_rms_cmd = {.name = "rms",
                                              .help = "RMS motor controller status",
                                              .handler = cmd_rms,
                                              .csv_handler = cmd_rms_csv,
                                              .hidden = true};
static const FEB_Console_Cmd_t pcu_tps_cmd = {
    .name = "tps", .help = "TPS power monitor", .handler = cmd_tps, .csv_handler = cmd_tps_csv, .hidden = true};
static const FEB_Console_Cmd_t pcu_bms_cmd = {
    .name = "bms", .help = "BMS-state mirror", .handler = cmd_bms, .csv_handler = cmd_bms_csv, .hidden = true};
static const FEB_Console_Cmd_t pcu_can_cmd = {.name = "can",
                                              .help = "CAN TX/error diagnostics; `can|reset` clears counters",
                                              .handler = cmd_can,
                                              .csv_handler = cmd_can_csv,
                                              .hidden = true};

static const FEB_Console_Cmd_t *const PCU_SUBCMDS[] = {
    &pcu_status_cmd, &pcu_apps_cmd, &pcu_brake_cmd, &pcu_rms_cmd, &pcu_tps_cmd, &pcu_bms_cmd, &pcu_can_cmd,
};
#define PCU_SUBCMDS_COUNT (sizeof(PCU_SUBCMDS) / sizeof(PCU_SUBCMDS[0]))

static void cmd_pcu(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_pcu_help();
    return;
  }
  const char *subcmd = argv[1];

  /* `PCU|apps|...` with deeper args is owned by the APPS module. The
   * bare `PCU|apps` falls through to cmd_apps below for the legacy
   * read-only summary. */
  if (argc > 2 && FEB_strcasecmp(subcmd, "apps") == 0)
  {
    PCU_APPS_HandleAppsSubcommand(argc - 1, argv + 1);
    return;
  }
  if (FEB_strcasecmp(subcmd, "faults") == 0)
  {
    PCU_APPS_HandleFaultsSubcommand(argc - 1, argv + 1);
    return;
  }

  for (size_t i = 0; i < PCU_SUBCMDS_COUNT; i++)
  {
    if (FEB_strcasecmp(PCU_SUBCMDS[i]->name, subcmd) == 0)
    {
      PCU_SUBCMDS[i]->handler(argc - 1, argv + 1);
      return;
    }
  }
  FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
  print_pcu_help();
}

const FEB_Console_Cmd_t pcu_cmd = {
    .name = "PCU",
    .help = "PCU commands (PCU|<sub>) - run PCU alone for full list",
    .handler = cmd_pcu,
    .csv_handler = NULL,
};

void PCU_RegisterCommands(void)
{
  FEB_Console_Register(&pcu_cmd);
  for (size_t i = 0; i < PCU_SUBCMDS_COUNT; i++)
  {
    FEB_Console_Register(PCU_SUBCMDS[i]);
  }
  PCU_APPS_RegisterCommands();
}
