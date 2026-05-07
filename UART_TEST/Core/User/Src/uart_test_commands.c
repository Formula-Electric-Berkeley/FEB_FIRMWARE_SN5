/**
 ******************************************************************************
 * @file           : uart_test_commands.c
 * @brief          : UART_TEST Custom Console Command Implementations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * The system `hello` command is registered by FEB_Commands_RegisterSystem()
 * and already fulfills the CSV protocol's mandatory `hello` discovery. We
 * only register `blink` here.
 *
 ******************************************************************************
 */

#include "uart_test_commands.h"
#include "feb_console.h"
#include "feb_string_utils.h"

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void cmd_blink(int argc, char *argv[]);
static void cmd_blink_csv(int argc, char *argv[]);

/* ============================================================================
 * Command Descriptors
 *
 * Per-subcommand descriptors are marked .hidden so the top-level `help`
 * lists only the UART_TEST parent. They stay registered top-level so the
 * CSV protocol's `UART_TEST|csv|<tx>|<sub>` lookup resolves directly.
 * ============================================================================ */

const FEB_Console_Cmd_t uart_test_cmd_blink = {
    .name = "blink",
    .help = "Blink LED (placeholder - no LED on this board)",
    .handler = cmd_blink,
    .csv_handler = cmd_blink_csv,
    .hidden = true,
};

/* ============================================================================
 * Mega-dispatcher and Registration
 * ============================================================================ */

static const FEB_Console_Cmd_t *const UART_TEST_SUBCMDS[] = {
    &uart_test_cmd_blink,
};
#define UART_TEST_SUBCMDS_COUNT (sizeof(UART_TEST_SUBCMDS) / sizeof(UART_TEST_SUBCMDS[0]))

static void print_uart_test_help(void)
{
  FEB_Console_Printf("UART_TEST Commands (UART_TEST|<sub>):\r\n");
  for (size_t i = 0; i < UART_TEST_SUBCMDS_COUNT; i++)
  {
    FEB_Console_Printf("  UART_TEST|%-12s - %s\r\n", UART_TEST_SUBCMDS[i]->name, UART_TEST_SUBCMDS[i]->help);
  }
  FEB_Console_Printf("\r\n");
  FEB_Console_Printf("CSV Protocol (machine-readable):\r\n");
  FEB_Console_Printf("  UART_TEST|csv|<tx_id>|<sub>  - any subcommand above also works as CSV\r\n");
  FEB_Console_Printf("  *|csv|<tx_id>|hello          - Discover all boards (system command)\r\n");
  FEB_Console_Printf("Each request emits: ack -> [rows] -> done\r\n");
}

static void cmd_uart_test(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_uart_test_help();
    return;
  }
  const char *subcmd = argv[1];
  for (size_t i = 0; i < UART_TEST_SUBCMDS_COUNT; i++)
  {
    if (FEB_strcasecmp(UART_TEST_SUBCMDS[i]->name, subcmd) == 0)
    {
      if (UART_TEST_SUBCMDS[i]->handler != NULL)
      {
        UART_TEST_SUBCMDS[i]->handler(argc - 1, argv + 1);
      }
      else
      {
        FEB_Console_Printf("Subcommand %s is CSV-only\r\n", subcmd);
      }
      return;
    }
  }
  FEB_Console_Printf("Unknown subcommand: %s\r\n", subcmd);
  print_uart_test_help();
}

static const FEB_Console_Cmd_t uart_test_cmd = {
    .name = "UART_TEST",
    .help = "UART_TEST commands (UART_TEST|<sub>) - run UART_TEST alone for full list",
    .handler = cmd_uart_test,
    .csv_handler = NULL,
};

void UART_TEST_RegisterCommands(void)
{
  FEB_Console_Register(&uart_test_cmd);
  for (size_t i = 0; i < UART_TEST_SUBCMDS_COUNT; i++)
  {
    FEB_Console_Register(UART_TEST_SUBCMDS[i]);
  }
}

/* ============================================================================
 * Command Handlers
 * ============================================================================ */

static void cmd_blink(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_Console_Printf("LED blink not implemented (no LED configured)\r\n");
}

static void cmd_blink_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_Console_CsvEmit("blink", "not_implemented");
}
