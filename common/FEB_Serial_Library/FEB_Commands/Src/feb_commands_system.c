/**
 ******************************************************************************
 * @file           : feb_commands_system.c
 * @brief          : FEB System Command Implementations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 * @details
 *
 * Implements default system commands:
 *   - echo   : Print arguments
 *   - help   : Show available commands
 *   - version: Show firmware build info
 *   - uptime : Show system uptime
 *   - reboot : Perform software reset
 *   - log    : Get/set log level
 *
 ******************************************************************************
 */

#include "feb_commands.h"
#include "feb_commands_system.h"
#include "feb_console.h"
#include "feb_log.h"
#include "feb_string_utils.h"
#include "feb_version.h"

#include <string.h>

/* HAL for HAL_GetTick and NVIC_SystemReset */
#include "main.h"

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void cmd_echo(int argc, char *argv[]);
static void cmd_help(int argc, char *argv[]);
static void cmd_hello(int argc, char *argv[]);
static void cmd_version(int argc, char *argv[]);
static void cmd_uptime(int argc, char *argv[]);
static void cmd_reboot(int argc, char *argv[]);
static void cmd_log(int argc, char *argv[]);

static void cmd_echo_csv(int argc, char *argv[]);
static void cmd_help_csv(int argc, char *argv[]);
static void cmd_hello_csv(int argc, char *argv[]);
static void cmd_version_csv(int argc, char *argv[]);
static void cmd_uptime_csv(int argc, char *argv[]);
static void cmd_reboot_csv(int argc, char *argv[]);
static void cmd_log_csv(int argc, char *argv[]);

/* ============================================================================
 * Command Descriptors
 * ============================================================================ */

const FEB_Console_Cmd_t feb_cmd_echo = {
    .name = "echo",
    .help = "Print arguments: echo|text to print",
    .handler = cmd_echo,
    .csv_handler = cmd_echo_csv,
};

const FEB_Console_Cmd_t feb_cmd_help = {
    .name = "help",
    .help = "Show commands: help or help|command",
    .handler = cmd_help,
    .csv_handler = cmd_help_csv,
};

const FEB_Console_Cmd_t feb_cmd_hello = {
    .name = "hello",
    .help = "Say hello from FEB",
    .handler = cmd_hello,
    .csv_handler = cmd_hello_csv,
};

const FEB_Console_Cmd_t feb_cmd_version = {
    .name = "version",
    .help = "Show firmware version, commit, build time, and flash time (use csv|version for CSV)",
    .handler = cmd_version,
    .csv_handler = cmd_version_csv,
};

const FEB_Console_Cmd_t feb_cmd_uptime = {
    .name = "uptime",
    .help = "Show system uptime in milliseconds",
    .handler = cmd_uptime,
    .csv_handler = cmd_uptime_csv,
};

const FEB_Console_Cmd_t feb_cmd_reboot = {
    .name = "reboot",
    .help = "Perform software reset",
    .handler = cmd_reboot,
    .csv_handler = cmd_reboot_csv,
};

const FEB_Console_Cmd_t feb_cmd_log = {
    .name = "log",
    .help = "Set log level: log|none|error|warn|info|debug|trace",
    .handler = cmd_log,
    .csv_handler = cmd_log_csv,
};

/* ============================================================================
 * Registration Function
 * ============================================================================ */

void FEB_Commands_RegisterSystem(void)
{
  FEB_Console_Register(&feb_cmd_echo);
  FEB_Console_Register(&feb_cmd_help);
  FEB_Console_Register(&feb_cmd_hello);
  FEB_Console_Register(&feb_cmd_version);
  FEB_Console_Register(&feb_cmd_uptime);
  FEB_Console_Register(&feb_cmd_reboot);
  FEB_Console_Register(&feb_cmd_log);
}

/* ============================================================================
 * Command Handlers
 * ============================================================================ */

static void cmd_echo(int argc, char *argv[])
{
  /* Print all arguments after "echo", separated by spaces */
  for (int i = 1; i < argc; i++)
  {
    if (i > 1)
    {
      FEB_Console_Printf(" ");
    }
    FEB_Console_Printf("%s", argv[i]);
  }
  FEB_Console_Printf("\r\n");
}

