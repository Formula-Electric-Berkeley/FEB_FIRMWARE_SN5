/**
 ******************************************************************************
 * @file           : FEB_Commands.h
 * @brief          : DASH-specific console commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_COMMANDS_H
#define FEB_COMMANDS_H

#include "feb_console.h"

/**
 * @brief Register all DASH-specific console commands
 *
 * Registers: ping, pong, canstop, canstatus
 * Call after FEB_Console_Init().
 */
void DASH_RegisterCommands(void);

#endif /* FEB_COMMANDS_H */
