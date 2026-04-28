/**
 ******************************************************************************
 * @file           : FEB_PCU_APPS_Commands.c
 * @brief          : APPS / faults debug console subcommands for PCU
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Pipe-delimited subcommand tree exposed through the existing feb_console.
 * Modeled on LVPDB/Core/User/Src/FEB_LVPDB_Commands.c. Dispatch:
 *   PCU|apps|<sub>      -> handled here via PCU_APPS_HandleAppsSubcommand
 *   PCU|faults|<sub>    -> handled here via PCU_APPS_HandleFaultsSubcommand
 *   PCU|csv|<tx>|apps   -> top-level "apps" command's csv_handler
 *   PCU|csv|<tx>|faults -> top-level "faults" command's csv_handler
 *
 * Safety-relevant overrides (sim, fault inject/clear, single-mode) are
 * gated by FEB_ADC_* helpers that refuse the operation when the BMS is in
 * drive state; this file only surfaces the user-facing errors.
 ******************************************************************************
 */

#include "FEB_PCU_APPS_Commands.h"
#include "FEB_ADC.h"
#include "feb_console.h"
#include "feb_string_utils.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Streaming state
 * ============================================================================ */

#define APPS_STREAM_TX_ID_LEN 32
#define APPS_STREAM_INFINITE 0u

static struct
{
  bool active;
  uint32_t period_ms;
  uint32_t remaining; /* APPS_STREAM_INFINITE = run forever until stop */
  uint32_t next_tick;
  bool csv;
  char tx_id[APPS_STREAM_TX_ID_LEN];
} apps_stream = {0};

/* ============================================================================
 * Argument parsing helpers
 * ============================================================================ */

static bool parse_uint(const char *s, uint32_t *out)
{
  if (!s || !*s)
    return false;
  char *end = NULL;
  unsigned long v = strtoul(s, &end, 0);
  if (!end || *end != '\0')
    return false;
  *out = (uint32_t)v;
  return true;
}

static bool parse_float(const char *s, float *out)
{
  if (!s || !*s)
    return false;
  char *end = NULL;
  float v = strtof(s, &end);
  if (!end || *end != '\0')
    return false;
  *out = v;
  return true;
}

static bool parse_on_off(const char *s, bool *out)
{
  if (!s)
    return false;
  if (FEB_strcasecmp(s, "on") == 0 || FEB_strcasecmp(s, "1") == 0 || FEB_strcasecmp(s, "true") == 0 ||
      FEB_strcasecmp(s, "enable") == 0)
  {
    *out = true;
    return true;
  }
  if (FEB_strcasecmp(s, "off") == 0 || FEB_strcasecmp(s, "0") == 0 || FEB_strcasecmp(s, "false") == 0 ||
      FEB_strcasecmp(s, "disable") == 0)
  {
    *out = false;
    return true;
  }
  return false;
}

/* ============================================================================
 * APPS sub-handlers (text mode)
 * ============================================================================ */

static void print_apps_help(void)
{
  FEB_Console_Printf("APPS subcommands:\r\n");
  FEB_Console_Printf("  PCU|apps|raw                          - cache snapshot (raw, mV, %%)\r\n");
  FEB_Console_Printf("  PCU|apps|stream|<period_ms>|<count>   - 0 count = until stopped\r\n");
  FEB_Console_Printf("  PCU|apps|stream|stop                  - stop streaming\r\n");
  FEB_Console_Printf("  PCU|apps|stats [|reset]               - running min/max/avg\r\n");
  FEB_Console_Printf("  PCU|apps|cal                          - show calibration\r\n");
  FEB_Console_Printf("  PCU|apps|cal|<1|2>|<min_mv>|<max_mv>  - set range\r\n");
  FEB_Console_Printf("  PCU|apps|cal|capture|<1|2>|<min|max>  - capture current voltage\r\n");
  FEB_Console_Printf("  PCU|apps|cal|reset                    - restore defaults\r\n");
  FEB_Console_Printf("  PCU|apps|filter [|<on|off>|samples|<n>|alpha|<f>]\r\n");
  FEB_Console_Printf("  PCU|apps|deadzone|<percent>           - 0..20%%\r\n");
  FEB_Console_Printf("  PCU|apps|mode|single|<on|off>         - bench-mode bypass (drive locked)\r\n");
  FEB_Console_Printf("  PCU|apps|sim|<percent>|sim|off        - simulate APPS, 30s window\r\n");
}

