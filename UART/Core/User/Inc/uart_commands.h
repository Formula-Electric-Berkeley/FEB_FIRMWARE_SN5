/**
 ******************************************************************************
 * @file           : uart_commands.h
 * @brief          : UART Custom Console Commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Custom console commands for the UART board. These commands are
 * board-specific and demonstrate how to add custom functionality to the
 * console system.
 *
 * Commands:
 *   - hello      : Print a greeting message
 *   - blink      : LED blink placeholder (not implemented on this board)
 *   - flashbench : Run flash read/write/erase benchmark on reserved sectors
 *
 * Usage:
 *   Call UART_RegisterCommands() after FEB_Console_Init() to add
 *   these custom commands to the console.
 *
 ******************************************************************************
 */

#ifndef UART_COMMANDS_H
#define UART_COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_console.h"

  /* ============================================================================
   * Command Descriptors
   * ============================================================================ */

  extern const FEB_Console_Cmd_t uart_cmd_hello;
  extern const FEB_Console_Cmd_t uart_cmd_blink;
  extern const FEB_Console_Cmd_t uart_cmd_flashbench;

  /* ============================================================================
   * Registration Function
   * ============================================================================ */

  /**
   * @brief Register all UART custom commands
   *
   * Call after FEB_Console_Init() to add board-specific commands.
   */
  void UART_RegisterCommands(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_COMMANDS_H */
