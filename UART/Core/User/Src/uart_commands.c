/**
 ******************************************************************************
 * @file           : uart_commands.c
 * @brief          : UART Custom Console Command Implementations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "uart_commands.h"
#include "feb_console.h"
#include "feb_log.h"
#include "feb_string_utils.h"
#include "flash_benchmark.h"
#include "main.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

/* ============================================================================
 * Blink Configuration
 * ============================================================================ */

#define BLINK_DEFAULT_COUNT 3
#define BLINK_DEFAULT_PERIOD_MS 100
#define BLINK_MIN_PERIOD_MS 10
#define BLINK_MAX_PERIOD_MS 10000
#define BLINK_MAX_COUNT 255

/* ============================================================================
 * Blink Timer State
 * ============================================================================ */

static osTimerId_t blink_timer_id = NULL;
static volatile uint8_t blink_count = 0;
static volatile uint8_t blink_target_count = 0;
static volatile bool blink_led_on = false;
static volatile bool blink_continuous = false;
static volatile uint32_t blink_period_ms = BLINK_DEFAULT_PERIOD_MS;
// Set by blink_start() when invoked from a CSV handler. Lets the timer
// callback suppress its "Done."/failure prints so they don't appear
// asynchronously inside the machine-readable CSV stream.
static volatile bool blink_quiet = false;

static const osTimerAttr_t blink_timer_attr = {.name = "blinkTimer"};

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void blink_timer_callback(void *argument);
static void blink_stop(void);
static bool blink_start(uint8_t count, uint32_t period_ms, bool continuous, bool quiet);
static void cmd_blink(int argc, char *argv[]);
static void cmd_blink_csv(int argc, char *argv[]);
static void cmd_flashbench(int argc, char *argv[]);
static void cmd_flashbench_csv(int argc, char *argv[]);

/* ============================================================================
 * Command Descriptors
 * ============================================================================ */

const FEB_Console_Cmd_t uart_cmd_blink = {
    .name = "blink",
    .help = "Blink LD2: blink [count] [period_ms] | blink|stop | blink|help",
    .handler = cmd_blink,
    .csv_handler = cmd_blink_csv,
};

const FEB_Console_Cmd_t uart_cmd_flashbench = {
    .name = "flashbench",
    .help = "Flash benchmark (ERASES sector 7!): flashbench [iterations] [pattern_hex]",
    .handler = cmd_flashbench,
    .csv_handler = cmd_flashbench_csv,
};

/* ============================================================================
 * Registration Function
 * ============================================================================ */

void UART_RegisterCommands(void)
{
  FEB_Console_Register(&uart_cmd_blink);
  FEB_Console_Register(&uart_cmd_flashbench);
}

/* ============================================================================
 * Command Handlers
 * ============================================================================ */

/**
 * @brief Timer callback for non-blocking LED blink
 *
 * Supports both finite and continuous blinking modes.
 *
 * @param argument Unused
 */
static void blink_timer_callback(void *argument)
{
  (void)argument;

  if (blink_led_on)
  {
    /* Turn LED OFF */
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

    if (blink_continuous)
    {
      LOG_T(TAG_GPIO, "Continuous blink: LED OFF");
    }
    else
    {
      LOG_T(TAG_GPIO, "Blink cycle %d/%d: LED OFF", (int)(blink_target_count - blink_count) + 1,
            (int)blink_target_count);
    }

    blink_led_on = false;

    if (!blink_continuous)
    {
      if (blink_count == 1)
      {
        /* Last blink - try to stop timer first */
        osStatus_t status = osTimerStop(blink_timer_id);
        if (status == osOK)
        {
          blink_count = 0;
          if (!blink_quiet)
          {
            FEB_Console_Printf("Done.\r\n");
          }
        }
        else
        {
          /* Keep blink_count at 1 so we retry next callback */
          if (!blink_quiet)
          {
            FEB_Console_Printf("Failed to stop timer: %d\r\n", status);
          }
        }
      }
      else
      {
        blink_count--;
      }
    }
  }
  else
  {
    /* Turn LED ON */
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);

    if (blink_continuous)
    {
      LOG_T(TAG_GPIO, "Continuous blink: LED ON");
    }
    else
    {
      LOG_T(TAG_GPIO, "Blink cycle %d/%d: LED ON", (int)(blink_target_count - blink_count) + 1,
            (int)blink_target_count);
    }

    blink_led_on = true;
  }
}

/**
 * @brief Stop blinking and reset state
 */