static void apps_print_raw(void)
{
  APPS_DataTypeDef apps;
  uint16_t r1, r2;
  float v1, v2, dev;
  uint32_t faults, elapsed;
  FEB_ADC_GetAPPSCacheSnapshot(&apps, &r1, &r2, &v1, &v2, &faults, &elapsed, &dev);
  FEB_Console_Printf("APPS raw:\r\n");
  FEB_Console_Printf("  raw1=%u  voltage1=%.0fmV  position1=%.2f%%\r\n", r1, (double)v1, (double)apps.position1);
  FEB_Console_Printf("  raw2=%u  voltage2=%.0fmV  position2=%.2f%%\r\n", r2, (double)v2, (double)apps.position2);
  FEB_Console_Printf("  acceleration=%.2f%%  deviation=%.2f%%\r\n", (double)apps.acceleration, (double)dev);
  FEB_Console_Printf("  plausible=%d  short=%d  open=%d\r\n", apps.plausible, apps.short_circuit, apps.open_circuit);
  FEB_Console_Printf("  implaus_elapsed=%lums  faults=0x%08lX\r\n", (unsigned long)elapsed, (unsigned long)faults);
}

static void apps_emit_raw_csv(const char *tx_id_or_null)
{
  APPS_DataTypeDef apps;
  uint16_t r1, r2;
  float v1, v2, dev;
  uint32_t faults, elapsed;
  FEB_ADC_GetAPPSCacheSnapshot(&apps, &r1, &r2, &v1, &v2, &faults, &elapsed, &dev);
  /* fields: raw1,v1_mv,p1,raw2,v2_mv,p2,accel,dev,plausible,short,open,implaus_ms,faults */
  if (tx_id_or_null)
  {
    FEB_Console_CsvEmitAs(tx_id_or_null, "apps_raw", "%u,%.0f,%.2f,%u,%.0f,%.2f,%.2f,%.2f,%d,%d,%d,%lu,0x%08lX", r1,
                          (double)v1, (double)apps.position1, r2, (double)v2, (double)apps.position2,
                          (double)apps.acceleration, (double)dev, apps.plausible ? 1 : 0, apps.short_circuit ? 1 : 0,
                          apps.open_circuit ? 1 : 0, (unsigned long)elapsed, (unsigned long)faults);
  }
  else
  {
    FEB_Console_CsvEmit("apps_raw", "%u,%.0f,%.2f,%u,%.0f,%.2f,%.2f,%.2f,%d,%d,%d,%lu,0x%08lX", r1, (double)v1,
                        (double)apps.position1, r2, (double)v2, (double)apps.position2, (double)apps.acceleration,
                        (double)dev, apps.plausible ? 1 : 0, apps.short_circuit ? 1 : 0, apps.open_circuit ? 1 : 0,
                        (unsigned long)elapsed, (unsigned long)faults);
  }
}

static void cmd_raw(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  apps_print_raw();
}

static void cmd_stream(int argc, char *argv[])
{
  /* argv[0] == "stream"; argv[1] is "stop" OR period; argv[2] is count */
  if (argc < 2)
  {
    if (apps_stream.active)
    {
      FEB_Console_Printf("Stream active: period=%lums remaining=%lu csv=%d\r\n", (unsigned long)apps_stream.period_ms,
                         (unsigned long)apps_stream.remaining, apps_stream.csv);
    }
    else
    {
      FEB_Console_Printf("Stream inactive\r\n");
      FEB_Console_Printf("Usage: PCU|apps|stream|<period_ms>|<count>  (count=0 -> until stopped)\r\n");
    }
    return;
  }
  if (FEB_strcasecmp(argv[1], "stop") == 0)
  {
    apps_stream.active = false;
    FEB_Console_Printf("Stream stopped\r\n");
    return;
  }
  uint32_t period = 0, count = 0;
  if (!parse_uint(argv[1], &period) || period == 0)
  {
    FEB_Console_Printf("Error: invalid period '%s'\r\n", argv[1]);
    return;
  }
  if (argc >= 3)
  {
    if (!parse_uint(argv[2], &count))
    {
      FEB_Console_Printf("Error: invalid count '%s'\r\n", argv[2]);
      return;
    }
  }
  apps_stream.active = true;
  apps_stream.period_ms = period;
  apps_stream.remaining = (count == 0) ? APPS_STREAM_INFINITE : count;
  apps_stream.next_tick = HAL_GetTick();
  apps_stream.csv = false;
  apps_stream.tx_id[0] = '\0';
  FEB_Console_Printf("Streaming every %lums (count=%s)\r\n", (unsigned long)period, (count == 0) ? "inf" : argv[2]);
}

