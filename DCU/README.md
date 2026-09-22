# DCU — Data Control Unit

Promiscuous CAN logger and telemetry transmitter. Captures **every** frame on both CAN buses, writes them to a CSV on the SD card, and forwards a rate-limited subset over an RFM95 LoRa link to [`DCU_Receiver`](../DCU_Receiver/README.md).

The DCU **never transmits on CAN** — it is a pure sink. (The `0xD3 // DCU heartbeat` entry in the radio allow-list is aspirational; nothing sends it yet.)

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F446RETx | Cortex-M4F | 512 KB | 128 KB | **Yes** (CMSIS-RTOS v2, heap_4) | `-Og` Debug / `-Os` Release |

## Peripherals

From [`DCU.ioc`](DCU.ioc):

- **CAN1, CAN2** — both @ 500 kbit/s, `CAN_MODE_NORMAL`, accept-all filters
- **SPI1** — SD card (FATFS), 22.5 Mbit/s
- **SPI3** — RFM95 LoRa radio, 5.625 Mbit/s
- **I2C1** — TPS2482 power monitor
- **USART2** — debug console @ 115200 8N1
- **DMA**, **NVIC**, **FATFS**, **FREERTOS**

## Data path

```
CAN1/CAN2  (accept-all filter banks 0 and 14, mask = 0)
    │  RX ISR → feb_can rx_queue (canRxQueue, 128 deep)
    ▼
canDispatchTask (prio 32, 1 ms)  →  FEB_CAN_RX_Process()
    │  wildcard_callback  → drop-oldest enqueue
    ▼
canLogQueue  (256 × DCU_CAN_Frame_t, 20 B)
    │
    ▼  canLogTask (prio 16)
    ├─► SD:      CSV row → 4 KB buffer → flush at 3072 B or 1000 ms
    ├─► console: CSV `can` row          [if dcu|can|stream|on]
    └─► radio:   DCU_CAN_Filter_ShouldForwardToRadio() → forward queue (64, drop-oldest)
                     │
                     ▼  radioTask (prio 40)
                 batch ≤16 frames → RFM95 → DCU_Receiver
```

**Capture does not depend on the SD card.** The CAN wildcards are registered before SD bring-up, so a missing or unmountable card costs you the CSV only — the console stream and the radio link keep running, and an SD card inserted later is picked up within ~10 s. (This was previously a hard dependency: no card meant no telemetry of any kind.)

## CAN acceptance

DCU accepts **everything**, structurally — nothing enumerates CAN IDs on the RX path:

- `FEB_CAN_Filter_AcceptAll()` on CAN1 bank 0 and CAN2 bank 14 → mask 0, every bit don't-care.
- A `FEB_CAN_FILTER_WILDCARD` RX registration on both buses → unconditional match.

Adding a new CAN ID anywhere on the car requires **no DCU change** to be captured and logged.

One caveat: the wildcards register `.id_type = FEB_CAN_ID_STD`, and `feb_can_rx.c` rejects on ID-type mismatch before the filter check — so **extended 29-bit frames are not dispatched**. Every FEB message is standard 11-bit, so this only affects third-party extended-ID traffic (e.g. the Elcon charger at `0x1806E5F4`).

## Radio link

RFM95 (Semtech SX1276), configured in `FEB_RFM95_GetDefaultConfig()`:

| Parameter | Value |
|---|---|
| Frequency | 915 MHz (US ISM) |
| TX power | 14 dBm |
| Bandwidth | 250 kHz |
| Spreading factor | SF7 |
| Coding rate | 4/5 |
| Sync word | `0x12`, preamble 8, CRC on |

Wire format is [`FEB_Radio_Protocol.h`](Core/User/Inc/FEB_Radio_Protocol.h) — **keep it byte-identical with the copy in `DCU_Receiver/`**. It batches up to 16 CAN frames into one ≤210-byte packet and carries a raw 32-bit CAN ID per record, so new IDs never require a protocol change. There is no sequence number, ACK or retransmit: a lost packet silently loses up to 16 frames.

### Bandwidth budget — the binding constraint

A full 210-byte batch takes **≈166 ms** of airtime, so the link ceiling is **≈96 CAN frames/s**, and only if batches stay full (one-frame batches drop it to ~43 f/s). Current demand is roughly 60 f/s. **This is the scarcest resource on the car — 40× narrower than the CAN bus it feeds from.**

The single tuning knob is `k_radio_allow[]` in [`Core/User/Src/DCU_CAN_Filter.c`](Core/User/Src/DCU_CAN_Filter.c):

