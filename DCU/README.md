# DCU — Data Control Unit

> **Status: placeholder.** The board builds cleanly but contains no application code. `Core/User/Src/` is empty and only USART2 is enabled in CubeMX. Treat this as a skeleton to fill in, not a flight-ready image.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F446RETx | Cortex-M4F | 512 KB | 128 KB | No | `-Og` Debug / `-Os` Release |

## Peripherals

From [`DCU.ioc`](DCU.ioc):

- **USART2** — only peripheral currently enabled

No CAN, I²C, SPI, ADC, or timers configured yet.

## Common Libraries Linked

From [`CMakeLists.txt`](CMakeLists.txt):

- `feb_version` — build provenance

Notably DCU does **not** link `feb_io`. If/when application code is added, link additional libraries as needed — see [`common/README.md`](../common/README.md) for available targets.

## Entry Point

None yet. The CubeMX-generated `Core/Src/main.c` runs and returns to an infinite loop with no user callbacks.

## Build & Flash

```bash
./scripts/build.sh -b DCU
./scripts/flash.sh -b DCU
```

## Notes

- **Not in production use.** Before flashing to a car, add `Core/User/Src/FEB_Main.c`, wire CubeMX peripherals, and link the appropriate common libraries.
- Linker script `STM32F446XX_FLASH.ld` is the same as other F446 boards (512 KB / 128 KB), so there's no size pressure — feel free to pull in `feb_io`, `feb_can`, and `feb_tps` when you start development.

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
- [`BMS/README.md`](../BMS/README.md), [`PCU/README.md`](../PCU/README.md) — templates to copy if you start filling in DCU
