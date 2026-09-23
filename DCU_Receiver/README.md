# DCU_Receiver — Telemetry Ground Station

The receiving half of the CAN-over-LoRa link. Listens for packets from the [`DCU`](../DCU/README.md) on the car, decodes them back into CAN frames, and emits them as CSV rows over UART for a host PC.

**This board has no CAN peripheral.** It does not re-transmit onto a CAN bus — it is the end of the line, turning radio packets into text a laptop can read.

## MCU

| Part | Core | Flash | RAM | FreeRTOS | Optimization |
|---|---|---|---|---|---|
| STM32F446RETx | Cortex-M4F | 512 KB | 128 KB | **Yes** (CMSIS-RTOS v2) | `-Og` Debug / `-Os` Release |

## Peripherals

From [`DCU_Receiver.ioc`](DCU_Receiver.ioc):

- **SPI3** — RFM95 LoRa radio, 5.625 Mbit/s
- **USART2**, **UART4** — both @ 115200 8N1, DMA TX/RX
- **DMA**, **NVIC**, **FREERTOS**

No CAN, no SD, no I²C.

## Data path

```
RFM95 LoRa  (915 MHz, SF7 / BW 250 kHz / CR 4/5, sync 0x12)
    │  FEB_RFM95_Receive(..., 500 ms timeout)
    ▼
handle_radio_payload()   — dispatch on magic byte
    │  0xFB → FEB_Radio_Parse()
    ▼  per decoded CAN frame:
    ├─► FEB_CAN_DB_Update(can_id, data, dlc, tick)   — generated DBC state model
    └─► FEB_CAN_Stream_EmitFrame(bus, can_id, dlc, data)
             │
             ▼  "csv,<tx_id>,DCU_Receiver,<us>,can,<bus>,0x<ID>,<dlc>,<d0..d7>"
         USART2 / UART4 @ 115200  →  host PC
```

Every 500 ms while streaming it also emits `signal,<rssi>,<snr>`, or `signal,nan,nan` if no packet has arrived for 1 s — so a host can distinguish "link down" from "car quiet".

## Decoding

`FEB_CAN_DB_Update()` comes from [`common/FEB_CAN_Library_SN4/gen/feb_can_db.c`](../common/FEB_CAN_Library_SN4/), generated from the DBC. It is a `switch` over every registered frame ID, so **new CAN IDs are supported automatically once the CAN submodule is regenerated** — this board needs no hand-written table.

An unknown ID returns `-1` and logs a warning, but `FEB_CAN_Stream_EmitFrame()` still emits the row, so the host sees the frame regardless. Noisy, not broken.

> Keep the generated CAN library **in sync between `DCU/` and `DCU_Receiver/`**. They share the submodule, so a normal build does this for you.

## Wire protocol

[`Core/User/Inc/FEB_Radio_Protocol.h`](Core/User/Inc/FEB_Radio_Protocol.h) — **must stay byte-identical with the `DCU/` copy.** It is header-only by design so neither CubeMX-generated CMakeLists needs a new `.c`. There is no build-time check that the two match; divergence corrupts frames silently.

## Throughput ceiling

| Stage | Limit |
|---|---|
| LoRa link | ~96 CAN frames/s (full 16-frame batches) |
| **Console output** | **115200 8N1 ÷ ~80 B/row ≈ 140 rows/s** |

The UART is the next wall behind the radio. Note that `feb_uart`'s write path spins up to **1000 ms** waiting for ring space before truncating — and it runs in the radio task, so a host that stops draining the port can stall reception for a full second. Keep the reader running.

## Build, Flash & Verify

```bash
./scripts/build.sh -b DCU_Receiver
./scripts/flash.sh -b DCU_Receiver
./scripts/serial.sh -b DCU_Receiver
```

Listen mode is **on by default**. With a DCU powered and streaming, expect `can` rows to start appearing. Console: `dcu|can|stream [on|off]`, `dcu|radio|status` (RSSI/SNR), `dcu|radio|stats`, `dcu|radio|listen`, `dcu|radio|tx <msg>`, `dcu|radio|rx <timeout_ms>`.

**Link check without a car:** run `dcu|radio|tx PING` on one board and watch for `PONG` on the other — the legacy ASCII ping/pong path shares the air with the `0xFB` batch traffic.

## Known issues

- `print_raw_packet()` formats into a `char line[128]`, which truncates the hex dump of any packet over ~34 bytes. Cosmetic — the decoded frames are unaffected — but it also writes to **both** UARTs, costing ~256 B of TX per packet.
- This board's `FEB_RFM95.c` is missing the `s_initialized` guards that the DCU copy has in `StartReceive`/`Standby`/`Sleep`/`OnDIO1`.
- No sequencing or ACK on the link: lost packets are silent, and up to 16 frames vanish with them.

## See Also

- [`DCU/README.md`](../DCU/README.md) — the transmitting end, including the radio bandwidth budget and the forwarding allow-list
- [`common/README.md`](../common/README.md) — library index
