/**
 ******************************************************************************
 * @file           : rtc_commands.h
 * @brief          : RTC Console Commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Console commands for RTC interaction:
 *   rtc           - Show help
 *   rtc|get       - Display current date/time
 *   rtc|time      - Display time only
 *   rtc|date      - Display date only
 *   rtc|set|YYYY|MM|DD|HH|MM|SS - Set full date/time
 *   rtc|settime|HH|MM|SS        - Set time only
 *   rtc|setdate|YYYY|MM|DD      - Set date only
 *
 ******************************************************************************
 */

#ifndef RTC_COMMANDS_H
#define RTC_COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_console.h"

  /* Command descriptor */
  extern const FEB_Console_Cmd_t rtc_cmd;

  /**
   * @brief Register RTC commands
   *
   * Call after FEB_Console_Init().
   */
  void RTC_RegisterCommands(void);

#ifdef __cplusplus
}
#endif

#endif /* RTC_COMMANDS_H */
