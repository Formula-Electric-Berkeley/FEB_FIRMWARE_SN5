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

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void cmd_blink(int argc, char *argv[]);
static void cmd_blink_csv(int argc, char *argv[]);

/* ============================================================================
 * Command Descriptors
 * ============================================================================ */

const FEB_Console_Cmd_t uart_test_cmd_blink = {
    .name = "blink",
    .help = "Blink LED (placeholder - no LED on this board)",
    .handler = cmd_blink,
    .csv_handler = cmd_blink_csv,
};

/* ============================================================================
 * Registration Function
 * ============================================================================ */

void UART_TEST_RegisterCommands(void)
{
  FEB_Console_Register(&uart_test_cmd_blink);
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
