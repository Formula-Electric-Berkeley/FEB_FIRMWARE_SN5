/**
 ******************************************************************************
 * @file           : DASH_Commands.h
 * @brief          : Console commands for DASH
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef DASH_COMMANDS_H
#define DASH_COMMANDS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  void DASH_Console_ProcessLine(const char *line, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DASH_COMMANDS_H */
