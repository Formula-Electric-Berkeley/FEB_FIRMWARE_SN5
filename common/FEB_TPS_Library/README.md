# FEB TPS2482 Library

Minimal driver for TPS2482 power monitors used across Formula Electric @ Berkeley boards.

## Design

Each call talks directly to the chip — no measurement caching, no I²C
retries, no peripheral resets. If the bus is healthy, reads succeed; if it
isn't, the call returns `FEB_TPS_ERR_I2C` and the caller decides what to do.

In FreeRTOS builds an optional `i2c_mutex` serializes bus access across
tasks. In bare-metal builds the mutex field is unused.

## Features

- Up to `FEB_TPS_MAX_DEVICES` devices (default 8)
- FreeRTOS auto-detection — same source builds on both runtimes
- Sign-magnitude current / shunt-voltage conversion
- Per-device shunt resistor and current range
- Optional EN / Power-Good GPIO wrappers
- Float, scaled-integer, or raw register access
- CONFIG + CAL readback verification on registration
- Injectable logging callback

## Logging

```c
#include "feb_tps.h"
#include "feb_log.h"

static void tps_log(FEB_TPS_LogLevel_t level, const char *msg) {
    switch (level) {
        case FEB_TPS_LOG_ERROR: LOG_E(TAG_TPS, "%s", msg); break;
        case FEB_TPS_LOG_WARN:  LOG_W(TAG_TPS, "%s", msg); break;
        case FEB_TPS_LOG_INFO:  LOG_I(TAG_TPS, "%s", msg); break;
        case FEB_TPS_LOG_DEBUG: LOG_D(TAG_TPS, "%s", msg); break;
        default: break;
    }
}

FEB_TPS_LibConfig_t cfg = {
    .log_func = tps_log,
    .log_level = FEB_TPS_LOG_INFO,
};
FEB_TPS_Init(&cfg);
```

Pass `NULL` to `FEB_TPS_Init()` for silent operation.

| Level | Value |
|-------|-------|
| `FEB_TPS_LOG_NONE`  | 0 |
| `FEB_TPS_LOG_ERROR` | 1 |
| `FEB_TPS_LOG_WARN`  | 2 |
| `FEB_TPS_LOG_INFO`  | 3 |
| `FEB_TPS_LOG_DEBUG` | 4 |

## Quick Start — single device (PCU, BMS)

```c
#include "feb_tps.h"

static FEB_TPS_Handle_t tps;

void setup(void) {
    FEB_TPS_Init(NULL);

    FEB_TPS_DeviceConfig_t cfg = {
        .hi2c = &hi2c1,
        .i2c_addr = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND),  /* 0x40 */
        .r_shunt_ohms = 0.012f,
        .i_max_amps = 4.0f,
        .name = "PCU",
    };
    if (FEB_TPS_DeviceRegister(&cfg, &tps) != FEB_TPS_OK) { /* handle error */ }
}

void loop(void) {
    FEB_TPS_Measurement_t m;
    if (FEB_TPS_Poll(tps, &m) == FEB_TPS_OK) {
        /* m.bus_voltage_v, m.current_a, m.power_w */
    }
}
```

## Multi-device (LVPDB)

Register each chip and poll them in a per-board loop. The library does not
expose batch-poll helpers — write the loop where the indexing into your
own per-rail arrays is obvious:

```c
for (uint8_t i = 0; i < NUM_DEVICES; i++) {
    if (tps_handles[i] == NULL) continue;
    uint16_t bv;
    int16_t cur;
    int16_t sv;
    if (FEB_TPS_PollRaw(tps_handles[i], &bv, &cur, &sv) == FEB_TPS_OK) {
        bus_voltage_raw[i] = bv;
        current_raw[i] = cur;
        shunt_voltage_raw[i] = sv;
    }
}
```

## I²C Address Calculation

Use `FEB_TPS_ADDR(A1, A0)`:

| Pin | Constant | Value |
|-----|----------|-------|
| GND | `FEB_TPS_PIN_GND` | 0 |
| VS  | `FEB_TPS_PIN_VS`  | 1 |
| SDA | `FEB_TPS_PIN_SDA` | 2 |
| SCL | `FEB_TPS_PIN_SCL` | 3 |

