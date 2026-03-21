/**
 ******************************************************************************
 * @file           : feb_console.c
 * @brief          : FEB Console Library Implementation
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#include "feb_console.h"
#include "feb_uart.h"

#include <stdarg.h>
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
 * Private Variables
 * ============================================================================ */

static const FEB_Console_Cmd_t *commands[FEB_CONSOLE_MAX_COMMANDS];
static size_t command_count = 0;
static int console_uart_instance = 0; /* Default to instance 0 */

/* ============================================================================
 * Private Function Prototypes
 * ============================================================================ */

static int strcasecmp_local(const char *a, const char *b);
static int parse_args(char *line, char *argv[], int max_args);
static const FEB_Console_Cmd_t *find_command(const char *name);

/* Forward declaration - implemented in feb_commands library */
extern void FEB_Commands_RegisterSystem(void);

/* ============================================================================
 * Public Functions
 * ============================================================================ */

void FEB_Console_Init(bool register_default_commands)
{
  command_count = 0;

#if FEB_CONSOLE_USE_FREERTOS
  if (console_mutex == NULL)
  {
    console_mutex = osMutexNew(NULL);
  }
#endif

  /* Register built-in commands if requested */
  if (register_default_commands)
  {
    /* Call the Commands library to register system commands */
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

  /* Stack-allocated buffer for reentrancy */
  char local_buffer[FEB_CONSOLE_LINE_BUFFER_SIZE];
  char *argv[FEB_CONSOLE_MAX_ARGS];

  /* Copy line to working buffer (we need to modify it for parsing) */
  if (len >= sizeof(local_buffer))
  {
    len = sizeof(local_buffer) - 1;
  }
  memcpy(local_buffer, line, len);
  local_buffer[len] = '\0';

  /* Parse arguments */
  int argc = parse_args(local_buffer, argv, FEB_CONSOLE_MAX_ARGS);

  if (argc == 0)
  {
    return; /* Empty line */
  }

  /* Find and execute command (thread-safe lookup) */
  CONSOLE_MUTEX_LOCK();
  const FEB_Console_Cmd_t *cmd = find_command(argv[0]);
  CONSOLE_MUTEX_UNLOCK();

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
  if (cmd == NULL)
  {
    return -1;
  }

  /* Validate required fields before acquiring mutex */
  if (cmd->name == NULL || cmd->handler == NULL)
  {
    return -1;
  }

  CONSOLE_MUTEX_LOCK();

  if (command_count >= FEB_CONSOLE_MAX_COMMANDS)
  {
    CONSOLE_MUTEX_UNLOCK();
    return -1;
  }

  commands[command_count++] = cmd;

  CONSOLE_MUTEX_UNLOCK();
  return 0;
}

int FEB_Console_Printf(const char *fmt, ...)
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
    int result = FEB_UART_Write((FEB_UART_Instance_t)console_uart_instance, (const uint8_t *)printf_buf, (size_t)len);
    if (result < 0)
    {
      return result;
    }
  }

  return len;
}

int FEB_Console_Flush(uint32_t timeout_ms)
{
  return FEB_UART_Flush((FEB_UART_Instance_t)console_uart_instance, timeout_ms);
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
 * @note Caller must hold mutex
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