static void cmd_help(int argc, char *argv[])
{
  if (argc >= 2)
  {
    /* Help for specific command */
    const FEB_Console_Cmd_t *cmd = FEB_Console_FindCommand(argv[1]);
    if (cmd != NULL)
    {
      FEB_Console_Printf("%s: %s\r\n", cmd->name, cmd->help);
    }
    else
    {
      FEB_Console_Printf("Unknown command: %s\r\n", argv[1]);
    }
    return;
  }

  /* List all commands */
  FEB_Console_Printf("Available commands (use | as delimiter):\r\n");
  FEB_Console_Printf("  Example: echo|hello world\r\n");
  FEB_Console_Printf("  Example: log|debug\r\n\r\n");

  size_t count = FEB_Console_GetCommandCount();
  for (size_t i = 0; i < count; i++)
  {
    const FEB_Console_Cmd_t *cmd = FEB_Console_GetCommand(i);
    if (cmd != NULL)
    {
      FEB_Console_Printf("  %-12s %s\r\n", cmd->name, cmd->help);
    }
  }
}

static void cmd_hello(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_Console_Printf("Hello from FEB!\r\n");
}

/* Copy a volatile char array into a local buffer for safe string use.
 * strncpy from a volatile is UB; this loop is portable and cheap. */
static void copy_volatile_string(char *dst, const volatile char *src, size_t n)
{
  size_t i;
  for (i = 0; i + 1 < n && src[i] != '\0'; i++)
  {
    dst[i] = src[i];
  }
  dst[i] = '\0';
}