1. IDs in the allow-list are forwarded at their own `min_interval_ms`.
2. Everything else is forwarded by the catch-all at 1 Hz per ID (`DCU_CAN_FORWARD_ALL`, `DCU_CAN_FORWARD_ALL_INTERVAL_MS`).
3. Set `DCU_CAN_FORWARD_ALL` to 0 for allow-list-only.

> **Sensor-node GPS note.** `0x40–0x45` / `0x50–0x55` are *not* allow-listed, so they go out at 1 Hz each regardless of the 10 Hz the sensor nodes publish. The full rate reaches the **SD card and the CAN bus**, not the radio. Putting GPS on the air at 10 Hz would need 60 f/s of the ~96 f/s budget by itself — allow-list one or two of the six frames, don't add them all.

> **Catch-all hash caveat.** `fwdall_should_forward()` hashes `(bus<<29)|can_id` into 128 slots, which for standard 11-bit IDs on bus 1 degenerates to **`can_id & 0x7F`**. Colliding IDs evict each other and each forward faster than the nominal 1 Hz. Today no two *transmitted* IDs collide, but e.g. `0x50` and `0x250` would.

## Common Libraries Linked

`feb_io` (UART + log + console + commands), `feb_can`, `feb_tps`, `feb_version`. CAN codecs come from [`common/FEB_CAN_Library_SN4/gen/`](../common/FEB_CAN_Library_SN4/).

## Entry Point

[`Core/User/Src/FEB_Main.c`](Core/User/Src/FEB_Main.c) — `FEB_Init()`, called from `MX_FREERTOS_Init()`.

### Tasks

| Task | Priority | Stack | Role |
|---|---|---|---|
| `radioTask` | High (40) | 512 | LoRa TX/RX, batching |
| `canDispatchTask` | AboveNormal (32) | 256 | `FEB_CAN_RX_Process()` @ 1 ms |
| `uartRxTask` | Normal (24) | 512 | Console input |
| `sdTask` | BelowNormal (16) | 1024 | Sole FATFS/SPI1 owner |
| `canLogTask` | BelowNormal (16) | 1024 | CSV format, SD flush, radio forward |

> The producer (`canDispatchTask`, 32) outranks the consumer (`canLogTask`, 16) by two levels, and `canLogTask` also blocks on SD I/O. Under sustained bus load `canLogQueue` can drain slower than it fills; `enqueue_frame()` then drops the **oldest** frame and bumps the counter shown by `dcu|can|log`. **A non-zero drop count is the signal to revisit these priorities.**

## SD logging

One CSV per boot: `0:log_NNNN.csv` (8.3 name — FATFS is built with `_USE_LFN=0`). The session counter persists in `0:canlog.idx`.

```
timestamp_ms,bus,can_id,dlc,d0,d1,d2,d3,d4,d5,d6,d7
```

## Build, Flash & Verify

```bash
./scripts/build.sh -b DCU
./scripts/flash.sh -b DCU
./scripts/serial.sh -b DCU
```

Expect a boot banner, then `canLogTask starting` and either `Logging to 0:log_NNNN.csv` or `SD prep failed — continuing without SD`.

Console: `dcu|can|log` (capture + SD state, written, drops, queue depth), `dcu|can|stream|on`, `dcu|radio|status`, `dcu|radio|stats`, `dcu|radio|stream|on|off`, `dcu|sd|...`, `dcu|tps`.

## Known issues

- **`DCU_TPS_Update()` is never called.** It is defined and declared, but no caller exists, so `DCU_TPS_GetData()` returns whatever `DCU_TPS_Init()` left behind. Its own header says "call at ~10 Hz from main loop" — there is no such loop. TPS readings from this board are currently meaningless.
- **Extended-ID frames are captured by hardware but dropped by the dispatcher** (see *CAN acceptance*).
- **No application-level integrity check on the radio link** beyond the SX1276 hardware CRC — no sequencing, so frame loss is silent.

## See Also

- [`DCU_Receiver/README.md`](../DCU_Receiver/README.md) — the other end of the link
- [`Core/User/Inc/FEB_Radio_Protocol.h`](Core/User/Inc/FEB_Radio_Protocol.h) — packet format (the authoritative spec)
- [`Core/User/Src/DCU_CAN_Filter.c`](Core/User/Src/DCU_CAN_Filter.c) — forwarding policy
- [`common/README.md`](../common/README.md) — library index
