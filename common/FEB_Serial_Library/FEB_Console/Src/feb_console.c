/**
 ******************************************************************************
 * @file           : feb_console.c
 * @brief          : FEB Console Library Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 *
 * Protocol split:
 *   Text mode: `<cmd>|<args>`                              (unchanged CLI)
 *   CSV mode:  `<board_name>|csv|<tx_id>|<cmd>|<args>`     (new spec)
 *
 * CSV responses are rows of the form:
 *   csv,<tx_id>,<board_name>,<us_timestamp>,<response_type>[,<body>]\r\n
 *
 * The dispatcher auto-emits `ack` on receipt and `done` after the handler
 * returns. Handlers use FEB_Console_CsvEmit/CsvError/CsvLog for body rows.
 *
 ******************************************************************************
 */

#include "feb_console.h"
#include "feb_string_utils.h"
#include "feb_time.h"
#include "feb_uart.h"
#include "feb_version.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * FreeRTOS Detection
 * ============================================================================ */

#ifndef FEB_CONSOLE_USE_FREERTOS
#if ((defined(INCLUDE_xSemaphoreGetMutexHolder) && (INCLUDE_xSemaphoreGetMutexHolder != 0)) ||                         \
     (defined(configUSE_MUTEXES) && (configUSE_MUTEXES != 0)) || (defined(USE_FREERTOS) && (USE_FREERTOS != 0)))
#define FEB_CONSOLE_USE_FREERTOS 1
#else
#define FEB_CONSOLE_USE_FREERTOS 0
#endif
#endif

#if FEB_CONSOLE_USE_FREERTOS
#include "FreeRTOS.h"
#include "cmsis_os2.h"
static osMutexId_t console_mutex = NULL;
#define CONSOLE_MUTEX_LOCK()                                                                                           \
  do                                                                                                                   \
  {                                                                                                                    \
    if (console_mutex != NULL)                                                                                         \
      osMutexAcquire(console_mutex, osWaitForever);                                                                    \
  } while (0)
#define CONSOLE_MUTEX_UNLOCK()                                                                                         \
  do                                                                                                                   \
  {                                                                                                                    \
    if (console_mutex != NULL)                                                                                         \
      osMutexRelease(console_mutex);                                                                                   \
  } while (0)
#else
#define CONSOLE_MUTEX_LOCK() ((void)0)
#define CONSOLE_MUTEX_UNLOCK() ((void)0)
#endif

/* ============================================================================
 * Private State
 * ============================================================================ */

static const FEB_Console_Cmd_t *commands[FEB_CONSOLE_MAX_COMMANDS];
static size_t command_count = 0;
static int console_uart_instance = 0;

/* CSV transaction state. Set only by FEB_Console_ProcessLine on the single
 * UART RX task, read by CsvEmit/CsvError/CsvLog from inside handlers on the
 * same task. Not guarded — guarding with the console mutex would deadlock
 * against handlers that register commands (e.g., `commands` listing uses
 * GetCommand which takes the same mutex). Single-threadedness by construction
 * is what keeps this safe. */
static char csv_current_tx_id[FEB_CSV_TX_ID_MAX_LEN + 1];
static bool csv_in_transaction;

/* ============================================================================
 * Private Prototypes
 * ============================================================================ */

static int parse_args(char *line, char *argv[], int max_args);
static const FEB_Console_Cmd_t *find_command(const char *name);
static size_t u64_to_decimal(uint64_t v, char *out, size_t cap);
static bool tx_id_is_valid(const char *s);
static bool board_matches(const char *addr);
static int csv_emit_v(const char *response_type, const char *fmt, va_list ap);

/* Forward declaration - implemented in feb_commands library */
extern void FEB_Commands_RegisterSystem(void);

/* ============================================================================
 * Public Functions
 * ============================================================================ */