static void cmd_version(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  char flash_utc[sizeof(feb_flash_info.flash_utc)];
  char flasher_user[sizeof(feb_flash_info.flasher_user)];
  char flasher_host[sizeof(feb_flash_info.flasher_host)];
  copy_volatile_string(flash_utc, feb_flash_info.flash_utc, sizeof(flash_utc));
  copy_volatile_string(flasher_user, feb_flash_info.flasher_user, sizeof(flasher_user));
  copy_volatile_string(flasher_host, feb_flash_info.flasher_host, sizeof(flasher_host));

  const bool unflashed = FEB_Version_IsUnflashed();

  FEB_Console_Printf("=== FEB Firmware ===\r\n");
  FEB_Console_Printf("  Board   : %s\r\n", feb_build_info.board_name);
  FEB_Console_Printf("  Version : %s (board) | %s (repo) | %s (common)\r\n", feb_build_info.version_string,
                     feb_build_info.repo_version_string, feb_build_info.common_version_string);
  FEB_Console_Printf("  Commit  : %s (%s)%s\r\n", feb_build_info.commit_short, feb_build_info.branch,
                     feb_build_info.dirty ? " [DIRTY]" : "");
  FEB_Console_Printf("  SHA     : %s\r\n", feb_build_info.commit_full);
  FEB_Console_Printf("  Built   : %s by %s@%s\r\n", feb_build_info.build_utc, feb_build_info.build_user,
                     feb_build_info.build_host);
  if (unflashed)
  {
    FEB_Console_Printf("  Flashed : (unflashed - programmed without flash-patcher)\r\n");
  }
  else
  {
    FEB_Console_Printf("  Flashed : %s by %s@%s\r\n", flash_utc, flasher_user, flasher_host);
  }
#ifdef __GNUC__
  FEB_Console_Printf("  Compiler: GCC %d.%d.%d\r\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
}

static void cmd_uptime(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  uint32_t ms = HAL_GetTick();
  uint32_t sec = ms / 1000;
  uint32_t min = sec / 60;
  uint32_t hr = min / 60;

  FEB_Console_Printf("Uptime: %lu ms (%lu:%02lu:%02lu)\r\n", (unsigned long)ms, (unsigned long)hr,
                     (unsigned long)(min % 60), (unsigned long)(sec % 60));
}

static void cmd_reboot(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("Rebooting...\r\n");

  /* Flush console output before reset */
  FEB_Console_Flush(100);

  NVIC_SystemReset();
}

static void cmd_log(int argc, char *argv[])
{
  if (argc < 2)
  {
    /* Show current level */
    const char *level_names[] = {"none", "error", "warn", "info", "debug", "trace"};
    FEB_Log_Level_t level = FEB_Log_GetLevel();
    if (level <= FEB_LOG_TRACE)
    {
      FEB_Console_Printf("Log level: %s\r\n", level_names[level]);
    }
    FEB_Console_Printf("Usage: log|<e|w|i|d|t|n>\r\n");
    return;
  }

  /* Set log level */
  FEB_Log_Level_t new_level;
  if (FEB_strcasecmp(argv[1], "error") == 0 || FEB_strcasecmp(argv[1], "e") == 0)
  {
    new_level = FEB_LOG_ERROR;
  }
  else if (FEB_strcasecmp(argv[1], "warn") == 0 || FEB_strcasecmp(argv[1], "w") == 0)
  {
    new_level = FEB_LOG_WARN;
  }
  else if (FEB_strcasecmp(argv[1], "info") == 0 || FEB_strcasecmp(argv[1], "i") == 0)
  {
    new_level = FEB_LOG_INFO;
  }
  else if (FEB_strcasecmp(argv[1], "debug") == 0 || FEB_strcasecmp(argv[1], "d") == 0)
  {
    new_level = FEB_LOG_DEBUG;
  }
  else if (FEB_strcasecmp(argv[1], "trace") == 0 || FEB_strcasecmp(argv[1], "t") == 0)
  {
    new_level = FEB_LOG_TRACE;
  }
  else if (FEB_strcasecmp(argv[1], "none") == 0 || FEB_strcasecmp(argv[1], "n") == 0)
  {
    new_level = FEB_LOG_NONE;
  }
  else
  {
    FEB_Console_Printf("Invalid level: %s\r\n", argv[1]);
    FEB_Console_Printf("Valid: error(e), warn(w), info(i), debug(d), trace(t), none(n)\r\n");
    return;
  }

  const char *level_names[] = {"none", "error", "warn", "info", "debug", "trace"};
  FEB_Log_SetLevel(new_level);
  FEB_Console_Printf("Log level set to: %s\r\n", level_names[new_level]);
}

/* ============================================================================
 * CSV-Mode Handlers
 * ============================================================================
 *
 * Rows emitted by FEB_Console_CsvPrintf have the shape
 *   <ident>,<us_timestamp>,<body>\r\n
 * so each handler only supplies the body fields.
 */

static const char *log_level_name(FEB_Log_Level_t level)
{
  static const char *const names[] = {"none", "error", "warn", "info", "debug", "trace"};
  if ((unsigned)level >= (sizeof(names) / sizeof(names[0])))
  {
    return "unknown";
  }
  return names[level];
}

static void cmd_echo_csv(int argc, char *argv[])
{
  /* Join argv[1..] with single spaces into a single body field.
   * A stack buffer is sized to match FEB_CONSOLE_LINE_BUFFER_SIZE. */
  char joined[FEB_CONSOLE_LINE_BUFFER_SIZE];
  size_t pos = 0;
  for (int i = 1; i < argc && pos + 1 < sizeof(joined); i++)
  {
    if (i > 1 && pos + 1 < sizeof(joined))
    {
      joined[pos++] = ' ';
    }
    const char *a = argv[i];
    while (*a && pos + 1 < sizeof(joined))
    {
      joined[pos++] = *a++;
    }
  }
  joined[pos] = '\0';
  FEB_Console_CsvPrintf("echo", "%s\r\n", joined);
}

static void cmd_help_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  /* Header row so clients can self-describe the columns. */
  FEB_Console_CsvPrintf("csv_help", "header,name,description,has_csv\r\n");

  size_t count = FEB_Console_GetCommandCount();
  for (size_t i = 0; i < count; i++)
  {
    const FEB_Console_Cmd_t *cmd = FEB_Console_GetCommand(i);
    if (cmd == NULL)
    {
      continue;
    }
    const char *desc = (cmd->help != NULL) ? cmd->help : "";
    int has_csv = (cmd->csv_handler != NULL) ? 1 : 0;
    FEB_Console_CsvPrintf("csv_help", "row,%s,\"%s\",%d\r\n", cmd->name, desc, has_csv);
  }
}

