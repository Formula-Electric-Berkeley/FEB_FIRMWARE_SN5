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
 *   - Pipe (|) delimited arguments - spaces within args are preserved
 *
 * Built-in commands are now in the separate feb_commands library.
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
 * CSV mode (machine-readable output for Chromium/WebSocket clients):
 *   Any command can be invoked in CSV mode by prefixing "csv|".
 *   The library:
 *     - Emits an ACK row immediately upon receipt:
 *         <us_timestamp>,csv_ack,<rest_of_line>
 *     - Calls the command's .csv_handler if registered.
 *     - Emits an error row if the command is unknown or has no csv_handler:
 *         <us_timestamp>,csv_err,unknown,<name>
 *         <us_timestamp>,csv_err,unsupported,<name>
 *   Every row emitted via FEB_Console_CsvPrintf() is prefixed with a
 *   64-bit microsecond timestamp (from feb_time.h) captured at the
 *   moment of the send call.
 *
 *   Examples:
 *     csv|help             - CSV command/description/has_csv rows
 *     csv|version          - CSV-formatted build info
 *     csv|BMS|volts        - BMS voltages as timestamped CSV rows
 *
 * Usage:
 *   1. Call FEB_Console_Init() after FEB_UART_Init()
 *   2. Register commands:
 *      - Use FEB_Commands_RegisterSystem() for built-in commands
 *      - Use FEB_Console_Register() for custom commands
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
#include <stdbool.h>
#include <stdint.h>

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
   *
   * Existing commands using only `name`/`help`/`handler` remain source-
   * compatible; the new `csv_handler` field is optional and defaults to
   * NULL when omitted.
   *
   * The same `help` string is used for both text-mode `help` output and
   * CSV-mode `csv|help` rows. In CSV output the description is wrapped
   * in double quotes so commas are safe; do not put unescaped double
   * quotes inside the help string.
   */
  typedef struct
  {
    const char *name;                  /**< Command name (case-insensitive match) */
    const char *help;                  /**< Short help/description (used in both text and CSV help) */
    FEB_Console_Handler_t handler;     /**< Text-mode handler */
    FEB_Console_Handler_t csv_handler; /**< Optional: CSV-mode handler; NULL = not CSV-capable */
  } FEB_Console_Cmd_t;

  /* ============================================================================
   * Public API
   * ============================================================================ */

  /**
   * @brief Initialize console
   *
   * Call after FEB_UART_Init() but before main loop.
   * Does NOT register built-in commands - use FEB_Commands_RegisterSystem()
   * or pass true to register_default_commands for convenience.
   *
   * @param register_default_commands If true, registers system commands
   *        (echo, help, version, uptime, reboot, log)
   */
  void FEB_Console_Init(bool register_default_commands);

  /**
   * @brief Set the UART instance used for console output
   *
   * @param uart_instance UART instance number (0 = FEB_UART_INSTANCE_1)
   */
  void FEB_Console_SetUartInstance(int uart_instance);

  /**
   * @brief Get the UART instance used for console output
   *
   * @return UART instance number
   */
  int FEB_Console_GetUartInstance(void);

  /**
   * @brief Process a received command line
   *
   * Parses the line into arguments using pipe (|) as delimiter and
   * executes the matching command. Spaces within arguments are preserved.
   * Connect this to FEB_UART_SetRxLineCallback() for automatic processing.
   *
   * Uses stack-allocated buffer for reentrancy.
   *
   * @param line Null-terminated command line (without line ending)
   * @param len  Length of line in bytes
   *
   * @note Reentrant - uses stack-allocated parse buffer
   * @note Thread-safe for command lookup
   */
  void FEB_Console_ProcessLine(const char *line, size_t len);

  /**
   * @brief Register a custom command
   *
   * @param cmd Pointer to command descriptor (must remain valid)
   * @return 0 on success, -1 if command table is full
   *
   * @note Thread-safe when FreeRTOS is enabled
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
   * @return Number of characters written, or negative on error
   *
   * @note Thread-safe - can be called from multiple tasks concurrently
   * @note ISR-safe via underlying FEB_UART_Write
   */
  int FEB_Console_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

  /**
   * @brief Printf-style output to console, CSV mode (timestamped per line)
   *
   * Emits exactly one CSV row with a machine-readable identifier first,
   * followed by a 64-bit microsecond timestamp (FEB_Time_Us() captured
   * at the call site), then the formatted body.
   *
   * Format:
   *   <ident>,<us>,<formatted body>
   *
   * One call = one row = one fresh timestamp - successive rows are
   * individually stamped rather than sharing one batch time. Include the
   * trailing "\r\n" in `fmt`.
   *
   * Example:
   *   FEB_Console_CsvPrintf("bmsV", "%d,%d,%.3f,%.3f\r\n",
   *                         bank+1, cell+1, v_c, v_s);
   *   -> bmsV,123456789,1,1,3.789,3.790
   *
   * @param ident Row identifier (must not contain commas or newlines;
   *              short alphanumeric tag recommended, e.g. "bmsV", "csv_ack")
   * @param fmt   Printf format string for the remaining body fields
   * @return Number of bytes written, or negative on error
   *
   * @note Thread-safe (stack buffer, mutex-free like FEB_Console_Printf).
   * @note Timestamp is captured at the call site, not at UART TX completion.
   *       Gap between stamp and actual byte departure is bounded by DMA
   *       queue depth (typically sub-100 us).
   */
  int FEB_Console_CsvPrintf(const char *ident, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

  /**
   * @brief Flush console output
   *
   * Waits for pending TX data to be transmitted.
   *
   * @param timeout_ms Maximum time to wait in milliseconds
   * @return 0 on success, -1 on timeout
   */
  int FEB_Console_Flush(uint32_t timeout_ms);

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
