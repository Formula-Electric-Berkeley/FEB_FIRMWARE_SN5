# PCU — Powertrain Control Unit

Gateway between the BMS, RMS motor inverter, and vehicle CAN. Handles tractive power requests, regen enablement, and analog sensing via triple ADC. Bare-metal.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F446RETx | Cortex-M4F | 512 KB | 128 KB | No | `-Og` Debug / `-Os` Release |

## Peripherals

From [`PCU.ioc`](PCU.ioc):

- **CAN1, CAN2** — dual CAN (vehicle + RMS inverter)
- **ADC1, ADC2, ADC3** — triple ADC for throttle / brake / accumulator sensing
- **I2C1** — TPS2482 power monitor
- **TIM1** — control-loop timing
- **USART2** — debug console
- **DMA**, **NVIC**

## Common Libraries Linked

From [`CMakeLists.txt`](CMakeLists.txt):

- [`feb_io`](../common/FEB_Serial_Library/README.md) — UART + Log + Console + Commands + Version + Time + String_Utils
- [`feb_can`](../common/FEB_CAN_Library/README.md) — CAN runtime (dual-instance)
- [`feb_tps`](../common/FEB_TPS_Library/README.md) — TPS2482 driver (single device)
- `feb_version`

CAN pack/unpack is compiled from [`common/FEB_CAN_Library_SN4/gen/feb_can.c`](../common/FEB_CAN_Library_SN4/README.md).

## Entry Point

`Core/User/Src/FEB_Main.c` — called from CubeMX's `main.c` after HAL init.

## Build & Flash

```bash
./scripts/build.sh -b PCU
./scripts/flash.sh -b PCU
```

## Notes

- **Triple ADC.** All three ADCs are enabled; DMA-driven conversions feed throttle / brake / accumulator voltage channels. Regen eligibility is checked against a brake-pedal safety interlock (BSPD).
- **Dual CAN.** CAN1 is vehicle CAN; CAN2 talks to the RMS inverter using Cascadia Motion message IDs (0xC0–0xCF range).
- **Bare-metal loop.** CAN TX/RX and TPS polling run from the main loop; no FreeRTOS tasks. Log levels default to `INFO` (`FEB_LOG_COMPILE_LEVEL=3`) — bump via `target_compile_definitions` in the CMakeLists if needed.
- **TPS shunt** is 12 mΩ, rated for 4 A. See the PCU example in the [TPS library README](../common/FEB_TPS_Library/README.md#single-device-pcu-bms).

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
