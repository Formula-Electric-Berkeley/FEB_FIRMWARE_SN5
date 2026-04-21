# FEB Serial Library

Unified serial I/O stack for Formula Electric @ Berkeley firmware projects. Provides UART communication, logging, and command-line interface with modular architecture.

## Architecture

```
FEB_Serial_Library/
├── FEB_UART/         # Core UART driver (DMA, line/binary modes, framing)
├── FEB_Log/          # Logging with configurable output backend
├── FEB_Console/      # Command-line interface
├── FEB_Commands/     # Default system commands (help, echo, etc.)
├── FEB_String_Utils/ # Case-insensitive comparison and small string helpers
├── FEB_Version/      # Build provenance types (FEB_Build_Info_t, FEB_Flash_Info_t)
└── CMakeLists.txt    # Aggregates all libraries
```

### CMake Targets

| Target | Role |
|---|---|
| `feb_uart` | DMA UART driver (line + binary modes). Standalone. |
| `feb_log` | Logging with injectable output. Depends on `feb_uart`. |
| `feb_console` | Pipe-delimited command parser. Depends on `feb_uart`, `feb_time`. |
| `feb_commands` | Default `help`, `echo`, `version`, `uptime`, `reboot`, `log`. Depends on `feb_console`, `feb_log`, `feb_version` (used by `version`). |
| `feb_string_utils` | `FEB_strcasecmp()` and friends. Standalone. |
| `feb_version` | Build provenance types; paired with `cmake/FEB_Version.cmake` which generates `feb_build_info.c` per board. |
| `feb_io` | Umbrella: links `feb_string_utils`, [`feb_time`](../FEB_Time_Library/README.md), `feb_uart`, `feb_log`, `feb_console`, `feb_version`, `feb_commands`. |

Linking `feb_io` pulls in [`feb_time`](../FEB_Time_Library/README.md) as a transitive dependency — console CSV rows and log timestamps rely on it.

## Quick Start

### CMake Integration

Add to your board's `CMakeLists.txt`:

```cmake
# Full I/O stack (recommended):
target_link_libraries(${PROJECT_NAME} PRIVATE feb_io)

# Or pick specific components:
target_link_libraries(${PROJECT_NAME} PRIVATE feb_uart)          # UART only
target_link_libraries(${PROJECT_NAME} PRIVATE feb_uart feb_log)  # UART + Logging
target_link_libraries(${PROJECT_NAME} PRIVATE feb_console)       # Console (auto-links UART)
```

### Basic Console Setup

```c
#include "feb_uart.h"
#include "feb_log.h"
#include "feb_console.h"
#include "feb_commands.h"

#define TAG "MAIN"

// Log output wrapper
static int log_output(const char *data, size_t len) {
    return FEB_UART_Write(FEB_UART_INSTANCE_1, (const uint8_t *)data, len);
}

void FEB_Main(void) {
    // 1. Initialize UART
    FEB_UART_Config_t uart_cfg = {
        .huart = &huart2,
        .rx_mode = FEB_UART_RX_MODE_DMA_IDLE,
    };
    FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg);

    // 2. Initialize logging (routes to UART)
    FEB_Log_Init(log_output, HAL_GetTick, FEB_LOG_INFO);
    FEB_Log_SetColors(true);
    FEB_Log_SetTimestamps(true);

    // 3. Initialize console with default commands
    FEB_Console_Init(true);
    FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

    LOG_I(TAG, "System initialized");

    // Main loop
    while (1) {
        FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);
    }
}
```

---

## UART Library

Core UART driver with DMA support, multiple modes, and binary framing.

### Features

- **Line Mode**: Callbacks on newline (for console)
- **Binary Mode**: Raw byte callbacks with idle timeout
- **Framing**: HDLC-style delimiters with byte stuffing
- **DMA**: Circular RX buffer, ring buffer TX
- **Thread-safe**: ISR and RTOS safe

### Line Mode (Console)

