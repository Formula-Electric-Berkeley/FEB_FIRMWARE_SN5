# DART — Fan / Tachometer Controller

Standalone fan-and-tachometer controller. Drives fan PWM via timers and reports tach feedback on CAN. Cortex-M0 with only 32 KB Flash, so it runs bare-metal and is force-compiled at `-Os` even in Debug.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F042K6Tx | Cortex-M0 (no FPU) | 32 KB | 6 KB | No | `-Os -g3` Debug / `-Os` Release |

The root toolchain defaults target Cortex-M4F; [`DART/CMakeLists.txt`](CMakeLists.txt) strips `-mcpu=cortex-m4`, `-mfpu=fpv4-sp-d16`, `-mfloat-abi=hard` and substitutes `-mcpu=cortex-m0 -mthumb`.

## Peripherals

From [`DART.ioc`](DART.ioc):

- **CAN** — fan control commands + tach telemetry
- **TIM1, TIM2, TIM3** — PWM outputs for fan channels
- **TIM14, TIM16, TIM17** — tachometer input capture
- **USART2** — debug (DMA, bare-metal mode)
- **DMA**, **NVIC**

## Common Libraries Linked

From [`CMakeLists.txt`](CMakeLists.txt):

- [`feb_io`](../common/FEB_Serial_Library/README.md) — full I/O umbrella, but forced into bare-metal mode (see below)
- `feb_version` — build provenance

CAN pack/unpack is compiled directly from [`common/FEB_CAN_Library_SN4/gen/feb_can.c`](../common/FEB_CAN_Library_SN4/README.md); no `feb_can` runtime dispatcher is linked.

## Entry Point

`Core/User/Src/FEB_main.c` — **lowercase `main`**, unlike other boards which use `FEB_Main.c`. Sibling modules:

- `FEB_Fan.c` — PWM control + tach feedback
- `FEB_CAN.c`, `FEB_CAN_BMS.c` — CAN RX/TX
- `FEB_DART_Commands.c` — local console command parser

## Build & Flash

```bash
./scripts/build.sh -b DART
./scripts/flash.sh -b DART
```

## Notes

- **Tight Flash budget.** Default `-O0 -g3` overflows 32 KB once `feb_io` is linked, so `CMAKE_C_FLAGS_DEBUG` is overridden to `-Os -g3` in the board CMakeLists. Do not change this without verifying size.
- **Bare-metal serial stack.** `FEB_UART_FORCE_BARE_METAL=1` is set; `FEB_UART_MAX_INSTANCES=1`. Buffer sizes are tuned down (`TX=256`, `RX=128`, `LINE=64`, `STAGING=256`).
- **No FreeRTOS.** Only 6 KB RAM — an RTOS heap doesn't fit.
- **No default console registration** — DART uses its own lightweight command parser in `FEB_DART_Commands.c` instead of `FEB_Commands_RegisterSystem()`.
- Tachometer telemetry is transmitted at 10 Hz.

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
