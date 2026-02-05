# FEB Console Library

A simple command-line interface for UART console with case-insensitive commands.

## Features

- Case-insensitive command matching (`echo` = `ECHO` = `Echo`)
- Easy command registration via static descriptors
- Built-in commands: `echo`, `help`, `version`, `uptime`, `reboot`, `log`
- Space or `|` as argument delimiters
- Works with any STM32 MCU (F4, U5, etc.)

## Built-in Commands

| Command | Description | Example |
|---------|-------------|---------|
| `echo <text>` | Print arguments back | `echo Hello World!` |
| `help [cmd]` | List all commands or help for specific | `help echo` |
| `version` | Show firmware build info | `version` |
| `uptime` | Show system uptime | `uptime` |
| `reboot` | Software reset | `reboot` |
| `log <level>` | Set log level | `log debug` |

## Usage

### Setup

```c
#include "feb_uart.h"
#include "feb_console.h"

void FEB_Main_Setup(void) {
  // Initialize UART first
  FEB_UART_Init(&uart_cfg);

  // Initialize console (registers built-in commands)
  FEB_Console_Init();

  // Connect UART RX to console
  FEB_UART_SetRxLineCallback(FEB_Console_ProcessLine);
}

void FEB_Main_Loop(void) {
  FEB_UART_ProcessRx();  // Process incoming commands
}
```

### Adding Custom Commands

```c
// 1. Define handler function
static void cmd_mycommand(int argc, char *argv[]) {
  if (argc < 2) {
    FEB_Console_Printf("Usage: mycommand <arg>\r\n");
    return;
  }
  FEB_Console_Printf("Argument: %s\r\n", argv[1]);
}

// 2. Create command descriptor (must be static or global)
static const FEB_Console_Cmd_t mycommand = {
  .name = "mycommand",     // Case-insensitive
  .help = "Does something with an argument",
  .handler = cmd_mycommand,
};

// 3. Register in setup
FEB_Console_Register(&mycommand);
```

## API Reference

### `void FEB_Console_Init(void)`
Initialize the console and register built-in commands. Call after `FEB_UART_Init()`.

### `void FEB_Console_ProcessLine(const char *line, size_t len)`
Process a received command line. Connect to `FEB_UART_SetRxLineCallback()`.

### `int FEB_Console_Register(const FEB_Console_Cmd_t *cmd)`
Register a custom command. Returns 0 on success, -1 if command table is full.

### `void FEB_Console_Printf(const char *fmt, ...)`
Printf-style output to the console.

## Configuration

Define before including `feb_console.h`:

```c
#define FEB_CONSOLE_MAX_COMMANDS 32       // Maximum registered commands
#define FEB_CONSOLE_MAX_ARGS 16           // Maximum arguments per command
#define FEB_CONSOLE_LINE_BUFFER_SIZE 128  // Command line buffer size
#define FEB_CONSOLE_PRINTF_BUFFER_SIZE 256 // Printf formatting buffer
```

## Thread-Safety

| Function | Thread-Safe | Notes |
|----------|-------------|-------|
| `FEB_Console_Printf()` | Yes | Uses stack buffer, ISR-safe via UART |
| `FEB_Console_ProcessLine()` | No | Single-task only (called from UART RX task) |
| `FEB_Console_Register()` | No | Call during initialization only |

**Important:**
- `FEB_Console_ProcessLine()` should only be called from the task that processes UART RX
- Register all commands during initialization before starting the RTOS scheduler

## CMake Integration

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE feb_console)
```

The console library automatically links `feb_uart`.
