/**
 ******************************************************************************
 * @file           : feb_commands.h
 * @brief          : FEB Commands Library - Default System Commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides default system commands for the FEB Console:
 *   - echo   : Print arguments
 *   - help   : Show available commands
 *   - version: Show firmware build info
 *   - uptime : Show system uptime
 *   - reboot : Perform software reset
 *   - log    : Get/set log level
 *
 * Usage:
 *   #include "feb_console.h"
 *   #include "feb_commands.h"
 *
 *   // Initialize console and register system commands
 *   FEB_Console_Init();
 *   FEB_Commands_RegisterSystem();
 *
 *   // Or register individual commands selectively:
 *   #include "feb_commands_system.h"
 *   FEB_Console_Register(&feb_cmd_help);
 *   FEB_Console_Register(&feb_cmd_version);
 *
 ******************************************************************************
 */

#ifndef FEB_COMMANDS_H
#define FEB_COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Register all system commands
   *
   * Registers: echo, help, version, uptime, reboot, log
   *
   * @note Call after FEB_Console_Init()
   */
  void FEB_Commands_RegisterSystem(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_COMMANDS_H */
