/**
 ******************************************************************************
 * @file           : uart_test_commands.h
 * @brief          : UART_TEST Custom Console Commands
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Custom console commands for the UART_TEST board. These commands are
 * board-specific and demonstrate how to add custom functionality to the
 * console system.
 *
 * Commands:
 *   - hello : Print a greeting message
 *   - blink : LED blink placeholder (not implemented on this board)
 *
 * Usage:
 *   Call UART_TEST_RegisterCommands() after FEB_Console_Init() to add
 *   these custom commands to the console.
 *
 ******************************************************************************
 */

#ifndef UART_TEST_COMMANDS_H
#define UART_TEST_COMMANDS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "feb_console.h"

  /* ============================================================================
   * Command Descriptors
   * ============================================================================ */

  extern const FEB_Console_Cmd_t uart_test_cmd_hello;
  extern const FEB_Console_Cmd_t uart_test_cmd_blink;

  /* ============================================================================
   * Registration Function
   * ============================================================================ */

  /**
   * @brief Register all UART_TEST custom commands
   *
   * Call after FEB_Console_Init() to add board-specific commands.
   */
  void UART_TEST_RegisterCommands(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_TEST_COMMANDS_H */
