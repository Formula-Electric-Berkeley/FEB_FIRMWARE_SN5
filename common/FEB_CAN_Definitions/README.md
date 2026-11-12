# FEB CAN Library SN4

CAN message definitions and code generation for the FEB SN5 Formula Electric vehicle.

## Quick Start

```bash
cd common/FEB_CAN_Definitions
./generate_can.sh
```

The script automatically:
- Creates a Python virtual environment
- Installs the correct version of cantools (39.4.13)
- Validates message definitions
- Generates the DBC file from Python message definitions
- Generates C source files from the DBC
- Normalizes timestamps for consistent diffs

## Commands Reference

| Option | Description |
|--------|-------------|
| `./generate_can.sh` | Generate all CAN library files |
| `./generate_can.sh -l, --list` | List all messages with their frame IDs |
| `./generate_can.sh -i, --ids` | Show frame ID allocation map |
| `./generate_can.sh -v, --validate` | Validate definitions without generating |
| `./generate_can.sh -d, --dump` | Visualize the DBC file contents |
| `./generate_can.sh -c, --check` | CI mode: verify files are up to date |
| `./generate_can.sh -h, --help` | Show help message |

## Adding a New CAN Message

### Step 1: Define the Message Function

Create or edit a `*_messages.py` file for your board. Each message is a function that takes a `frame_id` and returns a `cantools.db.Message`.

```python
# Example: lvpdb_messages.py
import cantools

def get_my_new_message(frame_id: int):
    """My new LVPDB message for XYZ data."""

    signal1 = cantools.db.Signal(
        name="my_signal",
        start=0,           # Bit position (0-63 for 8-byte message)
        length=16,         # Bit length
        byte_order="little_endian",
        is_signed=False,   # True for signed integers
    )

    signal2 = cantools.db.Signal(
        name="another_signal",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=True,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="my_message_name",  # Used in C code: feb_can_my_message_name_pack()
        length=4,                # Bytes (1-8)
        signals=[signal1, signal2],
        comment="Description of this message",
        strict=True
    )

    return msg
```

### Step 2: Register in `generate.py`

Add an entry to `MESSAGE_REGISTRY` in `generate.py`:

```python
MESSAGE_REGISTRY: Dict[int, Tuple[Callable, str]] = {
    # ... existing entries ...

    # Add your new message with an available frame ID
    0x19: (lvpdb_msg.get_my_new_message, "My new data message"),
}
```

### Step 3: Regenerate

```bash
./generate_can.sh
```

### Step 4: Verify

```bash
./generate_can.sh --list    # See your message in the list
./generate_can.sh --dump    # See the DBC representation
```

## Frame ID Allocation Strategy

Frame IDs are grouped by board/subsystem. Each board has a reserved range with room for expansion.

| Range | Owner | Current Use | Notes |
|-------|-------|-------------|-------|
| 0x00-0x08 | BMS | 5 messages | Battery management |
| 0x09-0x0F | PCU | 2 messages | Safety-critical (brake, BSPD) |
| 0x10-0x15 | DASH | 1 message | Dashboard I/O |
| 0x16-0x1D | LVPDB | 3 messages | Power distribution |
| 0x1E-0x2C | Sensors | 8 messages | Tire temp, WSS, linear pots |
| 0x2D-0x33 | DART | 2 messages | Fan control |
| 0x34-0x3F | TPS | 4 messages | Power monitoring chips |
| 0xC0-0xCF | RMS | 2 messages | Inverter communication |
| 0xD0-0xDF | Heartbeats | 6 messages | Board health monitoring |
| 0xE0-0xEF | Debug | 4 messages | Test/debug messages |
| 0xFF | PCU | 1 message | Raw ADC values |

**To find available IDs**: Run `./generate_can.sh --ids`

## Signal Definition Patterns

### Common Signal Types

```python
# 16-bit unsigned integer (0-65535)
cantools.db.Signal(
    name="voltage_mv",
    start=0, length=16,
    byte_order="little_endian",
    is_signed=False
)

# 16-bit signed integer (-32768 to 32767)
cantools.db.Signal(
    name="temperature_c",
    start=16, length=16,
    byte_order="little_endian",
    is_signed=True
)

# Single bit flag
cantools.db.Signal(
    name="fault_flag",
    start=32, length=1,
    byte_order="little_endian",
    is_signed=False
)

# 8-bit state/enum
cantools.db.Signal(
    name="state",
    start=0, length=8,
    byte_order="little_endian",
    is_signed=False
)

# 32-bit counter
cantools.db.Signal(
    name="counter",
    start=0, length=32,
    byte_order="little_endian",
    is_signed=False
)
```

