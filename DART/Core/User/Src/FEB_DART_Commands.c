/**
 ******************************************************************************
 * @file           : FEB_DART_Commands.c
 * @brief          : DART Custom Console Command Implementations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_DART_Commands.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "FEB_CAN.h"
#include "FEB_Fan.h"
#include "feb_console.h"
#include "feb_string_utils.h"

extern uint16_t frequency[NUM_FANS];

/* ============================================================================
 * Parsing Helpers
 * ============================================================================ */

/**
 * Parse a fan identifier: "all" (case-insensitive) or a 1-based index in 1..NUM_FANS.
 *
 * @param str      Input token.
 * @param idx_out  On success when not "all", receives 0-based fan index.
 * @param all_out  On success, set true if the identifier was "all".
 * @returns true on valid identifier, false otherwise.
 */
static bool parse_fan(const char *str, int *idx_out, bool *all_out)
{
  if (FEB_strcasecmp(str, "all") == 0)
  {
    *all_out = true;
    *idx_out = -1;
    return true;
  }
  char *endptr;
  long val = strtol(str, &endptr, 10);
  if (*endptr != '\0' || val < 1 || val > (long)NUM_FANS)
  {
    return false;
  }
  *all_out = false;
  *idx_out = (int)(val - 1);
  return true;
}

static bool parse_percent(const char *str, int *pct_out)
{
  char *endptr;
  long val = strtol(str, &endptr, 10);
  if (*endptr != '\0' || val < 0 || val > 100)
  {
    return false;
  }
  *pct_out = (int)val;
  return true;
}

static uint32_t rpm_percent(uint16_t freq_hz)
{
  uint32_t rpm = (uint32_t)freq_hz * 30u;
  uint32_t pct = rpm * 100u / FAN_MAX_RPM;
  if (pct > 100u)
  {
    pct = 100u;
  }
  return pct;
}

/* ============================================================================
 * Help
 * ============================================================================ */

static void print_dart_help(void)
{
  FEB_Console_Printf("DART Commands (mode: %s):\r\n", FEB_Fan_IsManualOverride() ? "manual" : "auto");
  FEB_Console_Printf("  DART|status                       - summary (mode, PWM, tach, BMS)\r\n");
  FEB_Console_Printf("  DART|pwm|set|<1-5|all>|<0-100>    - set manual PWM duty (enters manual mode)\r\n");
  FEB_Console_Printf("  DART|pwm|get|<1-5|all>            - read commanded PWM duty\r\n");
  FEB_Console_Printf("  DART|auto                         - return to CAN-driven mode\r\n");
  FEB_Console_Printf("  DART|tach|<1-5|all>               - read tach (Hz, RPM, %% of max)\r\n");
  FEB_Console_Printf("  DART|temp                         - BMS max cell temp + staleness\r\n");
  FEB_Console_Printf("  DART|cans                         - CAN RX/TX diagnostics\r\n");
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("FAN_MAX_RPM=%u  PWM_COUNTER=%u\r\n", (unsigned)FAN_MAX_RPM, (unsigned)PWM_COUNTER);
}

/* ============================================================================
 * PWM Subcommand
 * ============================================================================ */

static void print_pwm_row(int fan_idx_0based)
{
  uint8_t pct = FEB_Fan_GetCommandedPercent((uint8_t)fan_idx_0based);
  uint32_t counts = FEB_Fan_GetCommandedCounts((uint8_t)fan_idx_0based);
  FEB_Console_Printf("fan%d: %3u%% (counts=%u)\r\n", fan_idx_0based + 1, (unsigned)pct, (unsigned)counts);
}

static void cmd_pwm_set(int argc, char *argv[])
{
  /* argv = ["set", <fan>, <pct>] */
  if (argc < 3)
  {
    FEB_Console_Printf("Usage: DART|pwm|set|<1-5|all>|<0-100>\r\n");
    return;
  }

  int fan_idx = -1;
  bool all = false;
  if (!parse_fan(argv[1], &fan_idx, &all))
  {
    FEB_Console_Printf("Error: fan must be 1-%u or 'all'\r\n", (unsigned)NUM_FANS);
    return;
  }

  int pct = 0;
  if (!parse_percent(argv[2], &pct))
  {
    FEB_Console_Printf("Error: percent must be 0-100\r\n");
    return;
  }

  if (all)
  {
    FEB_Fan_SetManualOverride(true, (uint8_t)pct);
    FEB_Console_Printf("fan all: %d%% (manual)\r\n", pct);
  }
  else
  {
    FEB_Fan_SetManualFan((uint8_t)fan_idx, (uint8_t)pct);
    FEB_Console_Printf("fan%d: %d%% (manual)\r\n", fan_idx + 1, pct);
  }
}

static void cmd_pwm_get(int argc, char *argv[])
{
  /* argv = ["get", <fan>] */
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: DART|pwm|get|<1-5|all>\r\n");
    return;
  }

  int fan_idx = -1;
  bool all = false;
  if (!parse_fan(argv[1], &fan_idx, &all))
  {
    FEB_Console_Printf("Error: fan must be 1-%u or 'all'\r\n", (unsigned)NUM_FANS);
    return;
  }

  FEB_Console_Printf("PWM (mode: %s):\r\n", FEB_Fan_IsManualOverride() ? "manual" : "auto");
  if (all)
  {
    for (int i = 0; i < (int)NUM_FANS; ++i)
    {
      print_pwm_row(i);
    }
  }
  else
  {
    print_pwm_row(fan_idx);
  }
}

