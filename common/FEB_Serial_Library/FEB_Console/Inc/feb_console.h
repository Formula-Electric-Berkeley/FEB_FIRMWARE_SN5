/**
 ******************************************************************************
 * @file           : feb_console.h
 * @brief          : FEB Console Library - Command-line interface
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Provides a serial console with two coexisting transport modes:
 *
 *   1. Text mode (human). Pipe-delimited, case-insensitive, preserved from
 *      the original FEB CLI:
 *          command|arg1|arg2
 *
 *   2. CSV mode (machine). Formal transactional protocol used by the host /
 *      website integration:
 *          <board_name>|csv|<tx_id>|<command>[|<arg>...]
 *
 *      A request that uses `*` as <board_name> is a broadcast — every board
 *      on the link responds. Otherwise only the board whose
 *      feb_build_info.board_name (case-insensitive) matches the first token
 *      responds; other boards silently drop the line.
 *
 *      Every CSV response row follows a single shape:
 *          csv,<tx_id>,<board_name>,<us_timestamp>,<response_type>[,<body>]\r\n
 *
 *      <us_timestamp> is 64-bit microseconds since boot (FEB_Time_Us()).
 *
 *      Standard <response_type> values:
 *          ack    - request received (auto, no body)
 *          done   - request complete (auto, no body)
 *          error  - error,<level>,<description>  (level: error|warn|info)
 *          log    - log,<text>
 *          command- command,<name>,<description> (used by `commands`)
 *      Plus any command-specific type (voltage, temp, status, version, ...).
 *
 * Command Syntax examples:
 *   version                          (text)
 *   log|debug                        (text)
 *   BMS|csv|c001|cell-stats          (csv, targeted)
 *   *|csv|scan01|hello               (csv, broadcast)
 *
 * tx_id rules (enforced by the parser):
 *   - At most FEB_CSV_TX_ID_MAX_LEN bytes.
 *   - Must be non-empty and contain no commas, pipes, CR, or LF
 *     (those would break CSV framing).
 *   - Otherwise opaque to the board; the host chooses the format.
 *
 * Usage:
 *   1. FEB_UART_Init(...);
 *   2. FEB_Console_Init(true);   // registers system commands
 *   3. <Board>_RegisterCommands();
 *   4. FEB_UART_SetRxLineCallback(FEB_Console_ProcessLine);
 *   5. FEB_UART_ProcessRx(...) in the RX task.
 *
 * Registering a command (dual handler pattern):
 *   static void do_thing(int argc, char *argv[]) {
 *     FEB_Console_Printf("hello\r\n");
 *   }
 *   static void do_thing_csv(int argc, char *argv[]) {
 *     FEB_Console_CsvEmit("hello", "");   // ack + done are automatic
 *   }
 *   static const FEB_Console_Cmd_t cmd = {
 *     .name = "thing", .help = "do the thing",
 *     .handler = do_thing, .csv_handler = do_thing_csv,
 *   };
 *   FEB_Console_Register(&cmd);
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
#define FEB_CONSOLE_MAX_COMMANDS 48
#endif

#ifndef FEB_CONSOLE_MAX_ARGS
#define FEB_CONSOLE_MAX_ARGS 16
#endif

#ifndef FEB_CONSOLE_LINE_BUFFER_SIZE
#define FEB_CONSOLE_LINE_BUFFER_SIZE 192
#endif

#ifndef FEB_CONSOLE_PRINTF_BUFFER_SIZE
#define FEB_CONSOLE_PRINTF_BUFFER_SIZE 320
#endif

#ifndef FEB_CSV_TX_ID_MAX_LEN
#define FEB_CSV_TX_ID_MAX_LEN 32
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
   * Same struct services both text and CSV modes. Either handler may be
   * NULL:
   *   - handler == NULL, csv_handler != NULL: CSV-only command
   *   - handler != NULL, csv_handler == NULL: text-only command (CSV clients
   *     get `error,error,unsupported`)
   *   - both set: full dual mode
   */
  typedef struct
  {
    const char *name;                  /**< Command name (case-insensitive match) */
    const char *help;                  /**< Short human description; also emitted by `commands` */
    FEB_Console_Handler_t handler;     /**< Text-mode handler (may be NULL for CSV-only) */
    FEB_Console_Handler_t csv_handler; /**< CSV-mode handler (may be NULL; client gets unsupported) */
  } FEB_Console_Cmd_t;

  /* ============================================================================
   * Public API
   * ============================================================================ */

  /**
   * @brief Initialize console
   *
   * Call after FEB_UART_Init() but before the main loop.
   *
   * @param register_default_commands If true, registers system commands
   *        (echo, help, hello, commands, version, uptime, reboot, log)
   */
  void FEB_Console_Init(bool register_default_commands);

  /**
   * @brief Set the UART instance used for console output
   * @param uart_instance UART instance number (0 = FEB_UART_INSTANCE_1)
   */
  void FEB_Console_SetUartInstance(int uart_instance);

  /**
   * @brief Get the UART instance used for console output
   */
  int FEB_Console_GetUartInstance(void);

  /**
   * @brief Process a received command line
   *
   * Parses the line using pipe (|) as delimiter and dispatches to either
   * text or CSV mode depending on whether the second token is `csv`.
   * CSV mode enforces the board-name gate, validates the tx_id, and wraps
   * the handler call with auto-emitted ack / done rows.
   *
   * Must be invoked from a single RX task (today: uartRxTask). The CSV
   * transaction state (tx_id, in-transaction flag) is process-wide, so
   * recursive / concurrent calls would corrupt it.
   *
   * @param line Null-terminated command line (without line ending)
   * @param len  Length of line in bytes
   */
  void FEB_Console_ProcessLine(const char *line, size_t len);

  /**
   * @brief Register a custom command
   * @return 0 on success, -1 if command table is full, -2 on duplicate name
   */
  int FEB_Console_Register(const FEB_Console_Cmd_t *cmd);

  /**
   * @brief Printf-style output to console (text mode)
   *
   * Thread-safe via stack-allocated buffer. Use in text-mode handlers only;
   * mixing raw Printf output into a CSV transaction will corrupt the stream.
   */
  int FEB_Console_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

  /**
   * @brief Emit one CSV protocol row.
   *
   * Row layout:
   *   csv,<tx_id>,<board_name>,<us>,<response_type>[,<body>]\r\n
   *
   * Only valid inside a CSV command handler. The library captures tx_id,
   * board_name, and timestamp; `fmt` supplies only the optional body.
   * If `fmt` is NULL or empty the row ends `...,<response_type>\r\n` with
   * no trailing comma. Do not include \r\n in `fmt` — the library appends
   * it.
   *
   * Example:
   *   FEB_Console_CsvEmit("voltage", "%d,%d,%.3f,%.3f",
   *                       module, cell, v_primary, v_secondary);
   *
   * @return bytes written on success, negative on error (incl. called
   *         outside a CSV transaction).
   */
  int FEB_Console_CsvEmit(const char *response_type, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

  /**
   * @brief Emit a CSV error row.
   *
   * Convenience for FEB_Console_CsvEmit("error", "<level>,<description>"):
   *   csv,<tx>,<board>,<us>,error,<level>,<description>\r\n
   * `level` is one of "error", "warn", "info" (free-form; the host treats
   * it as an advisory severity). `fmt` supplies the description body and
   * may contain additional comma-separated fields.
   */
  int FEB_Console_CsvError(const char *level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

  /**
   * @brief Emit a CSV log row.
   *
   * Convenience for FEB_Console_CsvEmit("log", ...):
   *   csv,<tx>,<board>,<us>,log,<text>\r\n
   * Use from within a handler to attach advisory log output to the
   * transaction. Does NOT route through the FEB_Log facility.
   */
  int FEB_Console_CsvLog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

  /**
   * @brief Emit a CSV row with an explicit tx_id, bypassing the current
   *        transaction state.
   *
   * Intended for asynchronous callbacks that complete AFTER the original
   * command handler has returned and the dispatcher has emitted `done`.
   * The caller captures the tx_id during the original handler and invokes
   * this later from a different context. No ack/done framing — just a
   * single correlated row. Row shape is otherwise identical to CsvEmit.
   *
   * Hosts correlate by tx_id. Rows arriving after the transactional `done`
   * are a documented extension; handlers that can run synchronously should
   * prefer CsvEmit.
   */
  int FEB_Console_CsvEmitAs(const char *tx_id, const char *response_type, const char *fmt, ...)
      __attribute__((format(printf, 3, 4)));

  /**
   * @brief Snapshot the tx_id of the current CSV transaction.
   *
   * Returns false if called outside a CSV transaction or if `out` is NULL
   * or `cap` is too small. On success, writes a null-terminated copy of
   * the tx_id to `out`. Use before queueing async work so the completion
   * callback can emit with FEB_Console_CsvEmitAs().
   */
  bool FEB_Console_CsvCurrentTxId(char *out, size_t cap);

  /**
   * @brief Flush console output (waits for pending TX)
   * @return 0 on success, -1 on timeout
   */
  int FEB_Console_Flush(uint32_t timeout_ms);

  /**
   * @brief Get the number of registered commands
   */
  size_t FEB_Console_GetCommandCount(void);

  /**
   * @brief Get a command by index (0 to GetCommandCount()-1)
   */
  const FEB_Console_Cmd_t *FEB_Console_GetCommand(size_t index);

  /**
   * @brief Find a command by name (case-insensitive)
   */
  const FEB_Console_Cmd_t *FEB_Console_FindCommand(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* FEB_CONSOLE_H */
