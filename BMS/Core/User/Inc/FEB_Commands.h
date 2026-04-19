/**
 ******************************************************************************
 * @file           : FEB_Commands.h
 * @brief          : BMS-specific console commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_COMMANDS_H
#define FEB_COMMANDS_H

#include "feb_console.h"

/* BMS-specific module tags */
#define TAG_ADBMS "[ADBMS]"

/**
 * @brief Register all BMS-specific console commands
 *
 * Registers: status, cells, temps, balance
 * Call after FEB_Console_Init().
 */
void BMS_RegisterCommands(void);

#endif /* FEB_COMMANDS_H */
