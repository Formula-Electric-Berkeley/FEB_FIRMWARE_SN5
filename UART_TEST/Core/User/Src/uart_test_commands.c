/**
 ******************************************************************************
 * @file           : uart_test_commands.c
 * @brief          : UART_TEST Custom Console Command Implementations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "uart_test_commands.h"
#include "feb_console.h"

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void cmd_hello(int argc, char *argv[]);
static void cmd_blink(int argc, char *argv[]);

/* ============================================================================
 * Command Descriptors
 * ============================================================================ */

const FEB_Console_Cmd_t uart_test_cmd_hello = {
    .name = "hello",
    .help = "Say hello from UART_TEST",
    .handler = cmd_hello,
};

const FEB_Console_Cmd_t uart_test_cmd_blink = {
    .name = "blink",
    .help = "Blink LED (placeholder)",
    .handler = cmd_blink,
};

/* ============================================================================
 * Registration Function
 * ============================================================================ */

void UART_TEST_RegisterCommands(void)
{
  FEB_Console_Register(&uart_test_cmd_hello);
  FEB_Console_Register(&uart_test_cmd_blink);
}

/* ============================================================================
 * Command Handlers
 * ============================================================================ */

static void cmd_hello(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_Console_Printf("Hello from UART_TEST!\r\n");
  FEB_Console_Printf("STM32U575 Console Demo\r\n");
}

static void cmd_blink(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_Console_Printf("LED blink not implemented (no LED configured)\r\n");
}
