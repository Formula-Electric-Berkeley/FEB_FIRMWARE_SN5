/**
 ******************************************************************************
 * @file           : feb_console.h
 * @brief          : FEB Console Library - Command-line interface
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides a simple command console over UART with:
 *   - Case-insensitive command matching
 *   - Easy command registration
 *   - Built-in commands: echo, help, version, uptime, reboot, log
 *   - Pipe (|) delimited arguments - spaces within args are preserved
 *
 * Command Syntax:
 *   command|arg1|arg2|arg3
 *
 * Examples:
 *   version              - No args needed
 *   echo|hello world     - Prints "hello world" (space preserved)
 *   echo|hello|world     - Prints "hello world" (two args)
 *   log|debug            - Set log level to debug
 *   HELP                 - Commands are case-insensitive
 *
 * Usage:
 *   1. Call FEB_Console_Init() after FEB_UART_Init()
 *   2. Register custom commands with FEB_Console_Register()
 *   3. Connect to UART: FEB_UART_SetRxLineCallback(FEB_Console_ProcessLine)
 *   4. Call FEB_UART_ProcessRx() in main loop
 *
 * Example custom command:
 *   static void cmd_test(int argc, char *argv[]) {
 *     FEB_Console_Printf("Args: %d\r\n", argc);
 *   }
 *
 *   static const FEB_Console_Cmd_t test_cmd = {
 *     .name = "test",
 *     .help = "Test command: test|arg1|arg2",
 *     .handler = cmd_test,
 *   };
 *
 *   FEB_Console_Register(&test_cmd);
 *
 ******************************************************************************
 */

#ifndef FEB_CONSOLE_H
#define FEB_CONSOLE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

  /* ============================================================================
   * Configuration
   * ============================================================================ */

#ifndef FEB_CONSOLE_MAX_COMMANDS
#define FEB_CONSOLE_MAX_COMMANDS 32
#endif

#ifndef FEB_CONSOLE_MAX_ARGS
#define FEB_CONSOLE_MAX_ARGS 16
#endif

#ifndef FEB_CONSOLE_LINE_BUFFER_SIZE
#define FEB_CONSOLE_LINE_BUFFER_SIZE 128
#endif

#ifndef FEB_CONSOLE_PRINTF_BUFFER_SIZE
#define FEB_CONSOLE_PRINTF_BUFFER_SIZE 256
#endif

  /* ============================================================================
   * Types
   * ============================================================================ */

  /**
   * @brief Command handler function signature
   *
   * @param argc Number of arguments (including command name)
   * @param argv Array of argument strings (argv[0] is command name)
   */
  typedef void (*FEB_Console_Handler_t)(int argc, char *argv[]);

  /**
   * @brief Command registration structure
   */
  typedef struct
  {
    const char *name;              /**< Command name (case-insensitive match) */
    const char *help;              /**< Short help text for 'help' command */
    FEB_Console_Handler_t handler; /**< Handler function */
  } FEB_Console_Cmd_t;

  /* ============================================================================
   * Public API
   * ============================================================================ */

  /**
   * @brief Initialize console and register built-in commands
   *
   * Registers: echo, help, version, uptime, reboot, log
   * Call after FEB_UART_Init() but before main loop.
   */
  void FEB_Console_Init(void);

  /**
   * @brief Process a received command line
   *
   * Parses the line into arguments using pipe (|) as delimiter and
   * executes the matching command. Spaces within arguments are preserved.
   * Connect this to FEB_UART_SetRxLineCallback() for automatic processing.
   *
   * @param line Null-terminated command line (without line ending)
   * @param len  Length of line in bytes
   *
   * @note Line is modified during parsing (pipes replaced with nulls)
   * @note Not reentrant - call from single task/context only
   * @warning FreeRTOS: Should only be called from the UART RX processing task
   */
  void FEB_Console_ProcessLine(const char *line, size_t len);

  /**
   * @brief Register a custom command
   *
   * @param cmd Pointer to command descriptor (must remain valid)
   * @return 0 on success, -1 if command table is full
   *
   * @note Call during initialization only, before starting RTOS scheduler
   * @warning Not thread-safe - do not call after FEB_Console_Init() returns
   */
  int FEB_Console_Register(const FEB_Console_Cmd_t *cmd);

  /**
   * @brief Printf-style output to console
   *
   * Wrapper around FEB_UART_Write for console output. Uses stack-allocated
   * buffer for thread-safety.
   *
   * @param fmt Printf format string
   * @param ... Variable arguments
   *
   * @note Thread-safe - can be called from multiple tasks concurrently
   * @note ISR-safe via underlying FEB_UART_Write
   */
  void FEB_Console_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

  /**
   * @brief Get the number of registered commands
   *
   * Useful for iterating over all commands (e.g., for help display).
   *
   * @return Number of registered commands
   */
  size_t FEB_Console_GetCommandCount(void);

  /**
   * @brief Get a command by index
   *
   * @param index Index of the command (0 to GetCommandCount()-1)
   * @return Pointer to command descriptor, or NULL if index out of range
   */
  const FEB_Console_Cmd_t *FEB_Console_GetCommand(size_t index);

  /**
   * @brief Find a command by name (case-insensitive)
   *
   * @param name Command name to search for
   * @return Pointer to command descriptor, or NULL if not found
   */
  const FEB_Console_Cmd_t *FEB_Console_FindCommand(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CONSOLE_H */