static void blink_stop(void)
{
  if (blink_timer_id != NULL && osTimerIsRunning(blink_timer_id))
  {
    osStatus_t status = osTimerStop(blink_timer_id);
    if (status == osOK)
    {
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
      blink_led_on = false;
      blink_continuous = false;
      blink_count = 0;
      FEB_Console_Printf("Blink stopped.\r\n");
      LOG_T(TAG_GPIO, "Blink stopped by user");
    }
    else
    {
      FEB_Console_Printf("Failed to stop blink: %d\r\n", status);
      LOG_T(TAG_GPIO, "osTimerStop failed: %d", status);
    }
  }
  else
  {
    FEB_Console_Printf("No blink in progress.\r\n");
  }
}

/**
 * @brief Start blinking with specified parameters.
 *
 * @param quiet  When true, suppress all human-readable status prints so the
 *               function can be invoked from CSV-mode handlers without
 *               corrupting the machine-readable stream.
 */
static bool blink_start(uint8_t count, uint32_t period_ms, bool continuous, bool quiet)
{
  /* Validate period */
  if (period_ms < BLINK_MIN_PERIOD_MS || period_ms > BLINK_MAX_PERIOD_MS)
  {
    if (!quiet)
    {
      FEB_Console_Printf("Error: Period must be %d-%d ms\r\n", BLINK_MIN_PERIOD_MS, BLINK_MAX_PERIOD_MS);
    }
    return false;
  }

  /* Create timer if needed */
  if (blink_timer_id == NULL)
  {
    blink_timer_id = osTimerNew(blink_timer_callback, osTimerPeriodic, NULL, &blink_timer_attr);
    if (blink_timer_id == NULL)
    {
      if (!quiet)
      {
        FEB_Console_Printf("Error: Failed to create blink timer\r\n");
      }
      return false;
    }
  }

  /* Set state */
  blink_count = count;
  blink_target_count = count;
  blink_continuous = continuous;
  blink_period_ms = period_ms;
  blink_led_on = true;
  blink_quiet = quiet;

  /* Turn on LED immediately */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);

  if (!quiet)
  {
    if (continuous)
    {
      FEB_Console_Printf("Blinking continuously at %lu ms (use 'blink|stop' to stop)\r\n", (unsigned long)period_ms);
    }
    else
    {
      FEB_Console_Printf("Blinking LD2 (PA5) %d times at %lu ms...\r\n", count, (unsigned long)period_ms);
    }
  }
  if (continuous)
  {
    LOG_T(TAG_GPIO, "Continuous blink started at %lu ms", (unsigned long)period_ms);
  }
  else
  {
    LOG_T(TAG_GPIO, "Blink cycle 1/%d: LED ON", count);
  }

  /* Start timer */
  if (osTimerStart(blink_timer_id, pdMS_TO_TICKS(period_ms)) != osOK)
  {
    if (!quiet)
    {
      FEB_Console_Printf("Error: Failed to start blink timer\r\n");
    }
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    blink_led_on = false;
    blink_continuous = false;
    return false;
  }

  return true;
}

/**
 * @brief Print blink command help
 */
static void print_blink_help(void)
{
  FEB_Console_Printf("Blink Commands:\r\n");
  FEB_Console_Printf("  blink              - Blink %d times at %dms (default)\r\n", BLINK_DEFAULT_COUNT,
                     BLINK_DEFAULT_PERIOD_MS);
  FEB_Console_Printf("  blink|N            - Blink N times at %dms\r\n", BLINK_DEFAULT_PERIOD_MS);
  FEB_Console_Printf("  blink|N|PERIOD     - Blink N times at PERIOD ms\r\n");
  FEB_Console_Printf("  blink|0            - Continuous blink at %dms\r\n", BLINK_DEFAULT_PERIOD_MS);
  FEB_Console_Printf("  blink|0|PERIOD     - Continuous blink at PERIOD ms\r\n");
  FEB_Console_Printf("  blink|forever      - Continuous blink (alias for 0)\r\n");
  FEB_Console_Printf("  blink|stop         - Stop blinking\r\n");
  FEB_Console_Printf("  blink|help         - Show this help\r\n");
  FEB_Console_Printf("Period range: %d-%d ms\r\n", BLINK_MIN_PERIOD_MS, BLINK_MAX_PERIOD_MS);
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("CSV Protocol (machine-readable):\r\n");
  FEB_Console_Printf("  UART|csv|<tx_id>|blink                 - Default blink (start,N,period,ok)\r\n");
  FEB_Console_Printf("  UART|csv|<tx_id>|blink|<N>             - N blinks at %d ms\r\n", BLINK_DEFAULT_PERIOD_MS);
  FEB_Console_Printf("  UART|csv|<tx_id>|blink|<N>|<PERIOD>    - N blinks at PERIOD ms\r\n");
  FEB_Console_Printf("  UART|csv|<tx_id>|blink|0               - Continuous (alias: forever)\r\n");
  FEB_Console_Printf("  UART|csv|<tx_id>|blink|stop            - Stop (emits blink,stop,ok)\r\n");
  FEB_Console_Printf("  UART|csv|<tx_id>|flashbench [iter] [pat] - Queue flash benchmark\r\n");
  FEB_Console_Printf("  UART|csv|<tx_id>|commands              - List CSV commands\r\n");
  FEB_Console_Printf("  UART|csv|<tx_id>|hello                 - Heartbeat\r\n");
  FEB_Console_Printf("  *|csv|<tx_id>|hello                          - Discover all boards\r\n");
  FEB_Console_Printf("Each request emits: ack -> [rows] -> done\r\n");
}

