# Sensor_Nodes — Sensor Aggregator (FRONT / REAR variants)

Aggregates IMU, magnetometer, GPS, and wheel-speed sensors; broadcasts on both CAN buses. Largest user-code footprint in the repo. Bare-metal.

The same source tree produces two firmware binaries — **FRONT** and **REAR** — selected at build time. Both variants run on identical hardware (one shared `Sensor_Nodes.ioc`, one shared MCU); they differ in which CAN frame IDs they publish and, optionally, in which sensors are physically populated. See [`Core/User/Inc/FEB_SN_Config.h`](Core/User/Inc/FEB_SN_Config.h).

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F446RETx | Cortex-M4F | 512 KB | 128 KB | No | `-Og` Debug / `-Os` Release |

## Peripherals

From [`Sensor_Nodes.ioc`](Sensor_Nodes.ioc):

- **CAN1, CAN2** — dual-bus CAN
- **I2C1, I2C3** — IMU + magnetometer
- **UART4** — GPS (NMEA) @ 115200 (see *GPS update rate* below)
- **USART2** — debug console
- **ADC1** — wheel-speed sensing
- **DMA**, **NVIC**

## Variants

| Variant | Output ELF | WSS | IMU accel/gyro | Mag | GPS (6 frames) | Fusion (5 frames) | Sensor temps | LinPot | Heartbeat |
|---|---|---|---|---|---|---|---|---|---|
| FRONT (default) | `Sensor_Nodes_FRONT.elf` | 0x24 | 0x26 / 0x28 | 0x2A | 0x40–0x45 | 0x47–0x4B | 0x4C | 0x1E | 0xD4 |
| REAR            | `Sensor_Nodes_REAR.elf`  | 0x25 | 0x27 / 0x29 | 0x2B | 0x50–0x55 | 0x57–0x5B | 0x4D | 0x1F | 0xD5 |

### Selecting a variant

`Sensor_Nodes_FRONT` and `Sensor_Nodes_REAR` are real CMake executable targets defined in [`CMakeLists.txt`](CMakeLists.txt). Both build from the same source set with different compile-time `FEB_SENSOR_NODE_VARIANT` defines. They appear as distinct entries in the VS Code CMake Tools build-target dropdown and can be built independently:

```bash
./scripts/build.sh -b Sensor_Nodes_FRONT      # build/Debug/Sensor_Nodes/Sensor_Nodes_FRONT.elf
./scripts/build.sh -b Sensor_Nodes_REAR       # build/Debug/Sensor_Nodes/Sensor_Nodes_REAR.elf

./scripts/flash.sh -b Sensor_Nodes_FRONT
./scripts/flash.sh -b Sensor_Nodes_REAR
```

Or directly via CMake:

```bash
cmake --preset Debug -S .
cmake --build build/Debug --target Sensor_Nodes_FRONT
cmake --build build/Debug --target Sensor_Nodes_REAR
cmake --build build/Debug --target Sensor_Nodes        # builds both via aggregate target
```

Both ELFs land in the same build directory; no per-variant build tree, no CMake reconfigure churn.

### Per-sensor presence flags

If a variant doesn't physically have one of the sensors, set the matching flag in [`Core/User/Inc/FEB_SN_Config.h`](Core/User/Inc/FEB_SN_Config.h):

```c
#if FEB_SN_IS_REAR()
#  define FEB_SN_HAS_IMU          1
#  define FEB_SN_HAS_MAG          0   /* example: REAR board has no magnetometer */
#  define FEB_SN_HAS_GPS          1
#  define FEB_SN_HAS_WSS          1
#  define FEB_SN_HAS_FUSION       0   /* must be 0 if either IMU or MAG is 0 */
#  define FEB_SN_HAS_SENSOR_TEMPS 0   /* must be 0 if either IMU or MAG is 0 */
#endif
```

The flags gate both the sensor driver init (no I2C/UART traffic to a missing chip — avoids spurious init-failure logs and bus stalls) and the corresponding CAN reporter Tick body (no frames published for absent sensors). Disabled sensors carry zero flash/RAM cost.

