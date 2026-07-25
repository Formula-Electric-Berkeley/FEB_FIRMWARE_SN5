/**
 ******************************************************************************
 * @file           : FEB_Commands.h
 * @brief          : DASH-specific console commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_COMMANDS_H
#define FEB_COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_console.h"

  /**
   * @brief Register all DASH-specific console commands
   *
   * Registers: ping, pong, canstop, canstatus
   * Call after FEB_Console_Init().
   */
  void DASH_RegisterCommands(void);

#ifdef __cplusplus
}
#endif

#endif /* FEB_COMMANDS_H */