/**
 * @brief Blink command handler
 *
 * Syntax:
 *   blink              - Default: 3 blinks at 100ms
 *   blink|N            - N blinks at 100ms
 *   blink|N|PERIOD     - N blinks at PERIOD ms
 *   blink|0            - Continuous at 100ms
 *   blink|0|PERIOD     - Continuous at PERIOD ms
 *   blink|forever      - Alias for continuous
 *   blink|stop         - Stop blinking
 *   blink|help         - Show help
 */
static void cmd_blink(int argc, char *argv[])
{
  uint8_t count = BLINK_DEFAULT_COUNT;
  uint32_t period_ms = BLINK_DEFAULT_PERIOD_MS;
  bool continuous = false;

  /* Handle subcommands */
  if (argc >= 2)
  {
    /* Check for 'stop' command */
    if (FEB_strcasecmp(argv[1], "stop") == 0)
    {
      blink_stop();
      return;
    }

    /* Check for 'help' command */
    if (FEB_strcasecmp(argv[1], "help") == 0)
    {
      print_blink_help();
      return;
    }

    /* Check for 'forever' keyword */
    if (FEB_strcasecmp(argv[1], "forever") == 0)
    {
      continuous = true;
      count = 0;
    }
    else
    {
      /* Parse count as number */
      char *endptr;
      errno = 0;
      unsigned long parsed = strtoul(argv[1], &endptr, 10);

      if (endptr == argv[1] || *endptr != '\0')
      {
        FEB_Console_Printf("Error: Invalid count '%s'\r\n", argv[1]);
        return;
      }
      if (errno == ERANGE || parsed > BLINK_MAX_COUNT)
      {
        FEB_Console_Printf("Error: Count must be 0-%d (0 = continuous)\r\n", BLINK_MAX_COUNT);
        return;
      }

      count = (uint8_t)parsed;
      if (count == 0)
      {
        continuous = true;
      }
    }

    /* Parse period if provided */
    if (argc >= 3)
    {
      char *endptr;
      errno = 0;
      unsigned long parsed = strtoul(argv[2], &endptr, 10);

      if (endptr == argv[2] || *endptr != '\0')
      {
        FEB_Console_Printf("Error: Invalid period '%s'\r\n", argv[2]);
        return;
      }
      if (errno == ERANGE)
      {
        FEB_Console_Printf("Error: Period out of range\r\n");
        return;
      }

      period_ms = (uint32_t)parsed;
    }
  }

  /* Validate period before potentially stopping a running blink */
  if (period_ms < BLINK_MIN_PERIOD_MS || period_ms > BLINK_MAX_PERIOD_MS)
  {
    FEB_Console_Printf("Error: Period must be %d-%d ms\r\n", BLINK_MIN_PERIOD_MS, BLINK_MAX_PERIOD_MS);
    return;
  }

  /* Check if already blinking */
  if (blink_timer_id != NULL && osTimerIsRunning(blink_timer_id))
  {
    if (continuous || blink_continuous)
    {
      /* Allow stopping continuous mode to start a new blink */
      FEB_Console_Printf("Stopping current blink...\r\n");
      osStatus_t status = osTimerStop(blink_timer_id);
      if (status != osOK)
      {
        FEB_Console_Printf("Failed to stop timer: %d\r\n", status);
        return;
      }
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    }
    else
    {
      FEB_Console_Printf("Blink already in progress\r\n");
      return;
    }
  }

  /* Start blinking */
  blink_start(count, period_ms, continuous, false);
}

