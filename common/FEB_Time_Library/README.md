# FEB Time Library

64-bit microsecond monotonic clock. Backed by the DWT cycle counter on Cortex-M3+ (all F446RE boards and F469 DASH), with a `HAL_GetTick() + SysTick->VAL` fallback on Cortex-M0 (DART). The API is identical on both paths.

## CMake Target

INTERFACE library with a single translation unit. Add to your board's `CMakeLists.txt`:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE feb_time)
```

Pulled in transitively by `feb_io`, so any board already linking `feb_io` has `feb_time` available.

## Public API

| Function | Description |
|---|---|
| `FEB_Time_Init()` | One-time init. Enables DWT on Cortex-M3+; captures `SystemCoreClock`. Idempotent. |
| `FEB_Time_Us()` | 64-bit monotonic microseconds since `FEB_Time_Init()`. Thread- and ISR-safe. |
| `FEB_Time_Us32()` | 32-bit variant. Wraps every ~71 minutes. Cheaper at call sites that don't need 64-bit range. |
| `FEB_Time_OnSysTick()` | Optional wrap-catcher for idle systems — see **Wrap handling** below. |

See [`Inc/feb_time.h`](Inc/feb_time.h) for the full contract.

## Dependencies

- STM32 HAL (`HAL_GetTick`, `SystemCoreClock`, `SysTick`).
- No FreeRTOS requirement. Works identically bare-metal or under an RTOS.
- No other `feb_*` libraries.

## Configuration

None.

## Initialization

Call once after `SystemInit()` has finished setting `SystemCoreClock` to its final value — in practice, from `FEB_Main`, early `main()`, or the equivalent board init function.

```c
#include "feb_time.h"

void FEB_Main(void)
{
    FEB_Time_Init();

    uint64_t t0 = FEB_Time_Us();
    do_work();
    uint64_t elapsed_us = FEB_Time_Us() - t0;
}
```

## Wrap Handling

On Cortex-M3+, `DWT->CYCCNT` is 32-bit and wraps every `(2^32) / SystemCoreClock` seconds (≈23.86 s at 180 MHz, ≈25.77 s at 168 MHz). `FEB_Time_Us()` carries a 64-bit accumulator and detects wraps on each call, so as long as it's called at least once per wrap window the result is exact.

In practice:

- If the console is emitting CSV rows or logs at any rate, `FEB_Time_Us()` is polled many times per second and wraps are always caught.
- If the application is **fully idle** for > ~20 s (no logs, no console, no periodic CAN), wire `FEB_Time_OnSysTick()` into a periodic hook that runs at least once every wrap window:

  ```c
  // In SysTick_Handler, FreeRTOS tick hook, or any timer callback >= 0.05 Hz:
  FEB_Time_OnSysTick();
  ```

On Cortex-M0 the backend is `HAL_GetTick() + SysTick->VAL` and there is no 32-bit wrap to worry about — the 64-bit accumulator advances directly from the millisecond tick.

## Platform Notes

| Platform | Backend | Resolution |
|---|---|---|
| Cortex-M4 (F446RE: BMS, DCU, LVPDB, PCU, Sensor_Nodes, UART) | DWT CYCCNT | ~5–6 ns |
| Cortex-M4 with FPU (F469 DASH) | DWT CYCCNT | ~5–6 ns |
| Cortex-M33 (U575 UART_TEST) | DWT CYCCNT | ~6 ns |
| Cortex-M0 (F042 DART) | `HAL_GetTick() + SysTick->VAL` | sub-millisecond |

## Example: Timing a Critical Section

```c
#include "feb_time.h"

void measure_spi_transfer(void)
{
    uint64_t t0 = FEB_Time_Us();
    HAL_SPI_Transmit(&hspi1, buf, sizeof(buf), HAL_MAX_DELAY);
    uint64_t dt_us = FEB_Time_Us() - t0;

    LOG_I("SPI", "transfer took %llu us", (unsigned long long)dt_us);
}
```

## Boards Using This Library

Linked transitively via `feb_io` on every board that pulls in the I/O stack:

- [BMS](../../BMS/README.md), [DART](../../DART/README.md), [DASH](../../DASH/README.md), [LVPDB](../../LVPDB/README.md), [PCU](../../PCU/README.md), [Sensor_Nodes](../../Sensor_Nodes/README.md), [UART](../../UART/README.md), [UART_TEST](../../UART_TEST/README.md)

[DCU](../../DCU/README.md) does not currently link `feb_io` and does not use `feb_time`.

`feb_time` also powers the CSV-row timestamps emitted by [`FEB_Console`](../FEB_Serial_Library/README.md).

## See Also

- [`common/README.md`](../README.md) — library index
- [`FEB_Serial_Library/`](../FEB_Serial_Library/README.md) — consumes `feb_time` for log/console timestamps