### Bit Positioning

Signals are positioned in little-endian bit order:

```
Byte:  |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |
Bits:  0-7   8-15  16-23 24-31 32-39 40-47 48-55 56-63

Example 8-byte message with mixed signals:
  Signal A: start=0,  length=16 -> bytes 0-1
  Signal B: start=16, length=8  -> byte 2
  Signal C: start=24, length=8  -> byte 3
  Signal D: start=32, length=32 -> bytes 4-7
```

### Heartbeat Pattern (64 error flags)

Used for board health monitoring. See `lvpdb_messages.py:get_lvpdb_heartbeat()` for the standard pattern with 64 individual error flags.

## Using Generated Code in Firmware

The generated files in `gen/` provide pack/unpack functions:

```c
#include "feb_can_db.h"

// Sending a message
struct feb_can_lvpdb_flags_bus_voltage_lv_current_t data = {
    .flags = my_flags,
    .bus_voltage = voltage_mv,
    .lv_current = current_ma,
};
uint8_t buffer[FEB_CAN_LVPDB_FLAGS_BUS_VOLTAGE_LV_CURRENT_LENGTH];
feb_can_lvpdb_flags_bus_voltage_lv_current_pack(buffer, &data, sizeof(buffer));
// Send buffer via CAN peripheral

// Receiving a message
struct feb_can_bms_state_t rx_data;
feb_can_bms_state_unpack(&rx_data, rx_buffer, rx_len);
// Use rx_data.bms_state, rx_data.relay_state, etc.
```

## CI/CD Integration

The GitHub Actions workflow uses `--check` mode to verify generated files:

```yaml
- name: Verify CAN Library
  run: |
    cd common/FEB_CAN_Definitions
    ./generate_can.sh --check
```

This ensures Python definitions match the committed C code.

## Files and Directories

```
FEB_CAN_Definitions/
├── generate_can.sh           # Main entry point
├── generate.py               # Message registry and generation logic
├── README.md                 # This file
├── *_messages.py             # Message definitions by board
│   ├── bms_messages.py
│   ├── pcu_messages.py
│   ├── dash_messages.py
│   ├── lvpdb_messages.py
│   ├── dcu_message.py
│   ├── sensor_nodes_messsages.py
│   ├── dart_messages.py
│   └── ...
├── external_dbc/             # Vendor DBCs — every *.dbc here is auto-merged
│   ├── inverter.dbc         # Cascadia PM100 inverter
│   └── elcon.dbc            # Elcon / HK charger
├── gen/                      # Generated files (DO NOT EDIT)
│   ├── FEB_CAN.dbc          # DBC database file
│   ├── feb_can_db.h         # Codec (pack/unpack) + latest-value database
│   └── feb_can_db.c         # Implementation of both
└── .venv/                    # Python virtual environment (auto-created)
```

## Prerequisites

- Python 3.8 or higher
- pip

The script handles all other dependencies automatically.

## Troubleshooting

### Error: `ModuleNotFoundError: No module named 'cantools'`
Run `./generate_can.sh` which will set up the environment automatically.

### Error: `TypeError: Message.__init__() got an unexpected keyword argument 'strict'`
You have an incompatible version of cantools. Delete `.venv/` and run `./generate_can.sh` to recreate with the correct version.

### Error: `error: externally-managed-environment`
Run `./generate_can.sh` which uses a virtual environment automatically.

### CI check fails: "files are out of date"
Regenerate locally and commit:
```bash
cd common/FEB_CAN_Definitions
./generate_can.sh
git add gen/
git commit -m "Regenerate CAN library"
```

### Signal values seem wrong after unpacking
Check byte order and signedness in your signal definition. FEB uses little-endian throughout.

### Cannot find my message's pack function
Message names are normalized to C identifiers. Check `gen/feb_can_db.h` for the exact function name (underscores, lowercase).

### Validation error: "Duplicate function"
The same message function is registered at multiple frame IDs. Each function should only be registered once.

### Validation error: "Frame ID out of range"
Frame IDs must be in valid CAN range (0x000-0x7FF for standard CAN).

## External Resources

- [cantools Documentation](https://cantools.readthedocs.io/)
- [DBC File Format](https://www.csselectronics.com/pages/can-dbc-file-database-intro)
- [CAN Bus Tutorial](https://www.csselectronics.com/pages/can-bus-simple-intro-tutorial)