static void cmd_stats(int argc, char *argv[])
{
  if (argc >= 2 && FEB_strcasecmp(argv[1], "reset") == 0)
  {
    FEB_ADC_ResetAPPSStats();
    FEB_Console_Printf("APPS stats reset\r\n");
    return;
  }
  float p1_min, p1_max, p2_min, p2_max, p1_avg, p2_avg, dev_max;
  uint32_t samples;
  FEB_ADC_GetAPPSStats(&p1_min, &p1_max, &p2_min, &p2_max, &p1_avg, &p2_avg, &dev_max, &samples);
  FEB_Console_Printf("APPS stats over %lu samples:\r\n", (unsigned long)samples);
  FEB_Console_Printf("  P1: min=%.2f%%  avg=%.2f%%  max=%.2f%%\r\n", (double)p1_min, (double)p1_avg, (double)p1_max);
  FEB_Console_Printf("  P2: min=%.2f%%  avg=%.2f%%  max=%.2f%%\r\n", (double)p2_min, (double)p2_avg, (double)p2_max);
  FEB_Console_Printf("  Max deviation observed: %.2f%%\r\n", (double)dev_max);
}

static void cmd_cal(int argc, char *argv[])
{
  if (argc < 2)
  {
    float p1_min, p1_max, p2_min, p2_max;
    FEB_ADC_GetAPPSCalibration(1, &p1_min, &p1_max);
    FEB_ADC_GetAPPSCalibration(2, &p2_min, &p2_max);
    FEB_Console_Printf("APPS calibration:\r\n");
    FEB_Console_Printf("  APPS1: min=%.0fmV max=%.0fmV\r\n", (double)p1_min, (double)p1_max);
    FEB_Console_Printf("  APPS2: min=%.0fmV max=%.0fmV\r\n", (double)p2_min, (double)p2_max);
    return;
  }
  if (FEB_strcasecmp(argv[1], "reset") == 0)
  {
    FEB_ADC_ResetCalibrationToDefaults();
    FEB_Console_Printf("Calibration reset to defaults\r\n");
    return;
  }
  if (FEB_strcasecmp(argv[1], "capture") == 0)
  {
    if (argc < 4)
    {
      FEB_Console_Printf("Usage: PCU|apps|cal|capture|<1|2>|<min|max>\r\n");
      return;
    }
    uint32_t sensor;
    if (!parse_uint(argv[2], &sensor) || (sensor != 1 && sensor != 2))
    {
      FEB_Console_Printf("Error: sensor must be 1 or 2\r\n");
      return;
    }
    bool capture_max;
    if (FEB_strcasecmp(argv[3], "max") == 0)
      capture_max = true;
    else if (FEB_strcasecmp(argv[3], "min") == 0)
      capture_max = false;
    else
    {
      FEB_Console_Printf("Error: third arg must be 'min' or 'max'\r\n");
      return;
    }
    if (FEB_ADC_CaptureAPPSCalibration((uint8_t)sensor, capture_max) != ADC_STATUS_OK)
    {
      FEB_Console_Printf("Error: capture failed\r\n");
      return;
    }
    FEB_Console_Printf("Captured APPS%lu %s\r\n", (unsigned long)sensor, capture_max ? "max" : "min");
    return;
  }
  /* Explicit numeric form: cal|<1|2>|<min_mv>|<max_mv> */
  if (argc < 4)
  {
    FEB_Console_Printf("Usage: PCU|apps|cal|<1|2>|<min_mv>|<max_mv>\r\n");
    return;
  }
  uint32_t sensor;
  if (!parse_uint(argv[1], &sensor) || (sensor != 1 && sensor != 2))
  {
    FEB_Console_Printf("Error: sensor must be 1 or 2\r\n");
    return;
  }
  float min_mv, max_mv;
  if (!parse_float(argv[2], &min_mv) || !parse_float(argv[3], &max_mv))
  {
    FEB_Console_Printf("Error: invalid mV value\r\n");
    return;
  }
  if (FEB_ADC_SetAPPSVoltageRange((uint8_t)sensor, min_mv, max_mv) != ADC_STATUS_OK)
  {
    FEB_Console_Printf("Error: set range failed\r\n");
    return;
  }
  FEB_Console_Printf("APPS%lu calibration: min=%.0fmV max=%.0fmV\r\n", (unsigned long)sensor, (double)min_mv,
                     (double)max_mv);
}

