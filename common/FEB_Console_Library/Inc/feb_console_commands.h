/**
 ******************************************************************************
 * @file           : feb_console_commands.h
 * @brief          : FEB Console Built-in Commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Declares the built-in console commands that are automatically registered
 * when FEB_Console_Init() is called. These commands provide basic system
 * functionality:
 *
 *   - echo   : Print arguments back (use pipe delimiter: echo|hello world)
 *   - help   : List all commands or get help for a specific command
 *   - version: Show firmware version and build info
 *   - uptime : Show system uptime
 *   - reboot : Perform software reset
 *   - log    : Get/set log level
 *
 * Board-specific commands should be defined in separate files following
 * the same pattern (see uart_test_commands.c/h for example).
 *
 ******************************************************************************
 */

#ifndef FEB_CONSOLE_COMMANDS_H
#define FEB_CONSOLE_COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_console.h"

  /* ============================================================================
   * Built-in Command Descriptors
   * ============================================================================ */

  extern const FEB_Console_Cmd_t feb_console_cmd_echo;
  extern const FEB_Console_Cmd_t feb_console_cmd_help;
  extern const FEB_Console_Cmd_t feb_console_cmd_version;
  extern const FEB_Console_Cmd_t feb_console_cmd_uptime;
  extern const FEB_Console_Cmd_t feb_console_cmd_reboot;
  extern const FEB_Console_Cmd_t feb_console_cmd_log;

  /* ============================================================================
   * Registration Function
   * ============================================================================ */

  /**
   * @brief Register all built-in commands
   *
   * Called automatically by FEB_Console_Init(). Registers echo, help,
   * version, uptime, reboot, and log commands.
   */
  void FEB_Console_RegisterBuiltins(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CONSOLE_COMMANDS_H */
