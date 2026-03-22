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
#include "flash_benchmark.h"
#include "main.h"
#include "cmsis_os2.h"
#include <stdlib.h>

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void cmd_blink(int argc, char *argv[]);
static void cmd_flashbench(int argc, char *argv[]);

/* ============================================================================
 * Command Descriptors
 * ============================================================================ */

const FEB_Console_Cmd_t uart_cmd_blink = {
    .name = "blink",
    .help = "Blink LD2 (PA5) 3 times",
    .handler = cmd_blink,
};

const FEB_Console_Cmd_t uart_cmd_flashbench = {
    .name = "flashbench",
    .help = "Flash benchmark (ERASES sector 7!): flashbench [iterations] [pattern_hex]",
    .handler = cmd_flashbench,
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

static void cmd_blink(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("Blinking LD2 (PA5) 3 times...\r\n");

  for (int i = 0; i < 3; i++)
  {
    LOG_T(TAG_GPIO, "Blink cycle %d: LED ON", i + 1);
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    osDelay(100);
    LOG_T(TAG_GPIO, "Blink cycle %d: LED OFF", i + 1);
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    osDelay(100);
  }

  FEB_Console_Printf("Done.\r\n");
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