void FEB_Console_Init(bool register_default_commands)
{
  command_count = 0;
  csv_in_transaction = false;
  csv_current_tx_id[0] = '\0';

#if FEB_CONSOLE_USE_FREERTOS
  if (console_mutex == NULL)
  {
    console_mutex = osMutexNew(NULL);
  }
#endif

  FEB_Time_Init();

  if (register_default_commands)
  {
    FEB_Commands_RegisterSystem();
  }
}

void FEB_Console_SetUartInstance(int uart_instance)
{
  console_uart_instance = uart_instance;
}

int FEB_Console_GetUartInstance(void)
{
  return console_uart_instance;
}

void FEB_Console_ProcessLine(const char *line, size_t len)
{
  if (line == NULL || len == 0)
  {
    return;
  }

  /* Parse the line into a local argv. Stack-allocated for reentrancy of
   * the buffer itself; CSV transaction state is still single-threaded. */
  char local_buffer[FEB_CONSOLE_LINE_BUFFER_SIZE];
  char *argv[FEB_CONSOLE_MAX_ARGS];

  if (len >= sizeof(local_buffer))
  {
    len = sizeof(local_buffer) - 1;
  }
  memcpy(local_buffer, line, len);
  local_buffer[len] = '\0';

  int argc = parse_args(local_buffer, argv, FEB_CONSOLE_MAX_ARGS);
  if (argc == 0)
  {
    return;
  }

  /* Detect the CSV framing: <board>|csv|<tx_id>|<cmd>[|<args>...] */
  const bool csv_mode = (argc >= 2) && (FEB_strcasecmp(argv[1], "csv") == 0);

  if (!csv_mode)
  {
    /* Text mode — unchanged from the legacy CLI. */
    CONSOLE_MUTEX_LOCK();
    const FEB_Console_Cmd_t *cmd = find_command(argv[0]);
    CONSOLE_MUTEX_UNLOCK();

    if (cmd != NULL && cmd->handler != NULL)
    {
      cmd->handler(argc, argv);
    }
    else if (cmd != NULL)
    {
      FEB_Console_Printf("Command '%s' is CSV-only. Use: <board>|csv|<tx_id>|%s\r\n", cmd->name, cmd->name);
    }
    else
    {
      FEB_Console_Printf("Unknown command: %s\r\n", argv[0]);
      FEB_Console_Printf("Type 'help' for available commands\r\n");
    }
    return;
  }

  /* -------- CSV mode -------- */

  /* Board gate: silently drop lines not addressed to this board. Wildcard
   * `*` matches every board. */
  if (!board_matches(argv[0]))
  {
    return;
  }

  /* Malformed: need at least <board>|csv|<tx_id>|<cmd>. Missing tx_id means
   * we have nothing to echo, so use "-" and emit without ack/done. */
  if (argc < 4)
  {
    csv_in_transaction = true;
    strcpy(csv_current_tx_id, "-");
    FEB_Console_CsvError("error", "malformed");
    csv_in_transaction = false;
    csv_current_tx_id[0] = '\0';
    return;
  }

  const char *tx_id = argv[2];
  const char *cmd_name = argv[3];

  /* tx_id validation. On failure, emit an error row with tx_id echoed as
   * "-" (we cannot safely quote arbitrary bytes into the tx_id column). */
  if (!tx_id_is_valid(tx_id))
  {
    csv_in_transaction = true;
    strcpy(csv_current_tx_id, "-");
    FEB_Console_CsvError("error", "invalid_tx_id");
    csv_in_transaction = false;
    csv_current_tx_id[0] = '\0';
    return;
  }

  /* Enter transaction */
  strncpy(csv_current_tx_id, tx_id, FEB_CSV_TX_ID_MAX_LEN);
  csv_current_tx_id[FEB_CSV_TX_ID_MAX_LEN] = '\0';
  csv_in_transaction = true;

  /* Auto-ack */
  FEB_Console_CsvEmit("ack", NULL);

  /* Command lookup */
  CONSOLE_MUTEX_LOCK();
  const FEB_Console_Cmd_t *cmd = find_command(cmd_name);
  CONSOLE_MUTEX_UNLOCK();

  if (cmd == NULL)
  {
    FEB_Console_CsvError("error", "unknown_command,%s", cmd_name);
  }
  else if (cmd->csv_handler == NULL)
  {
    FEB_Console_CsvError("error", "unsupported,%s", cmd->name);
  }
  else
  {
    /* Hand the handler an argv slice beginning at the command name so its
     * argv[0] is the command and argv[1..] are the args — same convention
     * text handlers already use. */
    cmd->csv_handler(argc - 3, argv + 3);
  }

  /* Auto-done (always emitted, including after error rows). */
  FEB_Console_CsvEmit("done", NULL);

  csv_in_transaction = false;
  csv_current_tx_id[0] = '\0';
}

