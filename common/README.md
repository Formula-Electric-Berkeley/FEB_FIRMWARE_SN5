# FEB Common Libraries

Shared libraries for Formula Electric @ Berkeley firmware projects. All libraries are designed for STM32 with HAL and optional FreeRTOS support.

## Available Libraries

| Library | Target | Description |
|---------|--------|-------------|
| `feb_io` | All I/O | Convenience target: UART + Log + Console + Commands |
| `feb_uart` | UART only | DMA UART with line/binary modes and framing |
| `feb_log` | Logging | Configurable logging with colors and timestamps |
| `feb_console` | Console | Command-line interface with pipe-delimited args |
| `feb_commands` | Commands | Default system commands (help, echo, etc.) |
| `feb_can` | CAN | FreeRTOS-safe CAN with registration/callback pattern |
| `feb_tps` | TPS2482 | Power monitoring IC driver |

## Quick Integration Guide

### Step 1: Add common/ to Your CMake

In your board's `CMakeLists.txt`, ensure common/ is included:

```cmake
# Near the top of your CMakeLists.txt (after project())
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common common)
```

### Step 2: Link Libraries

Choose the libraries you need:

```cmake
# Full I/O stack (recommended for most boards):
target_link_libraries(${PROJECT_NAME} PRIVATE
    feb_io      # UART + Log + Console + Commands
    feb_can     # CAN communication
)

# Or pick specific components:
target_link_libraries(${PROJECT_NAME} PRIVATE
    feb_uart    # Just UART
    feb_log     # Add logging
    feb_can     # Add CAN
)

# Minimal console (no default commands):
target_link_libraries(${PROJECT_NAME} PRIVATE
    feb_console  # Console only (auto-links UART)
)
```

### Step 3: Initialize in FEB_Main.c

```c
#include "feb_uart.h"
#include "feb_log.h"
#include "feb_console.h"
#include "feb_commands.h"
#include "feb_can.h"

#define TAG "MAIN"

// Required: Log output function
static int log_output(const char *data, size_t len) {
    return FEB_UART_Write(FEB_UART_INSTANCE_1, (const uint8_t *)data, len);
}

void FEB_Main(void) {
    // ========================================
    // 1. UART Setup
    // ========================================
    FEB_UART_Config_t uart_cfg = {
        .huart = &huart2,  // Your UART handle
        .rx_mode = FEB_UART_RX_MODE_DMA_IDLE,
    };
    FEB_UART_Init(FEB_UART_INSTANCE_1, &uart_cfg);

    // ========================================
    // 2. Logging Setup (optional but recommended)
    // ========================================
    FEB_Log_Init(log_output, HAL_GetTick, FEB_LOG_INFO);
    FEB_Log_SetColors(true);       // ANSI colors
    FEB_Log_SetTimestamps(true);   // Millisecond timestamps

    // ========================================
    // 3. Console Setup
    // ========================================
    FEB_Console_Init(true);  // true = register default commands
    // Or for custom commands only:
    // FEB_Console_Init(false);
    // FEB_Console_Register(&my_cmd);

    // Connect console to UART
    FEB_UART_SetRxLineCallback(FEB_UART_INSTANCE_1, FEB_Console_ProcessLine);

    // ========================================
    // 4. CAN Setup (if needed)
    // ========================================
    FEB_CAN_Init(&hcan1);

    // ========================================
    // Ready!
    // ========================================
    LOG_I(TAG, "System initialized");

    // Main loop (bare-metal) or start scheduler (FreeRTOS)
    while (1) {
        FEB_UART_ProcessRx(FEB_UART_INSTANCE_1);
        // ... other processing
    }
}
```

## Directory Structure

```
common/
├── CMakeLists.txt              # Aggregates all libraries
├── README.md                   # This file
│
├── FEB_Serial_Library/         # Unified Serial I/O stack
│   ├── CMakeLists.txt          # feb_io convenience target
│   ├── README.md               # Detailed I/O documentation
│   ├── FEB_UART/               # feb_uart
│   ├── FEB_Log/                # feb_log
│   ├── FEB_Console/            # feb_console
│   └── FEB_Commands/           # feb_commands
│
├── FEB_CAN_Library/            # feb_can
│   ├── Inc/
│   ├── Src/
│   └── CMakeLists.txt
│
└── FEB_TPS_Library/            # feb_tps
    ├── Inc/
    ├── Src/
    └── CMakeLists.txt
```

