# UART — Serial / Console Bridge

Dedicated UART bridge and debug fixture. Runs the full FEB console stack under FreeRTOS with a flash-benchmark task reserved for flash sector characterization.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F446RETx | Cortex-M4F | 512 KB | 128 KB | Yes (heap_4, 50 KB) | `-Og` Debug / `-Os` Release |

## Peripherals

From [`UART.ioc`](UART.ioc):

- **USART1, USART2** — dual UART (one for host console, one for peer)
- **RTC** — timestamp source
- **DMA**, **NVIC**

## Common Libraries Linked

From [`CMakeLists.txt`](CMakeLists.txt):

- [`feb_io`](../common/FEB_Serial_Library/README.md) — UART + Log + Console + Commands + Version + Time + String_Utils
- [`feb_rtos_utils`](../common/FEB_RTOS_Utils/README.md) — `REQUIRE_RTOS_HANDLE`
- `feb_version`

No CAN, no TPS on this board.

## Entry Point

`Core/User/Src/FEB_Main.c` — starts console, then hands control to the FreeRTOS scheduler.

## Build & Flash

```bash
./scripts/build.sh -b UART
./scripts/flash.sh -b UART
```

## Notes

- **FreeRTOS heap** is 50 KB (`configTOTAL_HEAP_SIZE=51200`). Tasks: `uartRxTask`, `flashTask`.
- **Flash sector reserved.** Sector 7 is intentionally reserved for flash-performance testing by `flashTask`; don't write to it unless you know the scratch protocol.
- **FPU is on.**
- Primary use is as a **console / debug fixture** and as the reference implementation for the FEB serial stack — new consumers of `feb_io` should follow this board's initialization order (`FEB_UART_Init` → `FEB_Log_Init` → `FEB_Console_Init` → `FEB_UART_SetRxLineCallback`).

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
- [`common/FEB_Serial_Library/README.md`](../common/FEB_Serial_Library/README.md) — detailed console / log / UART docs