int FEB_Console_Register(const FEB_Console_Cmd_t *cmd)
{
  if (cmd == NULL || cmd->name == NULL)
  {
    return -1;
  }
  /* At least one handler must be set, otherwise the entry is useless. */
  if (cmd->handler == NULL && cmd->csv_handler == NULL)
  {
    return -1;
  }

  CONSOLE_MUTEX_LOCK();

  if (command_count >= FEB_CONSOLE_MAX_COMMANDS)
  {
    CONSOLE_MUTEX_UNLOCK();
    return -1;
  }

  for (size_t i = 0; i < command_count; i++)
  {
    if (FEB_strcasecmp(commands[i]->name, cmd->name) == 0)
    {
      CONSOLE_MUTEX_UNLOCK();
      return -2;
    }
  }

  commands[command_count++] = cmd;

  CONSOLE_MUTEX_UNLOCK();
  return 0;
}

int FEB_Console_Printf(const char *fmt, ...)
{
  char printf_buf[FEB_CONSOLE_PRINTF_BUFFER_SIZE];

  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(printf_buf, sizeof(printf_buf), fmt, args);
  va_end(args);

  if (len > 0)
  {
    if ((size_t)len >= sizeof(printf_buf))
    {
      len = sizeof(printf_buf) - 1;
    }
    int result = FEB_UART_Write((FEB_UART_Instance_t)console_uart_instance, (const uint8_t *)printf_buf, (size_t)len);
    if (result < 0)
    {
      return result;
    }
  }

  return len;
}

int FEB_Console_CsvEmit(const char *response_type, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int r = csv_emit_v(response_type, fmt, ap);
  va_end(ap);
  return r;
}

int FEB_Console_CsvError(const char *level, const char *fmt, ...)
{
  if (level == NULL)
  {
    level = "error";
  }

  /* Compose "<level>,<body>" into a stack buffer so we can dispatch through
   * the single workhorse. Cheaper than a second emit + snprintf dance. */
  char body[FEB_CONSOLE_PRINTF_BUFFER_SIZE];
  int used = snprintf(body, sizeof(body), "%s", level);
  if (used < 0)
  {
    return used;
  }
  if ((size_t)used >= sizeof(body))
  {
    used = (int)sizeof(body) - 1;
  }

  if (fmt != NULL && fmt[0] != '\0' && (size_t)used < sizeof(body) - 1)
  {
    body[used++] = ',';
    va_list ap;
    va_start(ap, fmt);
    int extra = vsnprintf(body + used, sizeof(body) - (size_t)used, fmt, ap);
    va_end(ap);
    if (extra < 0)
    {
      return extra;
    }
    if ((size_t)(used + extra) >= sizeof(body))
    {
      body[sizeof(body) - 1] = '\0';
    }
  }

  /* Emit through the single workhorse. `body` already contains the full
   * text; pass it as the sole %s arg so no format interpretation happens
   * against user-supplied bytes. */
  return FEB_Console_CsvEmit("error", "%s", body);
}