static void cmd_filter(int argc, char *argv[])
{
  if (argc < 2)
  {
    bool enabled;
    uint8_t samples;
    float alpha;
    FEB_ADC_GetAPPSFilterConfig(&enabled, &samples, &alpha);
    FEB_Console_Printf("APPS filter: %s, samples=%u, alpha=%.3f\r\n", enabled ? "on" : "off", samples, (double)alpha);
    return;
  }
  bool enabled;
  uint8_t samples;
  float alpha;
  FEB_ADC_GetAPPSFilterConfig(&enabled, &samples, &alpha);

  if (FEB_strcasecmp(argv[1], "samples") == 0)
  {
    if (argc < 3)
    {
      FEB_Console_Printf("Usage: PCU|apps|filter|samples|<1..16>\r\n");
      return;
    }
    uint32_t n;
    if (!parse_uint(argv[2], &n))
    {
      FEB_Console_Printf("Error: invalid sample count\r\n");
      return;
    }
    FEB_ADC_SetAPPSFilter(enabled, (uint8_t)n, alpha);
    FEB_ADC_GetAPPSFilterConfig(&enabled, &samples, &alpha);
    FEB_Console_Printf("Filter samples=%u\r\n", samples);
    return;
  }
  if (FEB_strcasecmp(argv[1], "alpha") == 0)
  {
    if (argc < 3)
    {
      FEB_Console_Printf("Usage: PCU|apps|filter|alpha|<0..1>\r\n");
      return;
    }
    float a;
    if (!parse_float(argv[2], &a))
    {
      FEB_Console_Printf("Error: invalid alpha\r\n");
      return;
    }
    FEB_ADC_SetAPPSFilter(enabled, samples, a);
    FEB_ADC_GetAPPSFilterConfig(&enabled, &samples, &alpha);
    FEB_Console_Printf("Filter alpha=%.3f\r\n", (double)alpha);
    return;
  }
  bool flag;
  if (parse_on_off(argv[1], &flag))
  {
    FEB_ADC_SetAPPSFilter(flag, samples, alpha);
    FEB_Console_Printf("Filter %s\r\n", flag ? "ON" : "OFF");
    return;
  }
  FEB_Console_Printf("Usage: PCU|apps|filter [|on|off|samples|<n>|alpha|<f>]\r\n");
}

static void cmd_deadzone(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Deadzone: %.2f%%\r\n", (double)FEB_ADC_GetAPPSDeadzone());
    return;
  }
  float pct;
  if (!parse_float(argv[1], &pct))
  {
    FEB_Console_Printf("Error: invalid percent\r\n");
    return;
  }
  FEB_ADC_SetAPPSDeadzone(pct);
  FEB_Console_Printf("Deadzone set to %.2f%%\r\n", (double)FEB_ADC_GetAPPSDeadzone());
}

static void cmd_mode(int argc, char *argv[])
{
  if (argc < 3 || FEB_strcasecmp(argv[1], "single") != 0)
  {
    FEB_Console_Printf("Usage: PCU|apps|mode|single|<on|off>\r\n");
    return;
  }
  bool flag;
  if (!parse_on_off(argv[2], &flag))
  {
    FEB_Console_Printf("Error: expected on or off\r\n");
    return;
  }
  if (FEB_ADC_SetSingleSensorMode(flag) != ADC_STATUS_OK)
  {
    FEB_Console_Printf("Error: refused (drive state active)\r\n");
    return;
  }
  FEB_Console_Printf("Single APPS mode: %s\r\n", flag ? "ON" : "OFF");
}