static void sub_pwm(int argc, char *argv[])
{
  /* argv = ["pwm", <sub>, ...] */
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: DART|pwm|<set|get>|...\r\n");
    return;
  }

  const char *sub = argv[1];
  if (FEB_strcasecmp(sub, "set") == 0)
  {
    cmd_pwm_set(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(sub, "get") == 0)
  {
    cmd_pwm_get(argc - 1, argv + 1);
  }
  else
  {
    FEB_Console_Printf("Unknown pwm subcommand: %s\r\n", sub);
    FEB_Console_Printf("Usage: DART|pwm|<set|get>|...\r\n");
  }
}

/* ============================================================================
 * Tachometer Subcommand
 * ============================================================================ */

static void print_tach_row(int fan_idx_0based)
{
  uint16_t hz = frequency[fan_idx_0based];
  uint32_t rpm = (uint32_t)hz * 30u;
  uint32_t pct = rpm_percent(hz);
  FEB_Console_Printf("fan%d: %5u Hz  %6u rpm  %3u%%\r\n", fan_idx_0based + 1, (unsigned)hz, (unsigned)rpm,
                     (unsigned)pct);
}

static void sub_tach(int argc, char *argv[])
{
  /* argv = ["tach", <fan>] */
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: DART|tach|<1-5|all>\r\n");
    return;
  }

  int fan_idx = -1;
  bool all = false;
  if (!parse_fan(argv[1], &fan_idx, &all))
  {
    FEB_Console_Printf("Error: fan must be 1-%u or 'all'\r\n", (unsigned)NUM_FANS);
    return;
  }

  if (all)
  {
    for (int i = 0; i < (int)NUM_FANS; ++i)
    {
      print_tach_row(i);
    }
  }
  else
  {
    print_tach_row(fan_idx);
  }
}

/* ============================================================================
 * Other Subcommands
 * ============================================================================ */

static void sub_auto(void)
{
  FEB_Fan_SetManualOverride(false, 0);
  FEB_Console_Printf("fan auto (CAN-driven)\r\n");
}

static void sub_temp(void)
{
  int16_t t = FEB_Fan_GetLastMaxCellTemp();
  uint32_t age = FEB_Fan_GetStalenessMs();
  FEB_Console_Printf("max_cell_temp=%d staleness=%u ms watchdog=%s\r\n", (int)t, (unsigned)age,
                     (age > BMS_RX_TIMEOUT_MS) ? "TRIPPED" : "ok");
}

static void sub_cans(void)
{
  FEB_Console_Printf("rx=%u tx_timeout=%u tx_err=%u\r\n", (unsigned)FEB_CAN_GetRxCount(),
                     (unsigned)FEB_CAN_GetTxTimeoutCount(), (unsigned)FEB_CAN_GetTxHalErrorCount());
}

static void sub_status(void)
{
  FEB_Console_Printf("DART Status (mode: %s):\r\n", FEB_Fan_IsManualOverride() ? "manual" : "auto");
  FEB_Console_Printf("%-5s %6s %8s %6s %7s %5s\r\n", "Fan", "PWM%", "Counts", "Hz", "RPM", "%max");
  FEB_Console_Printf("----- ------ -------- ------ ------- -----\r\n");
  for (int i = 0; i < (int)NUM_FANS; ++i)
  {
    uint8_t pct = FEB_Fan_GetCommandedPercent((uint8_t)i);
    uint32_t counts = FEB_Fan_GetCommandedCounts((uint8_t)i);
    uint16_t hz = frequency[i];
    uint32_t rpm = (uint32_t)hz * 30u;
    uint32_t tpct = rpm_percent(hz);
    FEB_Console_Printf("fan%-2d %5u%% %8u %6u %7u %4u%%\r\n", i + 1, (unsigned)pct, (unsigned)counts, (unsigned)hz,
                       (unsigned)rpm, (unsigned)tpct);
  }
  sub_temp();
}

/* ============================================================================
 * Main Dispatcher
 * ============================================================================ */

static void cmd_dart(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_dart_help();
    return;
  }

  const char *subcmd = argv[1];

  if (FEB_strcasecmp(subcmd, "status") == 0)
  {
    sub_status();
  }
  else if (FEB_strcasecmp(subcmd, "pwm") == 0)
  {
    sub_pwm(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(subcmd, "auto") == 0)
  {
    sub_auto();
  }
  else if (FEB_strcasecmp(subcmd, "tach") == 0)
  {
    sub_tach(argc - 1, argv + 1);
  }
  else if (FEB_strcasecmp(subcmd, "temp") == 0)
  {
    sub_temp();
  }
  else if (FEB_strcasecmp(subcmd, "cans") == 0)
  {
    sub_cans();
  }
  else if (FEB_strcasecmp(subcmd, "help") == 0)
  {
    print_dart_help();
  }
  else
  {
    FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
    print_dart_help();
  }
}

/* ============================================================================
 * Command Descriptor & Registration
 * ============================================================================ */

static const FEB_Console_Cmd_t dart_cmd = {
    .name = "DART",
    .help = "DART board commands (DART|status, DART|pwm|set|all|50, DART|tach|all)",
    .handler = cmd_dart,
};

void DART_RegisterCommands(void)
{
  FEB_Console_Register(&dart_cmd);
}
