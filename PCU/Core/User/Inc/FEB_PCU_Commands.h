/**
 ******************************************************************************
 * @file           : FEB_PCU_Commands.h
 * @brief          : PCU Custom Console Command Declarations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef INC_FEB_PCU_COMMANDS_H_
#define INC_FEB_PCU_COMMANDS_H_

#include "feb_console.h"

/**
 * @brief Register all PCU-specific console commands
 */
void PCU_RegisterCommands(void);

/* Command descriptor (exported for registration) */
extern const FEB_Console_Cmd_t pcu_cmd;

#endif /* INC_FEB_PCU_COMMANDS_H_ */
