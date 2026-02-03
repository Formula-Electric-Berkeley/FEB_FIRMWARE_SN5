/**
 ******************************************************************************
 * @file           : feb_console_commands.c
 * @brief          : FEB Console Built-in Command Implementations
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_console_commands.h"
#include "feb_console.h"
#include "feb_uart.h"
#include "main.h"

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static void cmd_echo(int argc, char *argv[]);
static void cmd_help(int argc, char *argv[]);
static void cmd_version(int argc, char *argv[]);
static void cmd_uptime(int argc, char *argv[]);
static void cmd_reboot(int argc, char *argv[]);
static void cmd_log(int argc, char *argv[]);

/* Case-insensitive string comparison (local helper) */
static int strcasecmp_local(const char *a, const char *b);

/* ============================================================================
 * Command Descriptors
 * ============================================================================ */

const FEB_Console_Cmd_t feb_console_cmd_echo = {
    .name = "echo",
    .help = "Print arguments: echo|text to print",
    .handler = cmd_echo,
};

const FEB_Console_Cmd_t feb_console_cmd_help = {
    .name = "help",
    .help = "Show commands: help or help|command",
    .handler = cmd_help,
};

const FEB_Console_Cmd_t feb_console_cmd_version = {
    .name = "version",
    .help = "Show firmware version and build info",
    .handler = cmd_version,
};

const FEB_Console_Cmd_t feb_console_cmd_uptime = {
    .name = "uptime",
    .help = "Show system uptime in milliseconds",
    .handler = cmd_uptime,
};

const FEB_Console_Cmd_t feb_console_cmd_reboot = {
    .name = "reboot",
    .help = "Perform software reset",
    .handler = cmd_reboot,
};

const FEB_Console_Cmd_t feb_console_cmd_log = {
    .name = "log",
    .help = "Set log level: log|error|warn|info|debug|trace",
    .handler = cmd_log,
};

/* ============================================================================
 * Registration Function
 * ============================================================================ */

void FEB_Console_RegisterBuiltins(void)
{
  FEB_Console_Register(&feb_console_cmd_echo);
  FEB_Console_Register(&feb_console_cmd_help);
  FEB_Console_Register(&feb_console_cmd_version);
  FEB_Console_Register(&feb_console_cmd_uptime);
  FEB_Console_Register(&feb_console_cmd_reboot);
  FEB_Console_Register(&feb_console_cmd_log);
}

/* ============================================================================
 * Private Helper Functions
 * ============================================================================ */

/**
 * @brief Case-insensitive string comparison
 */
static int strcasecmp_local(const char *a, const char *b)
{
  while (*a && *b)
  {
    char ca = *a;
    char cb = *b;

    /* Convert to lowercase */
    if (ca >= 'A' && ca <= 'Z')
    {
      ca += 32;
    }
    if (cb >= 'A' && cb <= 'Z')
    {
      cb += 32;
    }

    if (ca != cb)
    {
      return ca - cb;
    }
    a++;
    b++;
  }
  return *a - *b;
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

static void cmd_version(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  FEB_Console_Printf("FEB Firmware\r\n");
  FEB_Console_Printf("Build: %s %s\r\n", __DATE__, __TIME__);
#ifdef __GNUC__
  FEB_Console_Printf("Compiler: GCC %d.%d.%d\r\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
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
  FEB_UART_Flush(100); /* Wait for output to complete */
  NVIC_SystemReset();
}

static void cmd_log(int argc, char *argv[])
{
  if (argc < 2)
  {
    /* Show current level */
    const char *level_names[] = {"none", "error", "warn", "info", "debug", "trace"};
    FEB_UART_LogLevel_t level = FEB_UART_GetLogLevel();
    if (level <= FEB_UART_LOG_TRACE)
    {
      FEB_Console_Printf("Log level: %s\r\n", level_names[level]);
    }
    FEB_Console_Printf("Usage: log|<error|warn|info|debug|trace>\r\n");
    return;
  }

  /* Set log level */
  FEB_UART_LogLevel_t new_level;
  if (strcasecmp_local(argv[1], "error") == 0)
  {
    new_level = FEB_UART_LOG_ERROR;
  }
  else if (strcasecmp_local(argv[1], "warn") == 0)
  {
    new_level = FEB_UART_LOG_WARN;
  }
  else if (strcasecmp_local(argv[1], "info") == 0)
  {
    new_level = FEB_UART_LOG_INFO;
  }
  else if (strcasecmp_local(argv[1], "debug") == 0)
  {
    new_level = FEB_UART_LOG_DEBUG;
  }
  else if (strcasecmp_local(argv[1], "trace") == 0)
  {
    new_level = FEB_UART_LOG_TRACE;
  }
  else if (strcasecmp_local(argv[1], "none") == 0)
  {
    new_level = FEB_UART_LOG_NONE;
  }
  else
  {
    FEB_Console_Printf("Invalid level: %s\r\n", argv[1]);
    FEB_Console_Printf("Valid levels: error, warn, info, debug, trace, none\r\n");
    return;
  }

  FEB_UART_SetLogLevel(new_level);
  FEB_Console_Printf("Log level set to: %s\r\n", argv[1]);
}
