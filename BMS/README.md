# BMS — Battery Management System

Monitors cell voltages, pack temperatures, isolation (IVT), and accumulator relays; reports state over CAN. Runs a FreeRTOS task stack around an ADBMS6830B / LTC6811 cell-monitor front-end.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F446RETx | Cortex-M4F | 512 KB | 128 KB | Yes (heap_4, 64 KB) | `-Og` Debug / `-Os` Release |

## Peripherals

From [`BMS.ioc`](BMS.ioc):

- **CAN1** — vehicle CAN bus
- **SPI1, SPI2** — ADBMS6830B / LTC6811 cell monitor chain
- **I2C1** — TPS2482 power monitor
- **ADC1** — analog front-end
- **USART2** — debug console (DMA)
- **DMA**, **NVIC**

## Common Libraries Linked

From [`CMakeLists.txt`](CMakeLists.txt):

- [`feb_io`](../common/FEB_Serial_Library/README.md) — umbrella: UART + Log + Console + Commands + Version + Time + String_Utils
- [`feb_tps`](../common/FEB_TPS_Library/README.md) — TPS2482 power monitoring
- [`feb_rtos_utils`](../common/FEB_RTOS_Utils/README.md) — `REQUIRE_RTOS_HANDLE` fail-fast check
- `feb_version`, `feb_uart`, `feb_log`, `feb_console` — already in `feb_io`; listed here explicitly to keep intent visible in the build graph.

CAN pack/unpack is included directly from [`common/FEB_CAN_Library_SN4/gen/`](../common/FEB_CAN_Library_SN4/README.md) and the runtime dispatcher from [`common/FEB_CAN_Library/Src/`](../common/FEB_CAN_Library/README.md) via `file(GLOB)` — BMS does not link the `feb_can` INTERFACE target.

## Entry Point

`Core/User/Src/FEB_Main.c` — called from CubeMX's `main.c` after HAL init. Surrounding modules in `Core/User/Src/`:

- `FEB_CAN_State.c` — CAN state machine
- `FEB_ADBMS.c` / `FEB_LTC6811.c` — cell-monitor driver
- `FEB_Relay.c` — accumulator relay control
- `FEB_IVT.c` — isolation monitoring

## Build & Flash

```bash
./scripts/build.sh -b BMS
./scripts/flash.sh -b BMS
```

## Notes

- **FreeRTOS heap** is 64 KB (`configTOTAL_HEAP_SIZE=65536`). Six tasks: `uartRxTask`, `ADBMSTask`, `TPSTask`, `BMSTaskRx`, `BMSTaskTx`, `SMTask`.
- **Stack-overflow checking** is enabled (`configCHECK_FOR_STACK_OVERFLOW=2`).
- **FPU is on** in the FreeRTOS config; `-mfloat-abi=hard` matches.
- CAN mutexes (`canTxMutex`, `canRxMutex`) and queues (`canTxQueue`, `canRxQueue`) are declared in the CubeMX-generated RTOS config and wired into `FEB_CAN_Config_t`.

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
