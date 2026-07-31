/**
 ******************************************************************************
 * @file           : DCU_Commands.h
 * @brief          : Console commands for DCU_Receiver
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef DCU_COMMANDS_H
#define DCU_COMMANDS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  void DCU_Console_ProcessLine(int uart_instance, const char *line, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DCU_COMMANDS_H */
