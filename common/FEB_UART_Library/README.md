# FEB UART Library

A comprehensive UART/printf/scanf library for STM32 projects with DMA support, FreeRTOS compatibility, and rich logging features.

## Features

- **DMA-based TX/RX**: Non-blocking transmission with ring buffer
- **FreeRTOS-optional**: Works on bare-metal or with FreeRTOS (auto-detected)
- **Printf/scanf redirection**: Override stdio functions automatically
- **Logging macros**: `LOG_E`, `LOG_W`, `LOG_I`, `LOG_D`, `LOG_T`
- **ANSI colors**: Color-coded output for terminal readability
- **Timestamps**: Optional millisecond timestamps
- **Module tags**: `[MAIN]`, `[CAN]`, `[ADC]`, etc. for filtering
- **Command parsing**: Line-based RX with callback

## Quick Start

### 1. Add to CMakeLists.txt

In your board's `CMakeLists.txt`, add one line:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE feb_uart)
```

### 2. Configure CubeMX

Open your `.ioc` file and configure:

#### UART Settings (e.g., USART2)
- Mode: Asynchronous
- Baud Rate: 115200
- Word Length: 8 Bits
- Parity: None
- Stop Bits: 1

#### DMA for TX (Recommended)
1. Go to **Connectivity > USART2 > DMA Settings**
2. Click **Add**
3. Select: `USART2_TX`
4. Direction: `Memory to Peripheral`
5. Mode: `Normal`

#### DMA for RX (Recommended)
1. Click **Add** again
2. Select: `USART2_RX`
3. Direction: `Peripheral to Memory`
4. Mode: **`Circular`** (important!)

#### Enable Interrupt
1. Go to **NVIC Settings**
2. Enable: `USART2 global interrupt`

#### Generate Code

### 3. Initialize in Your Code

In your `FEB_Main.c` or main setup:

```c
#include "feb_uart.h"
#include "feb_uart_log.h"

// Allocate buffers (static ensures they persist)
static uint8_t uart_tx_buf[512];
static uint8_t uart_rx_buf[256];

void FEB_Main_Setup(void)
{
  FEB_UART_Config_t cfg = {
    .huart           = &huart2,
    .hdma_tx         = &hdma_usart2_tx,  // NULL for polling
    .hdma_rx         = &hdma_usart2_rx,  // NULL for polling
    .tx_buffer       = uart_tx_buf,
    .tx_buffer_size  = sizeof(uart_tx_buf),
    .rx_buffer       = uart_rx_buf,
    .rx_buffer_size  = sizeof(uart_rx_buf),
    .log_level       = FEB_UART_LOG_DEBUG,
    .enable_colors   = true,
    .enable_timestamps = true,
    .get_tick_ms     = HAL_GetTick,
  };

  FEB_UART_Init(&cfg);

  LOG_I(TAG_MAIN, "System initialized");
}
```

### 4. Route HAL Callbacks

The library needs to receive HAL callbacks. Add these in your callbacks file or `stm32f4xx_it.c`:

```c
// In stm32f4xx_it.c (USER CODE sections)

#include "feb_uart.h"

// In USART2_IRQHandler:
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */
  FEB_UART_IDLE_Callback(&huart2);  // Call BEFORE HAL handler
  /* USER CODE END USART2_IRQn 0 */

  HAL_UART_IRQHandler(&huart2);
}
```

In a separate callbacks file or `FEB_Callbacks.c`:

```c
#include "feb_uart.h"

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  FEB_UART_TxCpltCallback(huart);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  FEB_UART_RxEventCallback(huart, Size);
}
```

## Usage Examples

### Logging Macros

```c
#include "feb_uart_log.h"

LOG_E(TAG_BMS, "Cell voltage critical: %.3fV", voltage);  // Red, with file:line
LOG_W(TAG_CAN, "TX queue 80%% full");                      // Yellow, with file:line
LOG_I(TAG_MAIN, "System started");                         // Cyan
LOG_D(TAG_ADC, "Raw value: %d (0x%04X)", val, val);       // Magenta
LOG_T(TAG_TPS, "Register 0x%02X = 0x%04X", reg, value);   // Dim/gray

LOG_RAW("╔══════════════════════╗\r\n");                   // No formatting
```

### Standard Printf

```c
// Works automatically after FEB_UART_Init()
printf("Hello, World!\r\n");
printf("Temperature: %.2f C\r\n", temp);
```

### Command Parsing (RX)

```c
void handle_command(const char *line, size_t len)
{
  if (strcmp(line, "status") == 0)
  {
    LOG_I(TAG_MAIN, "Uptime: %lu ms", HAL_GetTick());
  }
  else if (strncmp(line, "log ", 4) == 0)
  {
    int level = atoi(line + 4);
    FEB_UART_SetLogLevel((FEB_UART_LogLevel_t)level);
    LOG_I(TAG_UART, "Log level set to %d", level);
  }
  else
  {
    LOG_W(TAG_UART, "Unknown command: %s", line);
  }
}