static void cmd_hello_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_Console_CsvPrintf("hello", "ok\r\n");
}

static void cmd_version_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  char flash_utc[sizeof(feb_flash_info.flash_utc)];
  char flasher_user[sizeof(feb_flash_info.flasher_user)];
  char flasher_host[sizeof(feb_flash_info.flasher_host)];
  copy_volatile_string(flash_utc, feb_flash_info.flash_utc, sizeof(flash_utc));
  copy_volatile_string(flasher_user, feb_flash_info.flasher_user, sizeof(flasher_user));
  copy_volatile_string(flasher_host, feb_flash_info.flasher_host, sizeof(flasher_host));

  /* Single CSV row. Field order is stable and documented here - host-side
   * tooling matches on position. Add new fields only at the END.
   *
   * Columns after <ident>,<us_timestamp>:
   *   board,board_ver,repo_ver,common_ver,commit,branch,dirty,
   *   build_utc,build_user,build_host,flash_utc,flasher_user,flasher_host
   */
  FEB_Console_CsvPrintf("version", "%s,%s,%s,%s,%s,%s,%d,%s,%s,%s,%s,%s,%s\r\n", feb_build_info.board_name,
                        feb_build_info.version_string, feb_build_info.repo_version_string,
                        feb_build_info.common_version_string, feb_build_info.commit_short, feb_build_info.branch,
                        feb_build_info.dirty ? 1 : 0, feb_build_info.build_utc, feb_build_info.build_user,
                        feb_build_info.build_host, flash_utc, flasher_user, flasher_host);
}

static void cmd_uptime_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  uint32_t ms = HAL_GetTick();
  uint32_t sec = ms / 1000;
  uint32_t min = sec / 60;
  uint32_t hr = min / 60;
  FEB_Console_CsvPrintf("uptime", "%lu,%lu,%lu,%lu\r\n", (unsigned long)ms, (unsigned long)hr,
                        (unsigned long)(min % 60), (unsigned long)(sec % 60));
}

static void cmd_reboot_csv(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  FEB_Console_CsvPrintf("reboot", "ok\r\n");
  FEB_Console_Flush(100);
  NVIC_SystemReset();
}

static void cmd_log_csv(int argc, char *argv[])
{
  if (argc < 2)
  {
    FEB_Console_CsvPrintf("log", "level,%s\r\n", log_level_name(FEB_Log_GetLevel()));
    return;
  }

  FEB_Log_Level_t new_level;
  if (FEB_strcasecmp(argv[1], "error") == 0 || FEB_strcasecmp(argv[1], "e") == 0)
  {
    new_level = FEB_LOG_ERROR;
  }
  else if (FEB_strcasecmp(argv[1], "warn") == 0 || FEB_strcasecmp(argv[1], "w") == 0)
  {
    new_level = FEB_LOG_WARN;
  }
  else if (FEB_strcasecmp(argv[1], "info") == 0 || FEB_strcasecmp(argv[1], "i") == 0)
  {
    new_level = FEB_LOG_INFO;
  }
  else if (FEB_strcasecmp(argv[1], "debug") == 0 || FEB_strcasecmp(argv[1], "d") == 0)
  {
    new_level = FEB_LOG_DEBUG;
  }
  else if (FEB_strcasecmp(argv[1], "trace") == 0 || FEB_strcasecmp(argv[1], "t") == 0)
  {
    new_level = FEB_LOG_TRACE;
  }
  else if (FEB_strcasecmp(argv[1], "none") == 0 || FEB_strcasecmp(argv[1], "n") == 0)
  {
    new_level = FEB_LOG_NONE;
  }
  else
  {
    FEB_Console_CsvPrintf("log", "err,invalid,%s\r\n", argv[1]);
    return;
  }

  FEB_Log_SetLevel(new_level);
  FEB_Console_CsvPrintf("log", "set,%s,ok\r\n", log_level_name(new_level));
}
