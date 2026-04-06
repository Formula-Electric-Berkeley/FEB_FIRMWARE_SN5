/**
 ******************************************************************************
 * @file           : FEB_DART_Commands.c
 * @brief          : DART-specific console commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "FEB_DART_Commands.h"
#include "feb_console.h"

/* ============================================================================
 * Commands for DART
 * ============================================================================ */

static void cmd_hello(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("Hello World from DART!\r\n");
}

static const FEB_Console_Cmd_t dart_cmd_hello = {
    .name = "hello",
    .help = "Print hello world",
    .handler = cmd_hello,
};

/* ============================================================================
 * Registration
 * ============================================================================ */

void DART_RegisterCommands(void)
{
  FEB_Console_Register(&dart_cmd_hello);
}
