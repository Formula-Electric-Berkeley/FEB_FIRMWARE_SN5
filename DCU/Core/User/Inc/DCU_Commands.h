/**
 ******************************************************************************
 * @file           : DCU_Commands.h
 * @brief          : Console commands for DCU
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef DCU_COMMANDS_H
#define DCU_COMMANDS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Register all DCU-specific console commands
   *
   * Registers the following commands:
   *   - dcu       : Show help
   *   - dcu|tps   : Show TPS power measurements
   *   - dcu|can   : Show CAN status and error counters
   *   - dcu|radio : Show radio status
   *
   * @return true on success, false if registration fails
   */
  bool DCU_RegisterCommands(void);

#ifdef __cplusplus
}
#endif

#endif /* DCU_COMMANDS_H */
