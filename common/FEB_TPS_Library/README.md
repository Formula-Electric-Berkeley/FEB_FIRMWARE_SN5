# FEB TPS2482 Library

Common library for TPS2482 power monitoring ICs used across Formula Electric @ Berkeley boards.

## Features

- **Multi-device support** - Up to 8 TPS2482 devices per board
- **FreeRTOS auto-detection** - Thread-safe I2C access with mutex protection when FreeRTOS is detected
- **Correct sign-magnitude conversion** - Properly handles TPS2482's sign-magnitude current format
- **Per-device calibration** - Configure different shunt resistors and current ranges per device
- **GPIO integration** - Optional EN/Power-Good/Alert pin control
- **Flexible measurement API** - Float, scaled integer, or raw register access

## Quick Start

### Single Device (PCU, BMS)

```c
#include "feb_tps.h"

static FEB_TPS_Handle_t tps;

void setup(void) {
    // Initialize library
    FEB_TPS_Init(NULL);

    // Configure and register device
    FEB_TPS_DeviceConfig_t cfg = {
        .hi2c = &hi2c1,
        .i2c_addr = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND),  // 0x40
        .r_shunt_ohms = 0.012f,  // 12 mOhm
        .i_max_amps = 4.0f,      // 4A fuse
        .name = "PCU",
    };
    FEB_TPS_DeviceRegister(&cfg, &tps);
}

void loop(void) {
    FEB_TPS_Measurement_t data;
    if (FEB_TPS_Poll(tps, &data) == FEB_TPS_OK) {
        float voltage = data.bus_voltage_v;
        float current = data.current_a;
        float power = data.power_w;
    }
}
```

### Multiple Devices (LVPDB)

```c
#include "feb_tps.h"

#define NUM_DEVICES 7
static FEB_TPS_Handle_t tps_handles[NUM_DEVICES];

void setup(void) {
    FEB_TPS_Init(NULL);

    // Register each device with its specific configuration
    for (int i = 0; i < NUM_DEVICES; i++) {
        FEB_TPS_DeviceConfig_t cfg = {
            .hi2c = &hi2c1,
            .i2c_addr = device_addresses[i],
            .r_shunt_ohms = R_SHUNT,
            .i_max_amps = device_fuse_max[i],
            .en_gpio_port = en_ports[i],
            .en_gpio_pin = en_pins[i],
            .name = device_names[i],
        };
        FEB_TPS_DeviceRegister(&cfg, &tps_handles[i]);
    }
}

void loop(void) {
    // Poll all devices at once
    uint16_t bus_v[NUM_DEVICES], current[NUM_DEVICES], shunt_v[NUM_DEVICES];
    FEB_TPS_PollAllRaw(bus_v, current, shunt_v, NUM_DEVICES);
}
```

## I2C Address Calculation

Use the `FEB_TPS_ADDR(A1, A0)` macro with pin options:

| Pin Setting | Constant | Value |
|-------------|----------|-------|
| GND | `FEB_TPS_PIN_GND` | 0x00 |
| VS  | `FEB_TPS_PIN_VS`  | 0x01 |
| SDA | `FEB_TPS_PIN_SDA` | 0x02 |
| SCL | `FEB_TPS_PIN_SCL` | 0x03 |

Examples:
```c
// A1=GND, A0=GND -> 0x40
FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND)

// A1=VS, A0=GND -> 0x44
FEB_TPS_ADDR(FEB_TPS_PIN_VS, FEB_TPS_PIN_GND)

// A1=SCL, A0=SDA -> 0x4E
FEB_TPS_ADDR(FEB_TPS_PIN_SCL, FEB_TPS_PIN_SDA)
```

## API Reference

### Initialization

| Function | Description |
|----------|-------------|
| `FEB_TPS_Init(config)` | Initialize library (pass NULL for defaults) |
| `FEB_TPS_DeInit()` | Deinitialize library and all devices |
| `FEB_TPS_DeviceRegister(config, handle)` | Register a TPS2482 device |
| `FEB_TPS_DeviceUnregister(handle)` | Unregister a device |

### Measurement (Single Device)