int FEB_Console_CsvLog(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int r = csv_emit_v("log", fmt, ap);
  va_end(ap);
  return r;
}

bool FEB_Console_CsvCurrentTxId(char *out, size_t cap)
{
  if (out == NULL || cap == 0)
  {
    return false;
  }
  if (!csv_in_transaction)
  {
    out[0] = '\0';
    return false;
  }
  size_t n = strlen(csv_current_tx_id);
  if (n + 1 > cap)
  {
    out[0] = '\0';
    return false;
  }
  memcpy(out, csv_current_tx_id, n + 1);
  return true;
}

int FEB_Console_CsvEmitAs(const char *tx_id, const char *response_type, const char *fmt, ...)
{
  if (tx_id == NULL || tx_id[0] == '\0')
  {
    return -1;
  }
  if (response_type == NULL)
  {
    response_type = "";
  }

  uint64_t us = FEB_Time_Us();
  char us_str[24];
  u64_to_decimal(us, us_str, sizeof(us_str));

  const char *board_name = (feb_build_info.board_name != NULL) ? feb_build_info.board_name : "?";

  char buf[FEB_CONSOLE_PRINTF_BUFFER_SIZE];
  int pre = snprintf(buf, sizeof(buf), "csv,%s,%s,%s,%s", tx_id, board_name, us_str, response_type);
  if (pre < 0)
  {
    return pre;
  }
  if ((size_t)pre >= sizeof(buf))
  {
    pre = (int)sizeof(buf) - 1;
  }

  int body = 0;
  if (fmt != NULL && fmt[0] != '\0')
  {
    if ((size_t)pre < sizeof(buf) - 1)
    {
      buf[pre++] = ',';
    }
    va_list ap;
    va_start(ap, fmt);
    body = vsnprintf(buf + pre, sizeof(buf) - (size_t)pre, fmt, ap);
    va_end(ap);
    if (body < 0)
    {
      return body;
    }
    if ((size_t)(pre + body) >= sizeof(buf))
    {
      body = (int)sizeof(buf) - 1 - pre;
    }
  }

  int total = pre + body;
  if (total > (int)sizeof(buf) - 2)
  {
    total = (int)sizeof(buf) - 2;
  }
  buf[total++] = '\r';
  buf[total++] = '\n';

  return FEB_UART_Write((FEB_UART_Instance_t)console_uart_instance, (const uint8_t *)buf, (size_t)total);
}

int FEB_Console_Flush(uint32_t timeout_ms)
{
  return FEB_UART_Flush((FEB_UART_Instance_t)console_uart_instance, timeout_ms);
}

size_t FEB_Console_GetCommandCount(void)
{
  CONSOLE_MUTEX_LOCK();
  size_t count = command_count;
  CONSOLE_MUTEX_UNLOCK();
  return count;
}

const FEB_Console_Cmd_t *FEB_Console_GetCommand(size_t index)
{
  CONSOLE_MUTEX_LOCK();
  const FEB_Console_Cmd_t *cmd = NULL;
  if (index < command_count)
  {
    cmd = commands[index];
  }
  CONSOLE_MUTEX_UNLOCK();
  return cmd;
}

const FEB_Console_Cmd_t *FEB_Console_FindCommand(const char *name)
{
  if (name == NULL)
  {
    return NULL;
  }
  CONSOLE_MUTEX_LOCK();
  const FEB_Console_Cmd_t *cmd = find_command(name);
  CONSOLE_MUTEX_UNLOCK();
  return cmd;
}

/* ============================================================================
 * Private Functions
 * ============================================================================ */

/* Splits ONLY on pipe (|) characters. Spaces within arguments are preserved.
 * Modifies input string in place. */