```c
#include "feb_uart.h"

void on_line(const char *line, size_t len) {
    // Process complete line
}

void setup(void) {
    FEB_UART_Config_t cfg = {
        .huart = &huart2,
        .rx_mode = FEB_UART_RX_MODE_DMA_IDLE,
    };
    FEB_UART_Init(FEB_UART_INSTANCE_1, &cfg);
    FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, on_line);
}

void loop(void) {
    FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);
}
```

### Binary Mode (Device-to-Device)

```c
#include "feb_uart.h"

void on_frame(FEB_UART_Instance_t inst, const uint8_t *data, size_t len) {
    // Process complete frame (delimiters stripped, unescaped)
}

void setup(void) {
    FEB_UART_Config_t cfg = {
        .huart = &huart1,
        .rx_mode = FEB_UART_RX_MODE_DMA_IDLE,
    };
    FEB_UART_Init(FEB_UART_INSTANCE_1, &cfg);

    // Switch to binary mode
    FEB_UART_SetMode(FEB_UART_INSTANCE_1, FEB_UART_MODE_BINARY);

    // Configure HDLC-style framing
    FEB_UART_FramingConfig_t framing = {
        .enable_framing = true,
        .start_delimiter = 0x7E,
        .end_delimiter = 0x7E,
        .escape_enabled = true,
        .escape_char = 0x7D,
        .max_frame_size = 256,
    };
    FEB_UART_SetFramingConfig(FEB_UART_INSTANCE_1, &framing);

    // Register callback (min_bytes=0, idle_timeout=10ms)
    FEB_UART_SetRxBinaryCallback(FEB_UART_INSTANCE_1, on_frame, 0, 10);
}

void send_frame(const uint8_t *data, size_t len) {
    // Automatically adds framing delimiters and escaping
    FEB_UART_WriteBinary(FEB_UART_INSTANCE_1, data, len, true);
}
```

### API Reference

| Function | Description |
|----------|-------------|
| `FEB_UART_Init()` | Initialize UART instance |
| `FEB_UART_Write()` | Write data (ISR-safe) |
| `FEB_UART_ProcessRx()` | Process received data (call in main loop) |
| `FEB_UART_SetRxLineCallback()` | Register line-mode callback |
| `FEB_UART_SetMode()` | Set line/binary mode |
| `FEB_UART_SetFramingConfig()` | Configure binary framing |
| `FEB_UART_SetRxBinaryCallback()` | Register binary-mode callback |
| `FEB_UART_WriteBinary()` | Write with optional framing |
| `FEB_UART_Flush()` | Wait for TX completion |

---

## Log Library

Standalone logging with configurable output backend, colors, and timestamps.

### Features

- **Configurable output**: Route logs anywhere (UART, USB, etc.)
- **Log levels**: ERROR, WARN, INFO, DEBUG, TRACE
- **Compile-time filtering**: Zero overhead for disabled levels
- **Runtime filtering**: Adjust levels without recompile
- **ANSI colors**: Optional colored output
- **Timestamps**: Optional millisecond timestamps
- **Thread-safe**: Mutex protection with FreeRTOS

### Basic Usage

```c
#include "feb_log.h"

#define TAG "MYMODULE"

// Output function (you provide this)
static int my_output(const char *data, size_t len) {
    return FEB_UART_Write(FEB_UART_INSTANCE_1, (const uint8_t *)data, len);
}

void setup(void) {
    FEB_Log_Init(my_output, HAL_GetTick, FEB_LOG_INFO);
    FEB_Log_SetColors(true);
    FEB_Log_SetTimestamps(true);
}

void example(void) {
    LOG_E(TAG, "Error: %s", "something broke");
    LOG_W(TAG, "Warning: value=%d", 42);
    LOG_I(TAG, "Info message");
    LOG_D(TAG, "Debug details");
    LOG_T(TAG, "Trace data");
}
```

### Output Example

