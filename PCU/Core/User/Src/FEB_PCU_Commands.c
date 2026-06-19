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
  FEB_Console_Printf("  PCU|rms      - Full RMS inverter telemetry (state/volts/current/temps/fw)\r\n");
  FEB_Console_Printf("  PCU|rms|raw  - Dump every inverter frame seen (raw bytes + age)\r\n");
  FEB_Console_Printf("  PCU|rms|<enable|disable>   - Manually enable/disable inverter (bench)\r\n");
  FEB_Console_Printf("  PCU|rms|clearfault         - Clear inverter (RMS) latched faults\r\n");
  FEB_Console_Printf("  PCU|rms|faults             - Decode active inverter faults by name\r\n");
  FEB_Console_Printf("  PCU|rms|eeprom|read|<id>           - Read any inverter parameter (dec or 0xHEX)\r\n");
  FEB_Console_Printf("  PCU|rms|eeprom|write|<id>|<value>  - Write a parameter (disable inverter first)\r\n");
  FEB_Console_Printf("  PCU|rms|eeprom|readall             - Read all documented EEPROM params\r\n");
  FEB_Console_Printf("  PCU|rms|eeprom|precharge|<on|off>  - Alias for param 140 (off = bypass precharge)\r\n");
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

/* Decode the M170 INV_Inverter_State enum (datasheet 0x0AA). */
static const char *inverter_state_name(uint8_t s)
{
  switch (s)
  {
  case 0:
    return "Power up";
  case 1:
    return "Stop";
  case 2:
    return "Open Loop";
  case 3:
    return "Closed Loop";
  case 8:
    return "Idle Run";
  case 9:
    return "Idle Stop";
  default:
    return "Internal";
  }
}

/* Decode the M170 INV_Inverter_Discharge_State (datasheet 0x0AA). */
static const char *discharge_state_name(uint8_t s)
{
  switch (s)
  {
  case 0:
    return "Disabled";
  case 1:
    return "Enabled";
  case 2:
    return "Speed Check";
  case 3:
    return "Active";
  case 4:
    return "Complete";
  default:
    return "?";
  }
}

/* Parse a decimal or 0xHEX integer token. Returns false on empty/garbage. */
static bool parse_long(const char *s, long *out)
{
  if (s == NULL || *s == '\0')
    return false;
  char *end = NULL;
  long v = strtol(s, &end, 0);
  if (end == s || *end != '\0')
    return false;
  *out = v;
  return true;
}

/* Walk the four M171 fault words, printing each active bit by datasheet name.
 * Returns the number of active fault bits found. */
