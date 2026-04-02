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
   * @brief help - Show available commands
   *
   * Usage: help [command]
   * Example: help
   * Example: help|reboot
   */
  extern const FEB_Console_Cmd_t feb_cmd_help;

  /**
   * @brief version - Show firmware version and build info
   *
   * Usage: version
   */
  extern const FEB_Console_Cmd_t feb_cmd_version;

  /**
   * @brief uptime - Show system uptime
   *
   * Usage: uptime
   */
  extern const FEB_Console_Cmd_t feb_cmd_uptime;

  /**
   * @brief reboot - Perform software reset
   *
   * Usage: reboot
   */
  extern const FEB_Console_Cmd_t feb_cmd_reboot;

  /**
   * @brief log - Get/set log level
   *
   * Usage: log
   * Usage: log|<error|warn|info|debug|trace|none>
   * Example: log|debug
   */
  extern const FEB_Console_Cmd_t feb_cmd_log;

#ifdef __cplusplus
}
#endif

#endif /* FEB_COMMANDS_SYSTEM_H */
