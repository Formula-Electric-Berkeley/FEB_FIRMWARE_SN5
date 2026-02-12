/**
 ******************************************************************************
 * @file           : feb_console.c
 * @brief          : FEB Console Library Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_console.h"
#include "feb_console_commands.h"
#include "feb_uart.h"
#include "main.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Private Variables
 * ============================================================================ */

static const FEB_Console_Cmd_t *commands[FEB_CONSOLE_MAX_COMMANDS];
static size_t command_count = 0;

/* Working buffer for line parsing (copy to avoid modifying const input) */
static char line_buffer[FEB_CONSOLE_LINE_BUFFER_SIZE];

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static int strcasecmp_local(const char *a, const char *b);
static int parse_args(char *line, char *argv[], int max_args);
static const FEB_Console_Cmd_t *find_command(const char *name);

/* ============================================================================
 * Public Functions
 * ============================================================================ */

void FEB_Console_Init(void)
{
  command_count = 0;

  /* Register built-in commands */
  FEB_Console_RegisterBuiltins();
}

void FEB_Console_ProcessLine(const char *line, size_t len)
{
  if (line == NULL || len == 0)
  {
    return;
  }

  /* Copy line to working buffer (we need to modify it for parsing) */
  if (len >= sizeof(line_buffer))
  {
    len = sizeof(line_buffer) - 1;
  }
  memcpy(line_buffer, line, len);
  line_buffer[len] = '\0';

  /* Parse arguments */
  char *argv[FEB_CONSOLE_MAX_ARGS];
  int argc = parse_args(line_buffer, argv, FEB_CONSOLE_MAX_ARGS);

  if (argc == 0)
  {
    return; /* Empty line */
  }

  /* Find and execute command */
  const FEB_Console_Cmd_t *cmd = find_command(argv[0]);
  if (cmd != NULL)
  {
    cmd->handler(argc, argv);
  }
  else
  {
    FEB_Console_Printf("Unknown command: %s\r\n", argv[0]);
    FEB_Console_Printf("Type 'help' for available commands\r\n");
  }
}

int FEB_Console_Register(const FEB_Console_Cmd_t *cmd)
{
  if (cmd == NULL || command_count >= FEB_CONSOLE_MAX_COMMANDS)
  {
    return -1;
  }

  commands[command_count++] = cmd;
  return 0;
}

void FEB_Console_Printf(const char *fmt, ...)
{
  /* Stack-allocated buffer for thread-safety (each call gets its own buffer) */
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
    FEB_UART_Write((const uint8_t *)printf_buf, (size_t)len);
  }
}

size_t FEB_Console_GetCommandCount(void)
{
  return command_count;
}

const FEB_Console_Cmd_t *FEB_Console_GetCommand(size_t index)
{
  if (index >= command_count)
  {
    return NULL;
  }
  return commands[index];
}

const FEB_Console_Cmd_t *FEB_Console_FindCommand(const char *name)
{
  return find_command(name);
}

/* ============================================================================
 * Private Functions
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

/**
 * @brief Parse command line into arguments
 *
 * Splits ONLY on pipe (|) characters. Spaces within arguments are preserved.
 * Example: "echo|hello world|test" -> ["echo", "hello world", "test"]
 * Modifies input string.
 *
 * @param line  Input line (modified in place)
 * @param argv  Output argument array
 * @param max_args Maximum arguments to parse
 * @return Number of arguments parsed
 */
static int parse_args(char *line, char *argv[], int max_args)
{
  int argc = 0;
  char *p = line;
  bool in_arg = false;

  while (*p && argc < max_args)
  {
    /* Pipe is the ONLY delimiter */
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

/**
 * @brief Find command by name (case-insensitive)
 */
static const FEB_Console_Cmd_t *find_command(const char *name)
{
  for (size_t i = 0; i < command_count; i++)
  {
    if (strcasecmp_local(commands[i]->name, name) == 0)
    {
      return commands[i];
    }
  }
  return NULL;
}