| Function | Description |
|----------|-------------|
| `FEB_TPS_Poll(handle, measurement)` | Read all values (float) |
| `FEB_TPS_PollScaled(handle, scaled)` | Read all values (integer, for CAN) |
| `FEB_TPS_PollBusVoltage(handle, voltage)` | Read bus voltage only |
| `FEB_TPS_PollCurrent(handle, current)` | Read current only |
| `FEB_TPS_PollRaw(handle, bus_v, current, shunt_v)` | Read raw registers |

### Measurement (Batch)

| Function | Description |
|----------|-------------|
| `FEB_TPS_PollAll(measurements, count)` | Poll all devices (float) |
| `FEB_TPS_PollAllScaled(scaled, count)` | Poll all devices (integer) |
| `FEB_TPS_PollAllRaw(bus_v, current, shunt_v, count)` | Poll all devices (raw) |

### GPIO Control

| Function | Description |
|----------|-------------|
| `FEB_TPS_Enable(handle, enable)` | Control EN pin |
| `FEB_TPS_ReadPowerGood(handle, state)` | Read PG pin status |
| `FEB_TPS_ReadAlert(handle, active)` | Read Alert pin status |
| `FEB_TPS_EnableAll(enable)` | Enable/disable all devices |

### Utility

| Function | Description |
|----------|-------------|
| `FEB_TPS_StatusToString(status)` | Convert status code to string |
| `FEB_TPS_GetDeviceName(handle)` | Get device name |
| `FEB_TPS_SignMagnitude(raw)` | Convert raw register to signed |
| `FEB_TPS_GetCurrentLSB(handle)` | Get device's current LSB value |
| `FEB_TPS_ReadID(handle, id)` | Read device unique ID |

## Data Structures

### FEB_TPS_Measurement_t (Floating Point)

```c
typedef struct {
    float bus_voltage_v;        // Bus voltage in Volts
    float current_a;            // Current in Amps (signed)
    float shunt_voltage_mv;     // Shunt voltage in millivolts
    float power_w;              // Power in Watts
    // Raw values also included for CAN/debugging
} FEB_TPS_Measurement_t;
```

### FEB_TPS_MeasurementScaled_t (Integer for CAN)

```c
typedef struct {
    uint16_t bus_voltage_mv;    // Bus voltage in millivolts
    int16_t current_ma;         // Current in milliamps
    int32_t shunt_voltage_uv;   // Shunt voltage in microvolts
    uint16_t power_mw;          // Power in milliwatts
} FEB_TPS_MeasurementScaled_t;
```

## Board Usage

### PCU
- Single TPS2482 at 0x40 (A0=GND, A1=GND)
- 12 mOhm shunt, 4A max
- Rate-limited polling at 10 Hz

### BMS
- Single TPS2482 at 0x40
- 2 mOhm shunt, 5A max
- FreeRTOS task at 1 Hz

### LVPDB
- 7 TPS2482 devices with different I2C addresses
- Per-device EN/PG/Alert GPIO control
- Batch polling for efficiency

## Configuration

Edit `feb_tps_config.h` to customize:

```c
// Maximum number of registered devices (default: 8)
#define FEB_TPS_MAX_DEVICES 8

// FreeRTOS detection (auto-detected, or manually override)
// #define FEB_TPS_USE_FREERTOS 1
```

## Sign-Magnitude Format

The TPS2482 uses sign-magnitude format for current and shunt voltage registers:
- Bit 15 = sign (0 = positive, 1 = negative)
- Bits 14:0 = magnitude

Use `FEB_TPS_SignMagnitude()` for manual conversion:

```c
uint16_t raw = 0x8064;  // Negative 100
int16_t signed_val = FEB_TPS_SignMagnitude(raw);  // Returns -100
```

## Error Handling

```c
FEB_TPS_Status_t status = FEB_TPS_Poll(handle, &data);
if (status != FEB_TPS_OK) {
    LOG_E("TPS error: %s", FEB_TPS_StatusToString(status));
}
```

Status codes:
- `FEB_TPS_OK` - Success
- `FEB_TPS_ERR_INVALID_ARG` - Invalid argument
- `FEB_TPS_ERR_I2C` - I2C communication error
- `FEB_TPS_ERR_NOT_INIT` - Library/device not initialized
- `FEB_TPS_ERR_CONFIG_MISMATCH` - Config readback mismatch
- `FEB_TPS_ERR_MAX_DEVICES` - Maximum device count exceeded