## Library Documentation

### FEB Serial Library

See [`FEB_Serial_Library/README.md`](FEB_Serial_Library/README.md) for detailed documentation on:
- UART line and binary modes
- Binary framing for device-to-device communication
- Logging configuration and usage
- Console command syntax
- Custom command registration

### FEB CAN Library

```c
#include "feb_can.h"

// Initialize
FEB_CAN_Init(&hcan1);

// Register message handler
FEB_CAN_Register(0x100, 0x7FF, my_handler, NULL);

// Send message
FEB_CAN_Transmit(&hcan1, 0x200, data, 8);
```

### FEB TPS Library

```c
#include "feb_tps.h"

// Initialize library
FEB_TPS_LibConfig_t lib_cfg = {
    .log_func = my_log_callback,  // Optional
    .log_level = FEB_TPS_LOG_INFO,
};
FEB_TPS_Init(&lib_cfg);

// Register device (use FEB_TPS_ADDR macro for pin configuration)
FEB_TPS_Handle_t handle;
FEB_TPS_DeviceConfig_t cfg = {
    .hi2c = &hi2c1,
    .i2c_addr = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND),
    .r_shunt_ohms = 0.002f,
    .i_max_amps = 10.0f,
};
FEB_TPS_DeviceRegister(&cfg, &handle);

// Read measurements
float voltage = FEB_TPS_GetVoltage(handle);
float current = FEB_TPS_GetCurrent(handle);
float power = FEB_TPS_GetPower(handle);
```

## FreeRTOS Support

All libraries automatically detect FreeRTOS via standard configuration macros:
- `configUSE_MUTEXES`
- `INCLUDE_xSemaphoreGetMutexHolder`
- `USE_FREERTOS`

When FreeRTOS is detected:
- Mutexes protect shared resources
- Logging uses mutex for thread-safety
- CAN uses message queues

**Note:** While most libraries auto-configure, the CAN library requires board-provided
mutexes and message queues to be passed via `FEB_CAN_Config_t`. See existing board
implementations (e.g., DASH, BMS) for examples of proper CAN initialization.

## Compile-Time Configuration

Override defaults in your board's CMakeLists.txt:

```cmake
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # UART
    FEB_UART_MAX_INSTANCES=2
    FEB_UART_RX_BUFFER_SIZE=256
    FEB_UART_TX_BUFFER_SIZE=512

    # Logging
    FEB_LOG_COMPILE_LEVEL=4        # 0=NONE, 1=ERROR, ..., 5=TRACE
    FEB_LOG_STAGING_BUFFER_SIZE=512

    # Console
    FEB_CONSOLE_MAX_COMMANDS=32
    FEB_CONSOLE_MAX_ARGS=16
)
```

## Adding a New Board Project

1. **Create project directory** with STM32CubeMX-generated code

2. **Add CMakeLists.txt** based on existing boards (e.g., PCU):
   ```cmake
   cmake_minimum_required(VERSION 3.22)
   project(MY_BOARD C ASM)

   # ... STM32 toolchain setup ...

   # Include common libraries
   add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common common)

   # Link what you need
   target_link_libraries(${PROJECT_NAME} PRIVATE
       feb_io
       feb_can
   )
   ```

3. **Create FEB_Main.c** in `Core/User/Src/`:
   ```c
   #include "feb_uart.h"
   #include "feb_log.h"
   #include "feb_console.h"

   void FEB_Main(void) {
       // Initialize and run
   }
   ```

4. **Call FEB_Main()** from `main.c` after HAL initialization

## Troubleshooting

### "undefined reference to FEB_..."
- Ensure `add_subdirectory(common)` is in your CMakeLists.txt
- Ensure `target_link_libraries()` includes the needed library

### "file not found" in IDE
- IDE may not parse CMake include paths correctly
- Code will compile correctly with CMake
- Rebuild CMake cache in IDE if needed

### Console not responding
- Check UART is initialized before console
- Verify `FEB_UART_ProcessRx()` is called in main loop
- Confirm UART instance matches between init and callback

### Logs not appearing
- Verify `FEB_Log_Init()` was called
- Check log level (`FEB_Log_SetLevel()`)
- Ensure output function routes to correct UART
