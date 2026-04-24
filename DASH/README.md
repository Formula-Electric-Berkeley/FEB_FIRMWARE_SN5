# DASH — Driver Dashboard

Graphical driver interface. Displays vehicle telemetry and errors on an 800×480 capacitive touchscreen, logs to an SD card, and runs UI via LVGL under FreeRTOS. Largest board on the car by Flash and RAM.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F469NIHx | Cortex-M4F | 2048 KB | 320 KB + 64 KB CCMRAM + 8 MB SDRAM | Yes (heap_4, 32 KB) | `-Og` Debug / `-Os` Release |

## Peripherals

From [`DASH.ioc`](DASH.ioc):

- **LTDC + DSIHOST + DMA2D** — MIPI-DSI display path to OTM8009A / NT35510 panel
- **FMC** — external SDRAM for framebuffer
- **QUADSPI** — external flash (fonts, UI assets)
- **I2C1** — FT6x06 touchscreen controller (via `FEB_I2C_Mutex`)
- **I2C2** — auxiliary I²C bus
- **SDIO + FATFS** — SD-card logging
- **CAN1** — vehicle CAN
- **USART3, USART6** — debug + auxiliary
- **SAI1** — audio
- **CRC**, **TIM1**, **DMA**, **NVIC**

## Common Libraries Linked

From [`CMakeLists.txt`](CMakeLists.txt):

- [`feb_io`](../common/FEB_Serial_Library/README.md) — UART + Log + Console + Commands + Version + Time + String_Utils
- [`feb_rtos_utils`](../common/FEB_RTOS_Utils/README.md) — `REQUIRE_RTOS_HANDLE`
- `feb_version`

CAN pack/unpack is compiled from [`common/FEB_CAN_Library_SN4/gen/`](../common/FEB_CAN_Library_SN4/README.md) and the runtime dispatcher from [`common/FEB_CAN_Library/Src/`](../common/FEB_CAN_Library/README.md) via `file(GLOB)`; DASH does not link the `feb_can` INTERFACE target.

## Third-Party Drivers

Bundled under `Drivers/`:

- **LVGL** (`Drivers/lvgl/`) — UI toolkit
- **LVGL STM HAL layer** (`Drivers/hal_stm_lvgl/`)
- **BSP** (`Drivers/BSP/STM32469I-Discovery/`) — display, SDRAM, touchscreen driver glue
- **Components** — `otm8009a`, `nt35510`, `ft6x06` display/touch drivers
- **Fonts** (`Drivers/Utilities/Fonts/`)

## Entry Point

No single `FEB_Main.c`. UI modules live in `Core/User/Src/` and `Core/User/Src/UI_Elements/`, wired into FreeRTOS tasks:

- `displayTask` — LVGL tick + render
- `btnTxLoopTask` — button / steering-wheel input
- `uartRxTask`, `uartTxTask` — serial I/O
- `DASHTaskRx`, `DASHTaskTx` — CAN endpoints

Start up from CubeMX's `main.c` → `MX_FREERTOS_Init()` → task entry points.

## Build & Flash

```bash
./scripts/build.sh -b DASH
./scripts/flash.sh -b DASH
```

## Notes

- **Tasks are dense.** `displayTask` runs at priority 40 with 1 KB stack; `btnTxLoopTask` at priority 41. Stack-overflow checking is on (`configCHECK_FOR_STACK_OVERFLOW=2`) — keep UI code off the stack.
- **SDRAM init order matters.** FMC and DSIHOST must be brought up before LVGL can allocate its draw buffers. CubeMX handles the sequence in `MX_FMC_Init()` / `MX_DSIHOST_DSI_Init()`; don't reorder.
- **`configUSE_IDLE_HOOK=1`** and `configUSE_MALLOC_FAILED_HOOK=1` — malloc failures halt immediately.
- **FATFS** is enabled for SD-card logging via SDIO.
- Recursive mutexes are enabled (`configUSE_RECURSIVE_MUTEXES=1`) for the I²C bus shared between touch and auxiliary sensors.

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
