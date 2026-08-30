/**
 ******************************************************************************
 * @file           : FEB_Commands.h
 * @brief          : BMS-specific console commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_COMMANDS_H
#define FEB_COMMANDS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* BMS-specific module tags */
#define TAG_ADBMS "[ADBMS]"

  void BMS_Console_ProcessLine(const char *line, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* FEB_COMMANDS_H */
