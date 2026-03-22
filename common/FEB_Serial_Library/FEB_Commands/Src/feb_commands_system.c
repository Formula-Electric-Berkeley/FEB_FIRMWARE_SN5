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

/* ============================================================================
 * Command Descriptors
 * ============================================================================ */

const FEB_Console_Cmd_t feb_cmd_echo = {
    .name = "echo",
    .help = "Print arguments: echo|text to print",
    .handler = cmd_echo,
};

const FEB_Console_Cmd_t feb_cmd_help = {
    .name = "help",
    .help = "Show commands: help or help|command",
    .handler = cmd_help,
};

const FEB_Console_Cmd_t feb_cmd_hello = {
    .name = "hello",
    .help = "Say hello from FEB",
    .handler = cmd_hello,
};

const FEB_Console_Cmd_t feb_cmd_version = {
    .name = "version",
    .help = "Show firmware version and build info",
    .handler = cmd_version,
};

const FEB_Console_Cmd_t feb_cmd_uptime = {
    .name = "uptime",
    .help = "Show system uptime in milliseconds",
    .handler = cmd_uptime,
};

const FEB_Console_Cmd_t feb_cmd_reboot = {
    .name = "reboot",
    .help = "Perform software reset",
    .handler = cmd_reboot,
};

const FEB_Console_Cmd_t feb_cmd_log = {
    .name = "log",
    .help = "Set log level: log|none|error|warn|info|debug|trace",
    .handler = cmd_log,
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
