# LVPDB — Low Voltage Power Distribution Board

Distributes and monitors low-voltage rails. Runs seven TPS2482 power monitors over I²C with per-device enable/power-good/alert GPIO, and reports telemetry on both CAN buses. Bare-metal.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F446RETx | Cortex-M4F | 512 KB | 128 KB | No | `-Og` Debug / `-Os` Release |

## Peripherals

From [`LVPDB.ioc`](LVPDB.ioc):

- **CAN1, CAN2** — dual-bus CAN
- **I2C1** — TPS2482 power monitors (7 devices)
- **TIM1, TIM2** — power sequencing / timeouts
- **USART2** — debug console
- **DMA**, **NVIC**

## Common Libraries Linked

From [`CMakeLists.txt`](CMakeLists.txt):

- [`feb_io`](../common/FEB_Serial_Library/README.md) — UART + Log + Console + Commands + Version + Time + String_Utils
- [`feb_can`](../common/FEB_CAN_Library/README.md) — CAN runtime (dual-instance)
- [`feb_tps`](../common/FEB_TPS_Library/README.md) — multi-device TPS2482 driver
- `feb_version`

CAN pack/unpack is compiled from [`common/FEB_CAN_Library_SN4/gen/feb_can.c`](../common/FEB_CAN_Library_SN4/README.md).

## Entry Point

`Core/User/Src/` holds several CAN-focused modules rather than a single `FEB_Main.c`. Startup is driven from CubeMX's `main.c` → user init.

## Build & Flash

```bash
./scripts/build.sh -b LVPDB
./scripts/flash.sh -b LVPDB
```

## Notes

- **Seven TPS2482s** share a single I²C bus; each has its own EN/PG/Alert GPIO. See the LVPDB example in the [TPS library README](../common/FEB_TPS_Library/README.md#boards-using-this-library).
- **Dual CAN** — both `CAN1` and `CAN2` are configured via `FEB_CAN_Config_t { .hcan1 = &hcan1, .hcan2 = &hcan2, ... }`.
- **Bare-metal** — no FreeRTOS in CubeMX. Periodic TPS polls and CAN processing are driven from the main loop.

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
- [`common/FEB_TPS_Library/README.md`](../common/FEB_TPS_Library/README.md) — batch-poll pattern this board uses