static void cmd_sim(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_Printf("Usage: PCU|apps|sim|<percent>  (or sim|off)\r\n");
    return;
  }
  if (FEB_strcasecmp(argv[1], "off") == 0)
  {
    FEB_ADC_SetAPPSSimulation(false, 0.0f);
    FEB_Console_Printf("Sim cleared\r\n");
    return;
  }
  float pct;
  if (!parse_float(argv[1], &pct))
  {
    FEB_Console_Printf("Error: invalid percent\r\n");
    return;
  }
  if (FEB_ADC_SetAPPSSimulation(true, pct) != ADC_STATUS_OK)
  {
    FEB_Console_Printf("Error: refused (drive state active)\r\n");
    return;
  }
  FEB_Console_Printf("Sim active at %.2f%% for 30s\r\n", (double)pct);
}

/* APPS sub-dispatcher used by FEB_PCU_Commands.c text mode. */
void PCU_APPS_HandleAppsSubcommand(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_apps_help();
    return;
  }
  const char *sub = argv[1];
  if (FEB_strcasecmp(sub, "raw") == 0)
    cmd_raw(argc - 1, argv + 1);
  else if (FEB_strcasecmp(sub, "stream") == 0)
    cmd_stream(argc - 1, argv + 1);
  else if (FEB_strcasecmp(sub, "stats") == 0)
    cmd_stats(argc - 1, argv + 1);
  else if (FEB_strcasecmp(sub, "cal") == 0)
    cmd_cal(argc - 1, argv + 1);
  else if (FEB_strcasecmp(sub, "filter") == 0)
    cmd_filter(argc - 1, argv + 1);
  else if (FEB_strcasecmp(sub, "deadzone") == 0)
    cmd_deadzone(argc - 1, argv + 1);
  else if (FEB_strcasecmp(sub, "mode") == 0)
    cmd_mode(argc - 1, argv + 1);
  else if (FEB_strcasecmp(sub, "sim") == 0)
    cmd_sim(argc - 1, argv + 1);
  else if (FEB_strcasecmp(sub, "help") == 0)
    print_apps_help();
  else
  {
    FEB_Console_Printf("Unknown apps subcommand: %s\r\n", sub);
    print_apps_help();
  }
}

/* ============================================================================
 * Faults sub-dispatcher
 * ============================================================================ */

