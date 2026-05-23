# UART_TEST — STM32U5 Validation Fixture

Minimal test harness for the next-generation Cortex-M33 MCU (STM32U575). Validates the FEB serial stack on the U5 HAL, GPDMA1 (not the legacy DMA controller), and the FreeRTOS M33 non-secure port with heap_5.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32U575ZITxQ | Cortex-M33 | 2048 KB | 768 KB + 16 KB SRAM4 | Yes (heap_5, ARM_CM33_NTZ port) | `-Og` Debug / `-Os` Release |

## Peripherals

From [`UART_TEST.ioc`](UART_TEST.ioc):

- **USART1** — host console
- **GPDMA1** — new-generation DMA (not compatible with legacy DMA APIs)
- **ICACHE**, **PWR**, **LPBAM**
- **CORTEX_M33_NS**, **DEBUG** — non-secure debug

## Common Libraries Linked

From [`CMakeLists.txt`](CMakeLists.txt):

- [`feb_io`](../common/FEB_Serial_Library/README.md) — UART + Log + Console + Commands + Version + Time + String_Utils
- [`feb_rtos_utils`](../common/FEB_RTOS_Utils/README.md) — `REQUIRE_RTOS_HANDLE`
- `feb_version`

No CAN, no TPS — this is a serial-stack test board.

## Entry Point

`Core/User/Src/FEB_Main.c` — brings up UART / log / console and then hands to the scheduler.

## Build & Flash

```bash
./scripts/build.sh -b UART_TEST
./scripts/flash.sh -b UART_TEST
```

## Notes

- **Cortex-M33, not M4.** The board CMakeLists explicitly adds `-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard` to override the root-level M4 flags.
- **GPDMA1, not DMA.** All DMA channel numbers and request-line indices differ from the F4 boards — don't copy DMA config from an F446 board.
- **FreeRTOS port.** Uses `ARM_CM33_NTZ/non_secure/` (non-trustzone); `heap_5` with `USE_FreeRTOS_HEAP_5` defined.
- **HAL family** is `STM32U5xx_HAL_Driver`, different from the F4/F0 boards.
- **Why it exists.** To qualify the common libraries on the U5 platform before any production board migrates. Keep this board minimal — new U5 code should go into a purpose-built board, not here.

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