static int print_active_faults(void)
{
  const uint16_t words[4] = {FEB_CAN_RMS_getPostFaultLo(), FEB_CAN_RMS_getPostFaultHi(), FEB_CAN_RMS_getRunFaultLo(),
                             FEB_CAN_RMS_getRunFaultHi()};
  static const char *const wname[4] = {"POSTlo", "POSThi", "RUNlo", "RUNhi"};
  int n = 0;
  for (int w = 0; w < 4; w++)
  {
    for (int b = 0; b < 16; b++)
    {
      if (words[w] & (1u << b))
      {
        const char *name = FEB_CAN_RMS_FaultName((uint8_t)(w * 16 + b));
        FEB_Console_Printf("    [%s b%-2d] %s\r\n", wname[w], b, name ? name : "(reserved)");
        n++;
      }
    }
  }
  return n;
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

#define RMS_PARAM_TIMEOUT_MS 60u

static void eeprom_print_one(uint16_t addr, int16_t value)
{
  const char *name = FEB_CAN_RMS_ParamName(addr);
  FEB_Console_Printf("  %3u (0x%03X)  %-30s = %6d  (0x%04X)\r\n", addr, addr, name ? name : "(undocumented)", value,
                     (uint16_t)value);
}

/* `PCU|rms|eeprom|...` — generic inverter parameter access over UART (M193/M194):
 *   read|<id>            read any parameter (id decimal or 0xHEX). Always safe.
 *   write|<id>|<value>   write a parameter (16-bit; decimal/0xHEX/signed). Guarded:
 *                        inverter must be disabled — EEPROM (100..499) is rejected
 *                        by the inverter while the motor runs, and writes persist.
 *   readall              read every documented EEPROM parameter (100..499).
 *   precharge|<on|off>   convenience alias for param 140 (off = bypass precharge).
 */
static void cmd_rms_eeprom(int argc, char *argv[])
{
  const char *sub = (argc >= 2) ? argv[1] : "";

  if (FEB_strcasecmp(sub, "read") == 0)
  {
    long id;
    if (argc < 3 || !parse_long(argv[2], &id) || id < 0 || id > 0xFFFF)
    {
      FEB_Console_Printf("Usage: PCU|rms|eeprom|read|<id>   (id decimal or 0xHEX)\r\n");
      return;
    }
    int16_t value;
    if (FEB_CAN_RMS_ReadParam((uint16_t)id, &value, RMS_PARAM_TIMEOUT_MS))
      eeprom_print_one((uint16_t)id, value);
    else
      FEB_Console_Printf("Param %ld read FAILED (no response / address unrecognized)\r\n", id);
    return;
  }

  if (FEB_strcasecmp(sub, "readall") == 0)
  {
    size_t n;
    const FEB_RMS_Param_t *tbl = FEB_CAN_RMS_ParamTable(&n);
    FEB_Console_Printf("=== RMS EEPROM Parameters (documented, 100..499) ===\r\n");
    int read = 0, miss = 0;
    for (size_t i = 0; i < n; i++)
    {
      if (tbl[i].addr < 100u || tbl[i].addr > 499u)
        continue; /* skip 0..99 command params */
      int16_t value;
      if (FEB_CAN_RMS_ReadParam(tbl[i].addr, &value, RMS_PARAM_TIMEOUT_MS))
      {
        eeprom_print_one(tbl[i].addr, value);
        read++;
      }
      else
      {
        FEB_Console_Printf("  %3u (0x%03X)  %-30s = (no response)\r\n", tbl[i].addr, tbl[i].addr, tbl[i].name);
        miss++;
      }
    }
    FEB_Console_Printf("Read %d params, %d no-response. (undocumented addresses: PCU|rms|eeprom|read|<id>)\r\n", read,
                       miss);
    return;
  }

  if (FEB_strcasecmp(sub, "write") == 0 || FEB_strcasecmp(sub, "precharge") == 0)
  {
    uint16_t addr;
    int16_t value;
    if (FEB_strcasecmp(sub, "precharge") == 0)
    {
      if (argc < 3 || (FEB_strcasecmp(argv[2], "on") != 0 && FEB_strcasecmp(argv[2], "off") != 0))
      {
        FEB_Console_Printf("Usage: PCU|rms|eeprom|precharge|<on|off>  (off = bypass inverter precharge)\r\n");
        return;
      }
      /* precharge on -> bypass 0 (normal); off -> bypass 1 (disabled). */
      addr = 140u;
      value = (FEB_strcasecmp(argv[2], "off") == 0) ? 1 : 0;
    }
    else
    {
      long id, v;
      if (argc < 4 || !parse_long(argv[2], &id) || id < 0 || id > 0xFFFF || !parse_long(argv[3], &v) || v < -32768 ||
          v > 0xFFFF)
      {
        FEB_Console_Printf("Usage: PCU|rms|eeprom|write|<id>|<value>   (decimal or 0xHEX)\r\n");
        return;
      }
      addr = (uint16_t)id;
      value = (int16_t)v;
    }

    /* Guard: EEPROM writes are persistent and the inverter only accepts them
     * with the motor disabled (datasheet p.38). Require PCU|rms|disable first. */
    if (RMS_CONTROL_MESSAGE.enabled)
    {
      FEB_Console_Printf("Refused: inverter is enabled. Run PCU|rms|disable first.\r\n");
      return;
    }
    if (addr < 100u || addr > 499u)
      FEB_Console_Printf("Note: address %u is outside EEPROM range (100..499) — a command/transient param.\r\n", addr);

    const char *name = FEB_CAN_RMS_ParamName(addr);
    FEB_Console_Printf("Writing param %u (0x%03X) %s = %d (0x%04X)...\r\n", addr, addr, name ? name : "(undocumented)",
                       value, (uint16_t)value);
    bool ok = false;
    if (FEB_CAN_RMS_WriteParam(addr, value, &ok, RMS_PARAM_TIMEOUT_MS))
      FEB_Console_Printf(ok ? "Inverter confirmed write OK\r\n" : "Inverter reported write FAILURE\r\n");
    else
      FEB_Console_Printf("No M194 response from inverter (is it powered / on the bus?)\r\n");
    return;
  }

  FEB_Console_Printf("Usage: PCU|rms|eeprom|<read|write|readall|precharge>\r\n");
  FEB_Console_Printf("  read|<id>            read any parameter (id decimal or 0xHEX)\r\n");
  FEB_Console_Printf("  write|<id>|<value>   write a parameter (inverter must be disabled)\r\n");
  FEB_Console_Printf("  readall              read all documented EEPROM params (100..499)\r\n");
  FEB_Console_Printf("  precharge|<on|off>   alias for param 140 (off = bypass precharge)\r\n");
}

/* `PCU|rms|faults` — decode the M171 fault words to named active faults. */
static void cmd_rms_faults(void)
{
  FEB_Console_Printf("=== RMS Faults (M171) ===\r\n");
  if (!FEB_CAN_RMS_FaultsSeen())
  {
    FEB_Console_Printf("  (no M171 broadcast — inverter silent on bus)\r\n");
    return;
  }
  FEB_Console_Printf("  POST lo=0x%04X hi=0x%04X   RUN lo=0x%04X hi=0x%04X\r\n", (unsigned)FEB_CAN_RMS_getPostFaultLo(),
                     (unsigned)FEB_CAN_RMS_getPostFaultHi(), (unsigned)FEB_CAN_RMS_getRunFaultLo(),
                     (unsigned)FEB_CAN_RMS_getRunFaultHi());
  if (!FEB_CAN_RMS_HasActiveFault())
  {
    FEB_Console_Printf("  Active faults: NONE\r\n");
    return;
  }
  FEB_Console_Printf("  Active faults:\r\n");
  print_active_faults();
  FEB_Console_Printf("  (clear with PCU|rms|clearfault once the condition is resolved)\r\n");
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
  if (argc >= 2 && FEB_strcasecmp(argv[1], "clearfault") == 0)
  {
    FEB_CAN_RMS_Transmit_ClearFaults();
    FEB_Console_Printf("Sent inverter fault-clear (M193 param 20). Re-check PCU|rms for fault state.\r\n");
    return;
  }
  if (argc >= 2 && FEB_strcasecmp(argv[1], "eeprom") == 0)
  {
    cmd_rms_eeprom(argc - 1, argv + 1);
    return;
  }
  if (argc >= 2 && FEB_strcasecmp(argv[1], "faults") == 0)
  {
    cmd_rms_faults();
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
    uint8_t istate = FEB_CAN_RMS_getInverterState();
    FEB_Console_Printf("  VSM State:       %u (%s)\r\n", vsm, vsm_state_name(vsm));
    FEB_Console_Printf("  Inverter State:  %u (%s)\r\n", istate, inverter_state_name(istate));
    FEB_Console_Printf("  Enable State:    %s\r\n", FEB_CAN_RMS_getEnableState() ? "ENABLED" : "DISABLED");
    FEB_Console_Printf("  Enable Lockout:  %s\r\n", FEB_CAN_RMS_getEnableLockout() ? "LOCKED" : "clear");
    FEB_Console_Printf("  Command Mode:    %s   Run Mode: %s\r\n", FEB_CAN_RMS_getCommandModeVsm() ? "VSM" : "CAN",
                       RMS_MESSAGE.run_mode ? "speed" : "torque");
    FEB_Console_Printf("  Direction:       %s   Discharge: %u (%s)\r\n",
                       RMS_MESSAGE.direction_command ? "forward" : "reverse/stopped", RMS_MESSAGE.discharge_state,
                       discharge_state_name(RMS_MESSAGE.discharge_state));
    FEB_Console_Printf("  Echoed Counter:  %u   PWM: %u kHz   Start Mode: %u\r\n", FEB_CAN_RMS_getEchoRollingCounter(),
                       RMS_MESSAGE.pwm_frequency, RMS_MESSAGE.start_mode_active);
    FEB_Console_Printf("  Relays 1-6:      %u%u%u%u%u%u\r\n", (RMS_MESSAGE.relay_status >> 0) & 1u,
                       (RMS_MESSAGE.relay_status >> 1) & 1u, (RMS_MESSAGE.relay_status >> 2) & 1u,
                       (RMS_MESSAGE.relay_status >> 3) & 1u, (RMS_MESSAGE.relay_status >> 4) & 1u,
                       (RMS_MESSAGE.relay_status >> 5) & 1u);
    FEB_Console_Printf("  BMS Active: %u   BMS Trq Limit: %u   Max-Spd Limit: %u   Low-Spd Limit: %u\r\n",
                       RMS_MESSAGE.bms_active, RMS_MESSAGE.bms_torque_limiting, RMS_MESSAGE.max_speed_limiting,
                       RMS_MESSAGE.low_speed_limiting);
  }
  FEB_Console_Printf("\r\n");

  /* Fault codes (M171), decoded to names from the datasheet. */
  FEB_Console_Printf("Faults (M171):\r\n");
  if (!FEB_CAN_RMS_FaultsSeen())
  {
    FEB_Console_Printf("  (no M171 broadcast)\r\n");
  }
  else
  {
    FEB_Console_Printf("  POST lo=0x%04X hi=0x%04X  RUN lo=0x%04X hi=0x%04X\r\n",
                       (unsigned)FEB_CAN_RMS_getPostFaultLo(), (unsigned)FEB_CAN_RMS_getPostFaultHi(),
                       (unsigned)FEB_CAN_RMS_getRunFaultLo(), (unsigned)FEB_CAN_RMS_getRunFaultHi());
    if (!FEB_CAN_RMS_HasActiveFault())
      FEB_Console_Printf("  Active: NONE\r\n");
    else
    {
      FEB_Console_Printf("  Active:\r\n");
      print_active_faults();
    }
  }
  FEB_Console_Printf("\r\n");

  /* Voltages (M167 bus/output + M169 internal references). */
  FEB_Console_Printf("Voltages:\r\n");
  FEB_Console_Printf("  DC Bus: %.1f V   Output: %.1f V   Vab/Vd: %.1f V   Vbc/Vq: %.1f V\r\n",
                     FEB_CAN_RMS_getDCBusVoltage(), RMS_MESSAGE.output_voltage / 10.0f,
                     RMS_MESSAGE.vab_vd_voltage / 10.0f, RMS_MESSAGE.vbc_voltage / 10.0f);
  if (RMS_MESSAGE.intv_rx_timestamp != 0)
    FEB_Console_Printf("  Internal: 1.5V=%.2f  2.5V=%.2f  5V=%.2f  12V=%.2f\r\n", RMS_MESSAGE.ref_voltage_1_5 / 100.0f,
                       RMS_MESSAGE.ref_voltage_2_5 / 100.0f, RMS_MESSAGE.ref_voltage_5_0 / 100.0f,
                       RMS_MESSAGE.ref_voltage_12_0 / 100.0f);
  FEB_Console_Printf("\r\n");

  /* Currents (M166 phase/DC + M168 Id/Iq). */
  FEB_Console_Printf("Currents:\r\n");
  if (RMS_MESSAGE.current_rx_timestamp != 0)
    FEB_Console_Printf("  Phase A %.1f  B %.1f  C %.1f A   DC Bus %.1f A\r\n", RMS_MESSAGE.phase_a_current / 10.0f,
                       RMS_MESSAGE.phase_b_current / 10.0f, RMS_MESSAGE.phase_c_current / 10.0f,
                       RMS_MESSAGE.dc_bus_current / 10.0f);
  if (RMS_MESSAGE.flux_rx_timestamp != 0)
    FEB_Console_Printf("  Id %.1f A   Iq %.1f A   Flux cmd %.3f Wb   fb %.3f Wb\r\n", RMS_MESSAGE.i_d / 10.0f,
                       RMS_MESSAGE.i_q / 10.0f, RMS_MESSAGE.flux_command / 1000.0f,
                       RMS_MESSAGE.flux_feedback / 1000.0f);
  FEB_Console_Printf("\r\n");

  /* Motor + modulation (M165 + M173). */
  FEB_Console_Printf("Motor:\r\n");
  FEB_Console_Printf("  Speed: %d RPM   Elec Freq: %.1f Hz   Angle: %.1f deg\r\n", FEB_CAN_RMS_getMotorSpeed(),
                     RMS_MESSAGE.electrical_freq / 10.0f, FEB_CAN_RMS_getMotorAngle() / 10.0f);
  FEB_Console_Printf("  Cmd Torque: %.1f Nm   Feedback: %.1f Nm   (inv echo %.1f Nm)\r\n",
                     FEB_CAN_RMS_getTorqueCommand(), FEB_CAN_RMS_getTorqueFeedback(),
                     RMS_MESSAGE.inv_commanded_torque / 10.0f);
  if (RMS_MESSAGE.mod_rx_timestamp != 0)
    FEB_Console_Printf("  Modulation Idx: %.2f   Flux-Weakening Out: %.1f A   Id/Iq cmd: %.1f/%.1f A\r\n",
                       RMS_MESSAGE.modulation_index / 100.0f, RMS_MESSAGE.flux_weakening_output / 10.0f,
                       RMS_MESSAGE.id_command / 10.0f, RMS_MESSAGE.iq_command / 10.0f);
  if (RMS_MESSAGE.torque_timer_rx_timestamp != 0)
    FEB_Console_Printf("  Power-on Timer: %.1f s\r\n", RMS_MESSAGE.power_on_timer * 0.003f);
  FEB_Console_Printf("\r\n");

  /* Temperatures (M160..M162). */
  if (RMS_MESSAGE.temps_rx_timestamp != 0)
  {
    FEB_Console_Printf("Temps (C):\r\n");
    FEB_Console_Printf("  Module A %.1f  B %.1f  C %.1f   Gate %.1f   Control %.1f\r\n",
                       RMS_MESSAGE.temp_module_a / 10.0f, RMS_MESSAGE.temp_module_b / 10.0f,
                       RMS_MESSAGE.temp_module_c / 10.0f, RMS_MESSAGE.temp_gate_driver / 10.0f,
                       RMS_MESSAGE.temp_control_board / 10.0f);
    FEB_Console_Printf("  Motor %.1f   RTD1 %.1f  RTD2 %.1f  RTD3 %.1f  RTD4 %.1f  RTD5 %.1f\r\n",
                       RMS_MESSAGE.temp_motor / 10.0f, RMS_MESSAGE.temp_rtd1 / 10.0f, RMS_MESSAGE.temp_rtd2 / 10.0f,
                       RMS_MESSAGE.temp_rtd3 / 10.0f, RMS_MESSAGE.temp_rtd4 / 10.0f, RMS_MESSAGE.temp_rtd5 / 10.0f);
    FEB_Console_Printf("\r\n");
  }

  /* Digital / analog I/O (M163 / M164). */
  if (RMS_MESSAGE.digital_rx_timestamp != 0 || RMS_MESSAGE.analog_rx_timestamp != 0)
  {
    FEB_Console_Printf("I/O:\r\n");
    if (RMS_MESSAGE.digital_rx_timestamp != 0)
      FEB_Console_Printf("  Digital In 1-8: %u%u%u%u%u%u%u%u\r\n", (RMS_MESSAGE.digital_in >> 0) & 1u,
                         (RMS_MESSAGE.digital_in >> 1) & 1u, (RMS_MESSAGE.digital_in >> 2) & 1u,
                         (RMS_MESSAGE.digital_in >> 3) & 1u, (RMS_MESSAGE.digital_in >> 4) & 1u,
                         (RMS_MESSAGE.digital_in >> 5) & 1u, (RMS_MESSAGE.digital_in >> 6) & 1u,
                         (RMS_MESSAGE.digital_in >> 7) & 1u);
    if (RMS_MESSAGE.analog_rx_timestamp != 0)
      FEB_Console_Printf("  Analog In 1-6:  %.2f %.2f %.2f %.2f %.2f %.2f V\r\n", RMS_MESSAGE.analog_in[0] / 100.0f,
                         RMS_MESSAGE.analog_in[1] / 100.0f, RMS_MESSAGE.analog_in[2] / 100.0f,
                         RMS_MESSAGE.analog_in[3] / 100.0f, RMS_MESSAGE.analog_in[4] / 100.0f,
                         RMS_MESSAGE.analog_in[5] / 100.0f);
    FEB_Console_Printf("\r\n");
  }

  /* Firmware info (M174). */
  if (RMS_MESSAGE.fw_rx_timestamp != 0)
    FEB_Console_Printf("Firmware: SW v%u   EEPROM/Project v%u   Date %04u-%04u\r\n\r\n", RMS_MESSAGE.fw_sw_version,
                       RMS_MESSAGE.fw_eeprom_version, RMS_MESSAGE.fw_date_yyyy, RMS_MESSAGE.fw_date_mmdd);

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
  const char *sub = (argc >= 2) ? argv[1] : "";

  if (FEB_strcasecmp(sub, "clearfault") == 0)
  {
    FEB_CAN_RMS_Transmit_ClearFaults();
    FEB_Console_CsvEmit("clearfault", "1");
    return;
  }

  if (FEB_strcasecmp(sub, "faults") == 0)
  {
    /* summary: postlo,posthi,runlo,runhi,active — then one `fault,bit,name` row each. */
    FEB_Console_CsvEmit("faults", "0x%04X,0x%04X,0x%04X,0x%04X,%d", (unsigned)FEB_CAN_RMS_getPostFaultLo(),
                        (unsigned)FEB_CAN_RMS_getPostFaultHi(), (unsigned)FEB_CAN_RMS_getRunFaultLo(),
                        (unsigned)FEB_CAN_RMS_getRunFaultHi(), FEB_CAN_RMS_HasActiveFault() ? 1 : 0);
    const uint16_t words[4] = {FEB_CAN_RMS_getPostFaultLo(), FEB_CAN_RMS_getPostFaultHi(), FEB_CAN_RMS_getRunFaultLo(),
                               FEB_CAN_RMS_getRunFaultHi()};
    for (int w = 0; w < 4; w++)
      for (int b = 0; b < 16; b++)
        if (words[w] & (1u << b))
        {
          const char *name = FEB_CAN_RMS_FaultName((uint8_t)(w * 16 + b));
          FEB_Console_CsvEmit("fault", "%d,%s", w * 16 + b, name ? name : "reserved");
        }
    return;
  }

  if (FEB_strcasecmp(sub, "eeprom") == 0)
  {
    const char *op = (argc >= 3) ? argv[2] : "";
    if (FEB_strcasecmp(op, "read") == 0)
    {
      long id;
      if (argc < 4 || !parse_long(argv[3], &id) || id < 0 || id > 0xFFFF)
      {
        FEB_Console_CsvEmit("error", "usage,read|<id>");
        return;
      }
      int16_t value = 0;
      bool ok = FEB_CAN_RMS_ReadParam((uint16_t)id, &value, RMS_PARAM_TIMEOUT_MS);
      FEB_Console_CsvEmit("eeprom", "%ld,%d,%d", id, ok ? 1 : 0, ok ? value : 0);
      return;
    }
    if (FEB_strcasecmp(op, "readall") == 0)
    {
      size_t n;
      const FEB_RMS_Param_t *tbl = FEB_CAN_RMS_ParamTable(&n);
      for (size_t i = 0; i < n; i++)
      {
        if (tbl[i].addr < 100u || tbl[i].addr > 499u)
          continue;
        int16_t value = 0;
        bool ok = FEB_CAN_RMS_ReadParam(tbl[i].addr, &value, RMS_PARAM_TIMEOUT_MS);
        FEB_Console_CsvEmit("eeprom", "%u,%d,%d", (unsigned)tbl[i].addr, ok ? 1 : 0, ok ? value : 0);
      }
      return;
    }
    if (FEB_strcasecmp(op, "write") == 0)
    {
      long id, v;
      if (argc < 5 || !parse_long(argv[3], &id) || id < 0 || id > 0xFFFF || !parse_long(argv[4], &v) || v < -32768 ||
          v > 0xFFFF)
      {
        FEB_Console_CsvEmit("error", "usage,write|<id>|<value>");
        return;
      }
      if (RMS_CONTROL_MESSAGE.enabled)
      {
        FEB_Console_CsvEmit("error", "inverter_enabled");
        return;
      }
      bool ok = false;
      bool resp = FEB_CAN_RMS_WriteParam((uint16_t)id, (int16_t)v, &ok, RMS_PARAM_TIMEOUT_MS);
      FEB_Console_CsvEmit("eeprom_write", "%ld,%d", id, (resp && ok) ? 1 : 0);
      return;
    }
    FEB_Console_CsvEmit("error", "usage,eeprom|<read|write|readall>");
    return;
  }

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