static void faults_print(void)
{
  uint32_t active = FEB_ADC_GetActiveFaults();
  FEB_Console_Printf("Active faults: 0x%08lX\r\n", (unsigned long)active);
  static const uint32_t bits[] = {
      (1u << 0), (1u << 1), (1u << 2), (1u << 3), (1u << 4), (1u << 5), (1u << 6), (1u << 7),
  };
  for (size_t i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
  {
    const char *name = FEB_ADC_FaultBitName(bits[i]);
    uint32_t hits = FEB_ADC_GetFaultHitCount(bits[i]);
    bool set = (active & bits[i]) != 0;
    FEB_Console_Printf("  %-22s %s  hits=%lu\r\n", name, set ? "ACTIVE" : "ok    ", (unsigned long)hits);
  }
}

void PCU_APPS_HandleFaultsSubcommand(int argc, char *argv[])
{
  if (argc < 2)
  {
    faults_print();
    return;
  }
  const char *sub = argv[1];
  if (FEB_strcasecmp(sub, "clear") == 0)
  {
    const char *name = (argc >= 3) ? argv[2] : "all";
    if (FEB_ADC_ClearFaultsByName(name) != ADC_STATUS_OK)
    {
      FEB_Console_Printf("Error: clear refused (drive state) or unknown fault '%s'\r\n", name);
      return;
    }
    FEB_Console_Printf("Cleared: %s\r\n", name);
    return;
  }
  if (FEB_strcasecmp(sub, "inject") == 0)
  {
    if (argc < 3)
    {
      FEB_Console_Printf("Usage: PCU|faults|inject|<NAME>\r\n");
      return;
    }
    uint32_t bit = FEB_ADC_FaultBitFromName(argv[2]);
    if (bit == 0)
    {
      FEB_Console_Printf("Error: unknown fault '%s'\r\n", argv[2]);
      return;
    }
    if (FEB_ADC_InjectFault(bit) != ADC_STATUS_OK)
    {
      FEB_Console_Printf("Error: refused (drive state active)\r\n");
      return;
    }
    FEB_Console_Printf("Injected: %s\r\n", FEB_ADC_FaultBitName(bit));
    return;
  }
  FEB_Console_Printf("Unknown faults subcommand: %s\r\n", sub);
  FEB_Console_Printf("Usage: PCU|faults [|clear [|<name>] | inject|<name>]\r\n");
}

/* ============================================================================
 * CSV handlers (top-level "apps" and "faults")
 * ============================================================================ */

static void csv_apps(int argc, char *argv[])
{
  /* In CSV mode the top-level command is "apps". argv[0]="apps".
   * argv[1+] carry any subcommand args, e.g. apps|raw. */
  const char *sub = (argc >= 2) ? argv[1] : "raw";
  if (FEB_strcasecmp(sub, "raw") == 0 || (argc < 2))
  {
    apps_emit_raw_csv(NULL);
    return;
  }
  if (FEB_strcasecmp(sub, "stats") == 0)
  {
    if (argc >= 3 && FEB_strcasecmp(argv[2], "reset") == 0)
    {
      FEB_ADC_ResetAPPSStats();
      FEB_Console_CsvEmit("apps_stats_reset", "ok");
      return;
    }
    float p1_min, p1_max, p2_min, p2_max, p1_avg, p2_avg, dev_max;
    uint32_t samples;
    FEB_ADC_GetAPPSStats(&p1_min, &p1_max, &p2_min, &p2_max, &p1_avg, &p2_avg, &dev_max, &samples);
    FEB_Console_CsvEmit("apps_stats", "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f", (unsigned long)samples, (double)p1_min,
                        (double)p1_avg, (double)p1_max, (double)p2_min, (double)p2_avg, (double)p2_max,
                        (double)dev_max);
    return;
  }
  if (FEB_strcasecmp(sub, "cal") == 0)
  {
    float p1_min, p1_max, p2_min, p2_max;
    FEB_ADC_GetAPPSCalibration(1, &p1_min, &p1_max);
    FEB_ADC_GetAPPSCalibration(2, &p2_min, &p2_max);
    FEB_Console_CsvEmit("apps_cal", "%.0f,%.0f,%.0f,%.0f", (double)p1_min, (double)p1_max, (double)p2_min,
                        (double)p2_max);
    return;
  }
  if (FEB_strcasecmp(sub, "filter") == 0)
  {
    bool enabled;
    uint8_t samples;
    float alpha;
    FEB_ADC_GetAPPSFilterConfig(&enabled, &samples, &alpha);
    FEB_Console_CsvEmit("apps_filter", "%d,%u,%.3f", enabled ? 1 : 0, samples, (double)alpha);
    return;
  }
  if (FEB_strcasecmp(sub, "stream") == 0)
  {
    /* Start a CSV stream that emits asynchronously via CsvEmitAs. */
    if (argc < 4)
    {
      FEB_Console_CsvError("error", "stream_usage");
      return;
    }
    uint32_t period = 0, count = 0;
    if (!parse_uint(argv[2], &period) || period == 0 || !parse_uint(argv[3], &count))
    {
      FEB_Console_CsvError("error", "stream_args");
      return;
    }
    char tx[APPS_STREAM_TX_ID_LEN];
    if (!FEB_Console_CsvCurrentTxId(tx, sizeof(tx)))
    {
      FEB_Console_CsvError("error", "no_tx_id");
      return;
    }
    apps_stream.active = true;
    apps_stream.period_ms = period;
    apps_stream.remaining = (count == 0) ? APPS_STREAM_INFINITE : count;
    apps_stream.next_tick = HAL_GetTick();
    apps_stream.csv = true;
    strncpy(apps_stream.tx_id, tx, sizeof(apps_stream.tx_id) - 1);
    apps_stream.tx_id[sizeof(apps_stream.tx_id) - 1] = '\0';
    FEB_Console_CsvEmit("apps_stream", "started,%lu,%lu", (unsigned long)period, (unsigned long)count);
    return;
  }
  FEB_Console_CsvError("error", "unknown_apps_sub,%s", sub);
}

static void csv_faults(int argc, char *argv[])
{
  if (argc < 2)
  {
    uint32_t active = FEB_ADC_GetActiveFaults();
    static const uint32_t bits[] = {
        (1u << 0), (1u << 1), (1u << 2), (1u << 3), (1u << 4), (1u << 5), (1u << 6), (1u << 7),
    };
    for (size_t i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
    {
      FEB_Console_CsvEmit("fault", "%s,%d,%lu", FEB_ADC_FaultBitName(bits[i]), (active & bits[i]) ? 1 : 0,
                          (unsigned long)FEB_ADC_GetFaultHitCount(bits[i]));
    }
    FEB_Console_CsvEmit("faults_active", "0x%08lX", (unsigned long)active);
    return;
  }
  const char *sub = argv[1];
  if (FEB_strcasecmp(sub, "clear") == 0)
  {
    const char *name = (argc >= 3) ? argv[2] : "all";
    if (FEB_ADC_ClearFaultsByName(name) != ADC_STATUS_OK)
    {
      FEB_Console_CsvError("error", "clear_refused,%s", name);
      return;
    }
    FEB_Console_CsvEmit("faults_clear", "%s", name);
    return;
  }
  if (FEB_strcasecmp(sub, "inject") == 0)
  {
    if (argc < 3)
    {
      FEB_Console_CsvError("error", "inject_usage");
      return;
    }
    uint32_t bit = FEB_ADC_FaultBitFromName(argv[2]);
    if (bit == 0)
    {
      FEB_Console_CsvError("error", "inject_unknown,%s", argv[2]);
      return;
    }
    if (FEB_ADC_InjectFault(bit) != ADC_STATUS_OK)
    {
      FEB_Console_CsvError("error", "inject_refused");
      return;
    }
    FEB_Console_CsvEmit("faults_inject", "%s", FEB_ADC_FaultBitName(bit));
    return;
  }
  FEB_Console_CsvError("error", "unknown_faults_sub,%s", sub);
}

/* ============================================================================
 * Streaming pump (called from FEB_Main_Loop)
 * ============================================================================ */

void PCU_APPS_StreamProcess(void)
{
  if (!apps_stream.active)
    return;
  uint32_t now = HAL_GetTick();
  if ((int32_t)(now - apps_stream.next_tick) < 0)
    return;
  apps_stream.next_tick = now + apps_stream.period_ms;

  if (apps_stream.csv)
    apps_emit_raw_csv(apps_stream.tx_id);
  else
    apps_print_raw();

  if (apps_stream.remaining != APPS_STREAM_INFINITE)
  {
    apps_stream.remaining--;
    if (apps_stream.remaining == 0)
    {
      apps_stream.active = false;
      if (apps_stream.csv && apps_stream.tx_id[0] != '\0')
        FEB_Console_CsvEmitAs(apps_stream.tx_id, "apps_stream", "done");
      else
        FEB_Console_Printf("Stream done\r\n");
    }
  }
}

/* ============================================================================
 * Top-level command descriptors
 * ============================================================================ */

static void cmd_apps_top_text(int argc, char *argv[])
{
  if (argc < 2)
  {
    /* Bare `apps` is owned by FEB_PCU_Commands' summary view. We only
     * see this entry point when registered as a top-level command, e.g.
     * the user typed `apps|raw` directly. Forward to the sub-dispatcher. */
    print_apps_help();
    return;
  }
  PCU_APPS_HandleAppsSubcommand(argc, argv);
}

static void cmd_faults_top_text(int argc, char *argv[])
{
  PCU_APPS_HandleFaultsSubcommand(argc, argv);
}

static const FEB_Console_Cmd_t apps_cmd = {
    .name = "apps",
    .help = "APPS debug tree — `apps|raw|stream|stats|cal|filter|deadzone|mode|sim`",
    .handler = cmd_apps_top_text,
    .csv_handler = csv_apps,
};

static const FEB_Console_Cmd_t faults_cmd = {
    .name = "faults",
    .help = "Fault status / clear / inject (drive-state gated)",
    .handler = cmd_faults_top_text,
    .csv_handler = csv_faults,
};

void PCU_APPS_RegisterCommands(void)
{
  FEB_Console_Register(&apps_cmd);
  FEB_Console_Register(&faults_cmd);
}
