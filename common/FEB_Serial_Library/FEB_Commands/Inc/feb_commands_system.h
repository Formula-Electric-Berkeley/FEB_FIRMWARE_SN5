/**
 ******************************************************************************
 * @file           : feb_commands_system.h
 * @brief          : FEB System Command Declarations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Declares individual system commands for selective registration.
 *
 * Usage:
 *   #include "feb_commands_system.h"
 *
 *   // Register specific commands only
 *   FEB_Console_Register(&feb_cmd_help);
 *   FEB_Console_Register(&feb_cmd_version);
 *
 ******************************************************************************
 */

#ifndef FEB_COMMANDS_SYSTEM_H
#define FEB_COMMANDS_SYSTEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_console.h"

  /* ============================================================================
   * System Command Descriptors
   * ============================================================================
   *
   * Individual command descriptors for selective registration.
   * Use FEB_Commands_RegisterSystem() to register all at once.
   */

  /**
   * @brief echo - Print arguments
   *
   * Usage: echo|text to print
   * Example: echo|hello world
   */
  extern const FEB_Console_Cmd_t feb_cmd_echo;

  /**
   * @brief help - Show available commands (human-readable)
   *
   * Usage: help [command]
   * Example: help
   * Example: help|reboot
   */
  extern const FEB_Console_Cmd_t feb_cmd_help;

  /**
   * @brief commands - List registered commands (CSV protocol)
   *
   * CSV usage (primary): <board>|csv|<tx>|commands
   * Text usage: commands    (prints name + description per line)
   */
  extern const FEB_Console_Cmd_t feb_cmd_commands;

  /**
   * @brief hello - Heartbeat / discovery command
   *
   * CSV usage: <board>|csv|<tx>|hello    or    *|csv|<tx>|hello
   * Text usage: hello
   */
  extern const FEB_Console_Cmd_t feb_cmd_hello;

  /**
   * @brief version - Show firmware version and build info
   */
  extern const FEB_Console_Cmd_t feb_cmd_version;

  /**
   * @brief uptime - Show system uptime
   */
  extern const FEB_Console_Cmd_t feb_cmd_uptime;

  /**
   * @brief reboot - Perform software reset
   */
  extern const FEB_Console_Cmd_t feb_cmd_reboot;

  /**
   * @brief log - Get/set log level
   *
   * Usage: log | log|<error|warn|info|debug|trace|none>
   */
  extern const FEB_Console_Cmd_t feb_cmd_log;

#ifdef __cplusplus
}
#endif

#endif /* FEB_COMMANDS_SYSTEM_H */
