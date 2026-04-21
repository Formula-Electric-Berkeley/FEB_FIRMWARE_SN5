# Sensor_Nodes — Sensor Aggregator

Aggregates IMU, magnetometer, GPS, and wheel-speed sensors; broadcasts on both CAN buses. Largest user-code footprint in the repo. Bare-metal.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F446RETx | Cortex-M4F | 512 KB | 128 KB | No | `-Og` Debug / `-Os` Release |

## Peripherals

From [`Sensor_Nodes.ioc`](Sensor_Nodes.ioc):

- **CAN1, CAN2** — dual-bus CAN
- **I2C1, I2C3** — IMU + magnetometer
- **UART4** — GPS (NMEA)
- **USART2** — debug console
- **ADC1** — wheel-speed sensing
- **DMA**, **NVIC**

## Common Libraries Linked

From [`CMakeLists.txt`](CMakeLists.txt):

- [`feb_io`](../common/FEB_Serial_Library/README.md) — UART + Log + Console + Commands + Version + Time + String_Utils
- [`feb_can`](../common/FEB_CAN_Library/README.md) — CAN runtime (dual-instance)
- [`feb_tps`](../common/FEB_TPS_Library/README.md) — TPS2482 driver (single device)
- `feb_version`

CAN pack/unpack is compiled from [`common/FEB_CAN_Library_SN4/gen/feb_can.c`](../common/FEB_CAN_Library_SN4/README.md).

## Third-Party

- **[LwGPS](https://github.com/MaJerle/lwgps)** — lightweight NMEA parser, pulled from the repo-root `third-party/lwgps/` and compiled directly into the board image via the CMakeLists.

## Entry Point

`Core/User/Src/FEB_main.c` (lowercase `main`, same as DART). User modules include `FEB_IMU.c`, `FEB_Magnetometer.c`, `FEB_GPS.c`, `FEB_WSS.c`, and `FEB_CAN_IMU.c`.

## Build & Flash

```bash
./scripts/build.sh -b Sensor_Nodes
./scripts/flash.sh -b Sensor_Nodes
```

## Notes

- **Bare-metal polling.** No FreeRTOS — all sensors are polled from the main loop.
- **Two I²C buses.** `I2C1` is the shared sensor bus; `I2C3` isolates a second sensor that needs its own bus.
- **Largest LOC.** More user code than any other board — be mindful of the `Core/User/` tree when navigating.
- **LwGPS** lives outside `common/` on purpose: it's a third-party drop-in, not a FEB library.

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