```
[00012345] E MYMODULE: Error: something broke (main.c:42)
[00012346] W MYMODULE: Warning: value=42
[00012347] I MYMODULE: Info message
```

### Compile-Time Configuration

In your board's CMakeLists.txt or compiler flags:

```cmake
# Set maximum compile-time log level (0=NONE, 1=ERROR, ..., 5=TRACE)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    FEB_LOG_COMPILE_LEVEL=4  # Include up to DEBUG
)
```

---

## Console Library

Command-line interface with pipe-delimited arguments.

### Features

- **Case-insensitive**: Commands match regardless of case
- **Pipe delimiters**: Spaces preserved within arguments
- **Thread-safe**: Mutex-protected command registration
- **Reentrant**: Stack-allocated parse buffers

### Command Syntax

```
command|arg1|arg2|arg3
```

Examples:
```
help                    # No arguments
echo|hello world        # Prints "hello world" (space preserved)
echo|hello|world        # Prints "hello world" (two arguments)
log|debug               # Set log level to debug
```

### Registering Custom Commands

```c
#include "feb_console.h"

static void cmd_mytest(int argc, char *argv[]) {
    FEB_Console_Printf("Got %d arguments\r\n", argc);
    for (int i = 0; i < argc; i++) {
        FEB_Console_Printf("  [%d] %s\r\n", i, argv[i]);
    }
}

static const FEB_Console_Cmd_t my_cmd = {
    .name = "mytest",
    .help = "Test command: mytest|arg1|arg2",
    .handler = cmd_mytest,
};

void setup(void) {
    FEB_Console_Init(false);  // No default commands
    FEB_Console_Register(&my_cmd);
}
```

---

## Commands Library

Default system commands. Depends on Console and Log libraries.

### Built-in Commands

| Command | Description |
|---------|-------------|
| `help` | List all registered commands |
| `echo\|text` | Print text to console |
| `version` | Show firmware version |
| `uptime` | Show system uptime |
| `reboot` | Soft reset the system |
| `log\|level` | Set log level (error/warn/info/debug/trace) |

### Selective Registration

```c
#include "feb_commands.h"
#include "feb_commands_system.h"

void setup(void) {
    FEB_Console_Init(false);

    // Register all system commands:
    FEB_Commands_RegisterSystem();

    // Or register individually:
    FEB_Console_Register(&feb_cmd_help);
    FEB_Console_Register(&feb_cmd_echo);
    FEB_Console_Register(&feb_cmd_version);
}
```

---

## Thread Safety

| Library | ISR-Safe | RTOS-Safe | Notes |
|---------|----------|-----------|-------|
| UART | Yes | Yes | Ring buffers, atomic operations |
| Log | No | Yes | Mutex-protected with FreeRTOS |
| Console | Partial | Yes | ProcessLine is reentrant, Register is mutex-protected |
| Commands | No | Yes | Uses Log for output |

---

## Configuration Options

### Compile-Time Defines

| Define | Default | Description |
|--------|---------|-------------|
| `FEB_UART_MAX_INSTANCES` | 2 | Maximum UART instances |
| `FEB_UART_RX_BUFFER_SIZE` | 256 | RX DMA buffer size |
| `FEB_UART_TX_BUFFER_SIZE` | 512 | TX ring buffer size |
| `FEB_LOG_COMPILE_LEVEL` | 4 (DEBUG) | Maximum compile-time log level |
| `FEB_LOG_STAGING_BUFFER_SIZE` | 512 | Log message buffer size |
| `FEB_CONSOLE_MAX_COMMANDS` | 32 | Maximum registered commands |
| `FEB_CONSOLE_MAX_ARGS` | 16 | Maximum arguments per command |
| `FEB_CONSOLE_LINE_BUFFER_SIZE` | 128 | Command line buffer size |

### FreeRTOS Detection

All libraries automatically detect FreeRTOS via:
- `configUSE_MUTEXES`
- `INCLUDE_xSemaphoreGetMutexHolder`
- `USE_FREERTOS`

No manual configuration required.