static void print_stats(const char *name, const FlashBench_Stats_t *s)
{
  FEB_Console_Printf("  %-8s: %lu / %lu / %lu us, %lu / %lu / %lu KB/s\r\n", name, s->min.time_us, s->avg.time_us,
                     s->max.time_us, s->max.throughput_kbs, s->avg.throughput_kbs, s->min.throughput_kbs);
}

static void flashbench_callback(const FlashBench_StatsResult_t *stats)
{
  FEB_Console_Printf("\r\n=== Flash Benchmark Results ===\r\n");
  FEB_Console_Printf("CPU: %lu MHz, Iterations: %lu, Pattern: 0x%02lX\r\n", stats->cpu_freq_mhz, stats->iterations,
                     stats->write_pattern);
  FEB_Console_Printf("\r\nResults (min / avg / max):\r\n");
  print_stats("Erase", &stats->erase);
  print_stats("Write", &stats->write);
  print_stats("Read", &stats->read);
  FEB_Console_Printf("\r\nBenchmark complete.\r\n");
}

/* Captured at CSV-handler dispatch time so the async flashbench callback
 * can correlate its row with the original transaction. Single-slot - only
 * one benchmark may be queued at a time, and the handler re-captures on
 * each invocation. */
static char flashbench_csv_tx_id[FEB_CSV_TX_ID_MAX_LEN + 1];

/* CSV sibling of flashbench_callback. Runs on the FlashBench task, after
 * the original transaction has ended, so we use FEB_Console_CsvEmitAs
 * with the captured tx_id to stay correlated. Body layout:
 *   cpu_mhz,iterations,pattern,
 *   erase_min,erase_avg,erase_max,
 *   write_min,write_avg,write_max,
 *   read_min,read_avg,read_max
 * All durations are microseconds. */
static void flashbench_csv_callback(const FlashBench_StatsResult_t *stats)
{
  if (flashbench_csv_tx_id[0] == '\0')
  {
    return;
  }
  FEB_Console_CsvEmitAs(flashbench_csv_tx_id, "bench", "%lu,%lu,0x%02lX,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                        (unsigned long)stats->cpu_freq_mhz, (unsigned long)stats->iterations,
                        (unsigned long)stats->write_pattern, (unsigned long)stats->erase.min.time_us,
                        (unsigned long)stats->erase.avg.time_us, (unsigned long)stats->erase.max.time_us,
                        (unsigned long)stats->write.min.time_us, (unsigned long)stats->write.avg.time_us,
                        (unsigned long)stats->write.max.time_us, (unsigned long)stats->read.min.time_us,
                        (unsigned long)stats->read.avg.time_us, (unsigned long)stats->read.max.time_us);
  flashbench_csv_tx_id[0] = '\0';
}

static void cmd_flashbench(int argc, char *argv[])
{
  uint32_t iterations = 1;
  uint8_t pattern = 0xAA;

  /* Parse iterations argument */
  if (argc >= 2)
  {
    iterations = (uint32_t)strtoul(argv[1], NULL, 10);
    if (iterations == 0 || iterations > 100)
    {
      FEB_Console_Printf("Error: iterations must be 1-100\r\n");
      return;
    }
  }

  /* Parse pattern argument (hex) */
  if (argc >= 3)
  {
    pattern = (uint8_t)strtoul(argv[2], NULL, 16);
  }

  FEB_Console_Printf("Queuing benchmark: %lu iterations, pattern 0x%02X\r\n", iterations, pattern);
  FEB_Console_Printf("Sector 7 @ 0x%08X (128 KB)\r\n", FLASH_BENCH_SECTOR_7_ADDR);

  FlashBench_Request_t req = {.iterations = iterations, .write_pattern = pattern, .callback = flashbench_callback};

  if (!FlashBench_QueueRequest(&req))
  {
    FEB_Console_Printf("Error: Failed to queue benchmark request\r\n");
  }
}