void FEB_Main_Setup(void)
{
  // ... init ...
  FEB_UART_SetRxLineCallback(handle_command);
}

void FEB_Main_Loop(void)
{
  FEB_UART_ProcessRx();  // Must call to trigger callbacks
  // ... other code ...
}
```

### Runtime Configuration

```c
// Change log level at runtime
FEB_UART_SetLogLevel(FEB_UART_LOG_WARN);  // Only WARN and ERROR

// Toggle colors
FEB_UART_SetColorsEnabled(false);

// Toggle timestamps
FEB_UART_SetTimestampsEnabled(false);

// Flush before power down
FEB_UART_Flush(1000);  // Wait up to 1 second
```

### Hexdump

```c
uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
LOG_HEXDUMP(TAG_CAN, data, sizeof(data));

// Output:
// [CAN] HEXDUMP (5 bytes):
//   0000: 01 02 03 04 05                                   |.....|
```

## Configuration

### Compile-Time Options

Define these before including headers or in your CMakeLists.txt:

```c
// Log level (0-5): messages above this are compiled out
#define FEB_UART_COMPILE_LOG_LEVEL 4  // DEBUG

// Buffer sizes
#define FEB_UART_DEFAULT_TX_BUFFER_SIZE 512
#define FEB_UART_DEFAULT_RX_BUFFER_SIZE 256
#define FEB_UART_STAGING_BUFFER_SIZE 256

// Force FreeRTOS mode (auto-detected by default)
#define FEB_UART_USE_FREERTOS 1
```

### Log Levels

| Level | Value | Macro | Color | Description |
|-------|-------|-------|-------|-------------|
| NONE  | 0     | -     | -     | No output |
| ERROR | 1     | LOG_E | Red   | Critical errors |
| WARN  | 2     | LOG_W | Yellow| Warnings |
| INFO  | 3     | LOG_I | Cyan  | Status updates |
| DEBUG | 4     | LOG_D | Magenta| Development |
| TRACE | 5     | LOG_T | Gray  | Verbose |

## API Reference

### Initialization

```c
int FEB_UART_Init(const FEB_UART_Config_t *config);
void FEB_UART_DeInit(void);
bool FEB_UART_IsInitialized(void);
```

### Output

```c
int FEB_UART_Printf(const char *format, ...);
int FEB_UART_Write(const uint8_t *data, size_t len);
int FEB_UART_Flush(uint32_t timeout_ms);
size_t FEB_UART_TxPending(void);
```

### Input

```c
void FEB_UART_SetRxLineCallback(FEB_UART_RxLineCallback_t callback);
void FEB_UART_ProcessRx(void);
size_t FEB_UART_RxAvailable(void);
size_t FEB_UART_Read(uint8_t *data, size_t max_len);
```

### Configuration

```c
void FEB_UART_SetLogLevel(FEB_UART_LogLevel_t level);
FEB_UART_LogLevel_t FEB_UART_GetLogLevel(void);
void FEB_UART_SetColorsEnabled(bool enable);
void FEB_UART_SetTimestampsEnabled(bool enable);
```

### HAL Callbacks (user must route)

```c
void FEB_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void FEB_UART_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size);
void FEB_UART_IDLE_Callback(UART_HandleTypeDef *huart);
```

## Troubleshooting

### No output appears
1. Check UART wiring (TX→RX, RX→TX)
2. Verify baud rate matches terminal (115200)
3. Ensure `FEB_UART_Init()` is called before printf
4. Check that DMA handles are correct (or NULL for polling)

### Output is garbled
1. Verify baud rate settings match on both ends
2. Check terminal for correct encoding (UTF-8)
3. Disable colors if terminal doesn't support ANSI

### RX callback not firing
1. Ensure `FEB_UART_ProcessRx()` is called in main loop
2. Verify DMA RX is in **Circular** mode
3. Check that USART interrupt is enabled
4. Verify `FEB_UART_IDLE_Callback()` is called from IRQ handler

### DMA TX seems slow
1. Increase TX buffer size if queue fills up
2. Check that `FEB_UART_TxCpltCallback()` is routed correctly

## FreeRTOS Notes

When FreeRTOS is detected:
- TX uses mutex for thread-safety
- Multiple tasks can safely call printf/LOG_* simultaneously
- `FEB_UART_Flush()` uses `osDelay()` for non-blocking wait
- Safe to call from ISR (uses separate code path)

When bare-metal:
- Critical sections use `__disable_irq()`/`__enable_irq()`
- Single-threaded access assumed

## Files

| File | Purpose |
|------|---------|
| `feb_uart.h` | Public API |
| `feb_uart.c` | Implementation |
| `feb_uart_config.h` | Configuration defaults |
| `feb_uart_log.h` | Logging macros |
| `feb_uart_internal.h` | Internal structures |
| `CMakeLists.txt` | CMake integration |