static int parse_args(char *line, char *argv[], int max_args)
{
  int argc = 0;
  char *p = line;
  bool in_arg = false;

  while (*p && argc < max_args)
  {
    if (*p == '|')
    {
      *p = '\0';
      in_arg = false;
    }
    else
    {
      if (!in_arg)
      {
        argv[argc++] = p;
        in_arg = true;
      }
    }
    p++;
  }

  return argc;
}

static const FEB_Console_Cmd_t *find_command(const char *name)
{
  for (size_t i = 0; i < command_count; i++)
  {
    if (FEB_strcasecmp(commands[i]->name, name) == 0)
    {
      return commands[i];
    }
  }
  return NULL;
}

/* newlib-nano printf drops %llu, so format the uint64 manually. */
static size_t u64_to_decimal(uint64_t v, char *out, size_t cap)
{
  if (cap == 0)
  {
    return 0;
  }
  char tmp[21];
  size_t i = 0;
  if (v == 0)
  {
    tmp[i++] = '0';
  }
  while (v > 0)
  {
    tmp[i++] = (char)('0' + (v % 10));
    v /= 10;
  }
  size_t n = (i < cap - 1) ? i : (cap - 1);
  for (size_t j = 0; j < n; j++)
  {
    out[j] = tmp[i - 1 - j];
  }
  out[n] = '\0';
  return n;
}

static bool tx_id_is_valid(const char *s)
{
  if (s == NULL || s[0] == '\0')
  {
    return false;
  }
  size_t n = 0;
  for (const char *p = s; *p; p++, n++)
  {
    if (n >= FEB_CSV_TX_ID_MAX_LEN)
    {
      return false;
    }
    char c = *p;
    if (c == ',' || c == '|' || c == '\r' || c == '\n')
    {
      return false;
    }
  }
  return true;
}

static bool board_matches(const char *addr)
{
  if (addr == NULL)
  {
    return false;
  }
  if (addr[0] == '*' && addr[1] == '\0')
  {
    return true;
  }
  if (feb_build_info.board_name == NULL)
  {
    return false;
  }
  return FEB_strcasecmp(addr, feb_build_info.board_name) == 0;
}

/* One CSV row → one FEB_UART_Write call → atomic at the UART layer. */
static int csv_emit_v(const char *response_type, const char *fmt, va_list ap)
{
  if (!csv_in_transaction)
  {
    return -1;
  }
  if (response_type == NULL)
  {
    response_type = "";
  }

  uint64_t us = FEB_Time_Us();
  char us_str[24];
  u64_to_decimal(us, us_str, sizeof(us_str));

  const char *board_name = (feb_build_info.board_name != NULL) ? feb_build_info.board_name : "?";

  char buf[FEB_CONSOLE_PRINTF_BUFFER_SIZE];
  int pre = snprintf(buf, sizeof(buf), "csv,%s,%s,%s,%s", csv_current_tx_id, board_name, us_str, response_type);
  if (pre < 0)
  {
    return pre;
  }
  if ((size_t)pre >= sizeof(buf))
  {
    pre = (int)sizeof(buf) - 1;
  }

  int body = 0;
  if (fmt != NULL && fmt[0] != '\0')
  {
    if ((size_t)pre < sizeof(buf) - 1)
    {
      buf[pre++] = ',';
    }
    body = vsnprintf(buf + pre, sizeof(buf) - (size_t)pre, fmt, ap);
    if (body < 0)
    {
      return body;
    }
    if ((size_t)(pre + body) >= sizeof(buf))
    {
      body = (int)sizeof(buf) - 1 - pre;
    }
  }

  int total = pre + body;
  /* Reserve space for "\r\n". If we're right at the edge, clamp. */
  if (total > (int)sizeof(buf) - 2)
  {
    total = (int)sizeof(buf) - 2;
  }
  buf[total++] = '\r';
  buf[total++] = '\n';

  int result = FEB_UART_Write((FEB_UART_Instance_t)console_uart_instance, (const uint8_t *)buf, (size_t)total);
  if (result < 0)
  {
    return result;
  }
  return total;
}