### Adding a new sensor that needs its own FRONT/REAR CAN IDs

1. Add `get_<sensor>_data_front` and `get_<sensor>_data_rear` functions to `common/FEB_CAN_Library_SN4/msg_defs/sensor_nodes_messages.py`.
2. Register both frame IDs in `common/FEB_CAN_Library_SN4/generate.py`.
3. Run `cd common/FEB_CAN_Library_SN4 && ./generate_can.sh` to refresh `gen/feb_can.{h,c}`.
4. Add the alias block to `Core/User/Inc/FEB_SN_Config.h` (mirror the existing pattern for IMU/MAG/etc.).
5. Implement the reporter `.c`/`.h` in `Core/User/`. Use `FEB_SN_<SENSOR>_*` aliases — never the underlying `feb_can_*_front/_rear` symbols.

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

`Core/User/Src/FEB_main.c` (lowercase `main`, same as DART). User modules include `FEB_IMU.c`, `FEB_Magnetometer.c`, `FEB_GPS.c`, `FEB_WSS.c`, and `FEB_CAN_IMU.c`. The variant config header is `Core/User/Inc/FEB_SN_Config.h`.

## GPS update rate

The receiver runs at **10 Hz**, and the six GPS frames are published **as each fix
lands** rather than on a fixed timer. Four things have to line up for that, and all
four used to be wrong — the pipeline was effectively 1 Hz:

| Stage | Mechanism |
|---|---|
| Link rate | UART4 is 115200 in the `.ioc`. 9600 (960 B/s) cannot carry GGA+GSA+RMC (~210 B) above ~4 Hz. |
| Baud negotiation | `FEB_GPS_Init()` probes for NMEA, and if the module came up at its 9600 default sends `PMTK251,115200` and follows it up. Falls back to 9600 @ 1 Hz rather than going silent. |
| Fix rate | `FEB_GPS_SetUpdateRate(10)` (`PMTK220,100`) at boot. Without it the MTK3339 stays at its 1 Hz factory default. |
| Commit trigger | The snapshot is committed once per RMC. It used to gate on the UTC second changing, but NMEA time is whole-second only, so every fix after the first in a given second was dropped. |

Accuracy is helped by SBAS/WAAS (`PMTK313,1` + `PMTK301,2`, ~2.5 m CEP → 1–2 m). The
parser is double-precision (`LWGPS_CFG_DOUBLE`) and lat/lon go out as int32 at 1e-7 deg
(~1.1 cm), so neither the parser nor the wire format limits precision — the receiver does.

## Notes

- **Bare-metal polling.** No FreeRTOS — all sensors are polled from the main loop.
- **Two I²C buses.** `I2C1` is the shared sensor bus; `I2C3` isolates a second sensor that needs its own bus.
- **Largest LOC.** More user code than any other board — be mindful of the `Core/User/` tree when navigating.
- **LwGPS** lives outside `common/` on purpose: it's a third-party drop-in, not a FEB library.
- **Single `.ioc`.** Both variants share `Sensor_Nodes.ioc` (same PCB). If FRONT/REAR ever require different pin maps, split into two `.ioc` files and gate the generated `Core/Src/main.c` by `SENSOR_NODE_VARIANT`.
- **Heartbeat (0xD4 / 0xD5).** Transmitted at 10 Hz by `FEB_CAN_Heartbeat.c`. Carries named error bits (see
  `common/FEB_CAN_Library_SN4/msg_defs/sensor_nodes_messages.py`) grouped by byte: device faults, data
  freshness, CAN health. The BMS keys node presence off its arrival (`FEB_HB_FSN` / `FEB_HB_RSN`).

## See Also

- [`README.md`](../README.md) — repo overview
- [`common/README.md`](../common/README.md) — library index
- [`Core/User/Inc/FEB_SN_Config.h`](Core/User/Inc/FEB_SN_Config.h) — variant macro + alias table