```c
FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND)  /* 0x40 */
FEB_TPS_ADDR(FEB_TPS_PIN_VS,  FEB_TPS_PIN_GND)  /* 0x44 */
FEB_TPS_ADDR(FEB_TPS_PIN_SCL, FEB_TPS_PIN_SDA)  /* 0x4E */
```

## API Reference

### Lifecycle

| Function | Description |
|----------|-------------|
| `FEB_TPS_Init(config)` | Initialize. NULL = silent, no mutex. In FreeRTOS, `config->i2c_mutex` is required. |
| `FEB_TPS_IsInitialized()` | True if `Init` has succeeded. |
| `FEB_TPS_DeviceRegister(config, handle)` | Write CONFIG + CAL, read both back, compare. |

### Measurement

| Function | Description |
|----------|-------------|
| `FEB_TPS_Poll(handle, measurement)` | Read all four registers, return floats. |
| `FEB_TPS_PollScaled(handle, scaled)` | Same but in mV / mA / µV / mW. |
| `FEB_TPS_PollRaw(handle, bv, cur, sv)` | Raw registers; current and shunt sign-corrected. Any output may be NULL. |

### GPIO

| Function | Description |
|----------|-------------|
| `FEB_TPS_Enable(handle, enable)` | Drive EN pin. Errors if no EN GPIO configured. |
| `FEB_TPS_ReadPowerGood(handle, state)` | Read PG pin. Errors if no PG GPIO configured. |

### Diagnostics

| Function | Description |
|----------|-------------|
| `FEB_TPS_ReadID(handle, id)` | Read device ID register (0xFF). |
| `FEB_TPS_GetCurrentLSB(handle)` | A/bit (`i_max / 32768`). |
| `FEB_TPS_GetDeviceName(handle)` | Returns the name passed at registration, or "Unknown". |
| `FEB_TPS_StatusToString(status)` | Map status enum to string. |
| `FEB_TPS_SignMagnitude(raw)` | Inline sign-magnitude → int16. |

## Data Structures

```c
typedef struct {
    float bus_voltage_v;
    float current_a;            /* signed */
    float shunt_voltage_mv;     /* signed */
    float power_w;
    /* Raw register values (sign-corrected for current and shunt) */
    uint16_t bus_voltage_raw;
    int16_t current_raw;
    int16_t shunt_voltage_raw;
    uint16_t power_raw;
} FEB_TPS_Measurement_t;

typedef struct {
    uint32_t bus_voltage_mv;
    int32_t current_ma;
    int32_t shunt_voltage_uv;
    uint32_t power_mw;          /* 32-bit handles 24 V @ 20 A = 480 000 mW */
} FEB_TPS_MeasurementScaled_t;
```

## Board Usage

| Board | Devices | Pattern |
|-------|---------|---------|
| BMS    | 1 @ 0x40, 2 mΩ, 5 A | FreeRTOS task at 1 Hz, `Poll` directly |
| PCU    | 1 @ 0x40, 12 mΩ, 4 A | bare-metal, `PollScaled` for CAN |
| LVPDB  | 7 across 0x40-0x4F, 2 mΩ, per-rail max | bare-metal, `PollRaw` per device |

## Configuration

`feb_tps_config.h`:

```c
#define FEB_TPS_MAX_DEVICES 8        /* slot count */
#define FEB_TPS_I2C_TIMEOUT_MS 100   /* timeout per HAL call */
/* #define FEB_TPS_USE_FREERTOS 1    -- usually auto-detected */
```

## Sign-Magnitude Format

TPS2482 current and shunt voltage registers are sign-magnitude (bit 15 =
sign, bits 14:0 = magnitude). The library handles this automatically;
`FEB_TPS_SignMagnitude()` is exposed inline for callers that read raw
registers themselves.

## Status Codes

- `FEB_TPS_OK`
- `FEB_TPS_ERR_INVALID_ARG`
- `FEB_TPS_ERR_I2C` — HAL_I2C call returned non-OK
- `FEB_TPS_ERR_NOT_INIT` — `FEB_TPS_Init` not called, or device not yet registered
- `FEB_TPS_ERR_CONFIG_MISMATCH` — CONFIG or CAL readback differed from write
- `FEB_TPS_ERR_MAX_DEVICES` — `FEB_TPS_MAX_DEVICES` already registered

## See also

- [`common/README.md`](../README.md) — library index