static void cmd_blink_csv(int argc, char *argv[])
{
  uint8_t count = BLINK_DEFAULT_COUNT;
  uint32_t period_ms = BLINK_DEFAULT_PERIOD_MS;
  bool continuous = false;
  const char *mode = "start";

  if (argc >= 2)
  {
    if (FEB_strcasecmp(argv[1], "stop") == 0)
    {
      bool was_running = (blink_timer_id != NULL && osTimerIsRunning(blink_timer_id));
      bool stop_ok = false;
      if (was_running)
      {
        stop_ok = (osTimerStop(blink_timer_id) == osOK);
        if (stop_ok)
        {
          HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
          blink_led_on = false;
          blink_continuous = false;
          blink_count = 0;
        }
      }
      FEB_Console_CsvEmit("blink", "stop,%d", stop_ok ? 1 : 0);
      return;
    }
    if (FEB_strcasecmp(argv[1], "help") == 0)
    {
      FEB_Console_CsvEmit("blink", "help,%d,%d", BLINK_DEFAULT_COUNT, BLINK_DEFAULT_PERIOD_MS);
      return;
    }
    if (FEB_strcasecmp(argv[1], "forever") == 0)
    {
      continuous = true;
      count = 0;
      mode = "forever";
    }
    else
    {
      char *endptr;
      errno = 0;
      unsigned long parsed = strtoul(argv[1], &endptr, 10);
      if (endptr == argv[1] || *endptr != '\0' || errno == ERANGE || parsed > BLINK_MAX_COUNT)
      {
        FEB_Console_CsvError("error", "blink_count,%s", argv[1]);
        return;
      }
      count = (uint8_t)parsed;
      if (count == 0)
      {
        continuous = true;
        mode = "forever";
      }
    }
    if (argc >= 3)
    {
      char *endptr;
      errno = 0;
      unsigned long parsed = strtoul(argv[2], &endptr, 10);
      if (endptr == argv[2] || *endptr != '\0' || errno == ERANGE)
      {
        FEB_Console_CsvError("error", "blink_period,%s", argv[2]);
        return;
      }
      period_ms = (uint32_t)parsed;
    }
  }

  if (period_ms < BLINK_MIN_PERIOD_MS || period_ms > BLINK_MAX_PERIOD_MS)
  {
    FEB_Console_CsvError("error", "blink_period_range,%lu", (unsigned long)period_ms);
    return;
  }

  if (blink_timer_id != NULL && osTimerIsRunning(blink_timer_id))
  {
    if (continuous || blink_continuous)
    {
      if (osTimerStop(blink_timer_id) != osOK)
      {
        FEB_Console_CsvError("error", "blink_stop_failed");
        return;
      }
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    }
    else
    {
      FEB_Console_CsvError("warn", "blink_busy");
      return;
    }
  }

  bool ok = blink_start(count, period_ms, continuous, true);
  FEB_Console_CsvEmit("blink", "%s,%u,%lu,%d", mode, count, (unsigned long)period_ms, ok ? 1 : 0);
}

static void cmd_flashbench_csv(int argc, char *argv[])
{
  uint32_t iterations = 1;
  uint8_t pattern = 0xAA;

  if (argc >= 2)
  {
    // strict parse: full consumption + errno check. Passing NULL to strtoul
    // would accept garbage like "12abc" as 12 and hide an invalid-queue call.
    char *endptr;
    errno = 0;
    unsigned long parsed = strtoul(argv[1], &endptr, 10);
    if (endptr == argv[1] || *endptr != '\0' || errno != 0 || parsed < 1 || parsed > 100)
    {
      FEB_Console_CsvError("error", "flashbench_iter,%s", argv[1]);
      return;
    }
    iterations = (uint32_t)parsed;
  }
  if (argc >= 3)
  {
    char *endptr;
    errno = 0;
    unsigned long parsed = strtoul(argv[2], &endptr, 16);
    if (endptr == argv[2] || *endptr != '\0' || errno != 0 || parsed > 0xFF)
    {
      FEB_Console_CsvError("error", "flashbench_pattern,%s", argv[2]);
      return;
    }
    pattern = (uint8_t)parsed;
  }

  /* Capture tx_id so the async callback can emit a correlated result row
   * after this handler (and its transaction's auto-done) has already
   * returned. */
  if (!FEB_Console_CsvCurrentTxId(flashbench_csv_tx_id, sizeof(flashbench_csv_tx_id)))
  {
    flashbench_csv_tx_id[0] = '\0';
  }

  FlashBench_Request_t req = {.iterations = iterations, .write_pattern = pattern, .callback = flashbench_csv_callback};
  int queued = FlashBench_QueueRequest(&req) ? 1 : 0;
  FEB_Console_CsvEmit("flashbench", "%lu,0x%02X,%d", (unsigned long)iterations, pattern, queued);
}
