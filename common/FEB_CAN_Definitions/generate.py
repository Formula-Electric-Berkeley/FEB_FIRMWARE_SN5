#!/usr/bin/env python3
"""
FEB CAN Library Generator

This script generates the FEB_CAN.dbc file from Python message definitions.
Run via ./generate_can.sh rather than directly.

Usage:
    python generate.py                  # Generate DBC file (default)
    python generate.py -l, --list       # List all messages with their IDs
    python generate.py -i, --ids        # Show frame ID allocation map
    python generate.py -v, --validate   # Validate message registry
    python generate.py --emit-db DIR     # Consolidate cantools output into gen/feb_can_db.{h,c}
    python generate.py -h, --help       # Show help
"""
import glob
import re
import cantools
import os
import sys
from dataclasses import dataclass
from typing import Callable, Dict, List, Tuple

EXTERNAL_DBC_DIR = os.path.join(os.path.dirname(__file__), "external_dbc")
_VECTOR_INDEPENDENT_ID = 3221225472     # cantools sentinel, not a real CAN frame

BUS_BITRATE = 500_000
BUS_LOAD_CEILING = 0.40
TOTAL_LOAD_WARN = 0.60

FD_BUS = False

NODES = ["BMS", "PCU", "DASH", "DCU", "LVPDB", "DART", "SN_FRONT", "SN_REAR", "IVT"]


def _frame_bits(msg) -> int:
    """Worst case: frame overhead plus one stuff bit per 4 stuffable bits."""
    overhead = 67 if msg.is_extended_frame else 47
    raw = overhead + 8 * msg.length
    return raw + (raw - 13) // 4


def validate_bus_load(messages: List[cantools.db.Message]) -> bool:
    """Gate on traffic we author. External rates come from vendor DBCs and are
    reported but not gated, since no change here can lower them."""
    ours = theirs = 0.0
    for m in messages:
        if not m.cycle_time:
            continue
        bps = _frame_bits(m) * (1000.0 / m.cycle_time)
        if m.frame_id in MESSAGE_REGISTRY:
            ours += bps
        else:
            theirs += bps

    k = BUS_BITRATE // 1000
    print(f"[INFO] periodic bus load @{k}k: "
          f"{ours / BUS_BITRATE:.1%} FEB + {theirs / BUS_BITRATE:.1%} external "
          f"= {(ours + theirs) / BUS_BITRATE:.1%}")

    if ours / BUS_BITRATE > BUS_LOAD_CEILING:
        print(f"[ERROR] FEB periodic traffic exceeds the "
              f"{BUS_LOAD_CEILING:.0%} budget", file=sys.stderr)
        return False
    if (ours + theirs) / BUS_BITRATE > TOTAL_LOAD_WARN:
        print(f"[WARN] total periodic load is above {TOTAL_LOAD_WARN:.0%}; "
              f"inverter broadcast rates dominate and are configurable on the "
              f"controller", file=sys.stderr)
    return True


# CANopen predefined connection set — these IDs MUST NOT be used for custom messages.
# 0x000 NMT | 0x080 SYNC | 0x081 EMCY node-1 | 0x100 TIME
# 0x181 TPDO1-n1 | 0x201 RPDO1-n1 | 0x581 SDO-tx-n1 | 0x601 SDO-rx-n1
# 0x701 NMT heartbeat n1 | 0x7E4 LSS-tx | 0x7E5 LSS-rx
CANOPEN_RESERVED_IDS: set = {
    0x000, 0x080, 0x081, 0x100, 0x181, 0x201,
    0x581, 0x601, 0x701, 0x7E4, 0x7E5,
}


def external_dbc_files() -> List[str]:
    # Sorted so --check stays deterministic.
    if not os.path.isdir(EXTERNAL_DBC_DIR):
        print(f"[WARN] {EXTERNAL_DBC_DIR} not found: skipping external merge",
              file=sys.stderr)
        return []
    return sorted(glob.glob(os.path.join(EXTERNAL_DBC_DIR, "*.dbc")))


def load_external_messages() -> List[Tuple[str, cantools.db.Message]]:
    """Returns (source_filename, message) pairs in (filename, frame_id) order."""
    out: List[Tuple[str, cantools.db.Message]] = []

    for path in external_dbc_files():
        name = os.path.basename(path)
        db = cantools.db.load_file(path)
        if not isinstance(db, cantools.db.Database):
            raise TypeError(f"{path} is not a CAN database")
        msgs = [m for m in db.messages if m.frame_id != _VECTOR_INDEPENDENT_ID]
        out.extend((name, m) for m in sorted(msgs, key=lambda m: m.frame_id))

    return out


def external_message_counts() -> Dict[str, int]:
    counts: Dict[str, int] = {}
    for name, _ in load_external_messages():
        counts[name] = counts.get(name, 0) + 1
    return counts


# CAN message modules
from msg_defs import bms_messages as bms_msg
from msg_defs import pcu_messages as pcu_msg
from msg_defs import dash_messages as dash_msg
from msg_defs import lvpdb_messages as lvpdb_msg
from msg_defs import dcu_message as dcu_msg
from msg_defs import sensor_nodes_messages as sensor_msg
from msg_defs import dart_messages as dart_msg
from msg_defs import ivt_messages as ivt_msg
from msg_defs import ping_pong_messages as ping_pong_msg
from msg_defs import res_messages as res_msg

# =============================================================================
# MESSAGE REGISTRY
# =============================================================================
# Each entry maps: frame_id -> (message_function, description)
# Frame IDs are grouped by board/subsystem with reserved ranges for expansion.
#
# ID ALLOCATION STRATEGY:
#   0x00:       CANopen NMT — RESERVED, DO NOT USE
#   0x01-0x09:  BMS messages (5 used, 4 reserved)
#   0x09-0x0F:  PCU safety-critical (brake, BSPD, RES) (3 used, 4 reserved)
#   0x10-0x15:  DASH messages (1 used, 5 reserved)
#   0x16-0x1D:  LVPDB messages (3 used, 5 reserved)
#   0x1E-0x2C:  Sensor node messages — base (FRONT/REAR WSS, IMU, mag, tire/linpot) (12 used, 1 reserved)
#   0x2D-0x33:  DART messages (2 used, 5 reserved)
#   0x34-0x3F:  TPS chip / PCU ADC (5 used, 7 reserved)
#   0x40-0x4F:  Sensor nodes FRONT (extended): GPS / Fusion / steering
#   0x50-0x5F:  Sensor nodes REAR (extended): GPS / Fusion
#   0xC0-0xCF:  PCU RMS commands (2 used, 14 reserved)
#   0xD0-0xDF:  Heartbeat messages (6 used, 10 reserved)
#   0xE0-0xEF:  Debug/test messages (4 used, 12 reserved)
#   0x500-0x50F: EBS / Driverless safety (1 used, 15 reserved)
#   0x520-0x52F: IVT-S current/voltage sensor (5 used, 11 reserved; 0x526-0x528 reserved)
#   0x7C0-0x7CF: Low-priority bulk telemetry (1 used, 15 reserved)
# =============================================================================

MESSAGE_REGISTRY: Dict[int, Tuple[Callable[[int], cantools.db.Message], str]] = {
    # ----- BMS Messages (0x01-0x09) -----
    # NOTE: 0x00 is CANopen NMT — RESERVED, DO NOT USE (see CANOPEN_RESERVED_IDS)
    0x02: (bms_msg.get_accumulator_voltage, "Accumulator pack voltage"),
    0x03: (bms_msg.get_accumulator_temperature, "Accumulator pack temperature"),
    0x05: (bms_msg.get_bms_state, "General BMS info"),
    # 0x06-0x09: Reserved for future BMS messages

    # ----- PCU Safety Messages (0x09-0x0F) -----
    0x09: (pcu_msg.normalized_brake, "Normalized brake pressure"),
    0x0A: (pcu_msg.bspd, "BSPD state"),
    0x0B: (res_msg.get_res_state, "RES state"),
    # 0x0C-0x0F: Reserved for future PCU safety messages

    # ----- DASH Messages (0x10-0x15) -----
    0x10: (dash_msg.get_dash_state, "Dashboard state + buttons and switches"),
    # 0x11-0x15: Reserved for future DASH messages

    # ----- LVPDB Messages (0x16-0x1D) -----
    0x16: (lvpdb_msg.get_lvpdb_lv_24v_bus_and_12v_bus_voltages, "Voltages for LV 24V bus & LV 12V bus"),
    0x17: (lvpdb_msg.get_lvpdb_lv_sh_af1_af2_cp_currents, "Currents for LV, SH, AF1_AF2, CP"),
    0x18: (lvpdb_msg.get_lvpdb_rf_current, "Current for RF"),
    # 0x19-0x1D: Reserved for future LVPDB messages

    # ----- Sensor Node Messages (0x1E-0x2C) -----
    0x1E: (lambda fid: sensor_msg.get_linpot(fid, "front"), "Front linear potentiometer"),
    0x1F: (lambda fid: sensor_msg.get_linpot(fid, "rear"), "Rear linear potentiometer"),
    0x20: (lambda fid: sensor_msg.get_tire_temp_left(fid, "front"), "Front left tire temperature"),
    0x21: (lambda fid: sensor_msg.get_tire_temp_right(fid, "front"), "Front right tire temperature"),
    0x22: (lambda fid: sensor_msg.get_tire_temp_left(fid, "rear"), "Rear left tire temperature"),
    0x23: (lambda fid: sensor_msg.get_tire_temp_right(fid, "rear"), "Rear right tire temperature"),

    # WSS (Wheel Speed Sensors):
    # FRONT
    0x24: (lambda fid: sensor_msg.get_wss(fid, "front"), "[WSS][FRONT] wheel speed sensor data"),
    # REAR
    0x25: (lambda fid: sensor_msg.get_wss(fid, "rear"), "[WSS][REAR] wheel speed sensor data"),

    # IMU: accelerometer/gyro
    # FRONT
    0x26: (lambda fid: sensor_msg.get_imu_accel(fid, "front"), "[IMU][FRONT] accelerometer data (raw)"),
    0x28: (lambda fid: sensor_msg.get_imu_gyro(fid, "front"), "[IMU][FRONT] gyroscope data (raw)"),
    # REAR
    0x27: (lambda fid: sensor_msg.get_imu_accel(fid, "rear"), "[IMU][REAR] accelerometer data (raw)"),
    0x29: (lambda fid: sensor_msg.get_imu_gyro(fid, "rear"), "[IMU][REAR] gyroscope data (raw)"),

    # Magnetometer
    # FRONT
    0x2A: (lambda fid: sensor_msg.get_mag(fid, "front"), "[Magnetometer][FRONT] data (raw)"),
    # REAR
    0x2B: (lambda fid: sensor_msg.get_mag(fid, "rear"), "[Magnetometer][REAR] data (raw)"),
    # 0x2C: Reserved for future sensor messages

    # ----- DART Messages (0x2D-0x33) -----
    0x2D: (dart_msg.get_measured_fan_speeds_1234, "DART fan speeds 1-4"),
    0x2E: (dart_msg.get_measured_fan_speeds_5, "DART fan speed 5"),
    # 0x2F-0x33: Reserved for future DART messages

    # ----- TPS Chip Messages (0x34-0x3F) -----
    0x34: (bms_msg.get_tps_voltage_current, "BMS TPS voltage/current"),
    0x35: (pcu_msg.get_tps_voltage_current, "PCU TPS voltage/current"),
    0x36: (dash_msg.get_tps_voltage_current, "DASH TPS voltage/current"),
    0x37: (dcu_msg.get_tps_voltage_current, "DCU TPS voltage/current"),
    0x38: (pcu_msg.get_raw_acc, "PCU raw accelerator ADC"),
    0x39: (pcu_msg.get_pedal_voltages, "PCU raw pedal sensor voltages (mV)"),
    # 0x3A-0x3F: Reserved for future TPS / PCU ADC messages

    # ----- Sensor Nodes FRONT (extended): GPS / Fusion / steering (0x40-0x4F) -----
    0x40: (lambda fid: sensor_msg.get_gps_pos(fid, "front"), "[GPS][FRONT] latitude/longitude"),
    0x41: (lambda fid: sensor_msg.get_gps_altitude(fid, "front"), "[GPS][FRONT] altitude + HDOP/VDOP"),
    0x42: (lambda fid: sensor_msg.get_gps_motion(fid, "front"), "[GPS][FRONT] speed and course"),
    0x43: (lambda fid: sensor_msg.get_gps_time(fid, "front"), "[GPS][FRONT] time data (UTC)"),
    0x44: (lambda fid: sensor_msg.get_gps_date(fid, "front"), "[GPS][FRONT] date data (UTC)"),
    0x45: (lambda fid: sensor_msg.get_gps_status(fid, "front"), "[GPS][FRONT] fix quality, satellite counts, PDOP"),
    0x46: (lambda fid: sensor_msg.get_thermistor(fid, "front"), "[Thermistor][FRONT] coolant temperatures + fault flags"),
    0x47: (lambda fid: sensor_msg.get_fusion_quat(fid, "front"), "[Fusion][FRONT] orientation quaternion"),
    0x48: (lambda fid: sensor_msg.get_fusion_euler(fid, "front"), "[Fusion][FRONT] Euler angles"),
    0x49: (lambda fid: sensor_msg.get_fusion_lin_accel(fid, "front"), "[Fusion][FRONT] linear acceleration (body frame)"),
    0x4A: (lambda fid: sensor_msg.get_fusion_earth_accel(fid, "front"), "[Fusion][FRONT] linear acceleration (earth frame)"),
    0x4B: (lambda fid: sensor_msg.get_fusion_status(fid, "front"), "[Fusion][FRONT] internal flags + rejection errors"),
    0x4E: (lambda fid: sensor_msg.get_steer(fid, "front"), "[Steering][FRONT] angle, AGC gain, magnet status + magnitude"),
    0x4F: (lambda fid: sensor_msg.get_strain_gauge(fid, "front"), "Strain Gauges"),

    # ----- Sensor Nodes REAR (extended): GPS / Fusion (0x50-0x5F) -----
    0x50: (lambda fid: sensor_msg.get_gps_pos(fid, "rear"), "[GPS][REAR] latitude/longitude"),
    0x51: (lambda fid: sensor_msg.get_gps_altitude(fid, "rear"), "[GPS][REAR] altitude + HDOP/VDOP"),
    0x52: (lambda fid: sensor_msg.get_gps_motion(fid, "rear"), "[GPS][REAR] speed and course"),
    0x53: (lambda fid: sensor_msg.get_gps_time(fid, "rear"), "[GPS][REAR] time data (UTC)"),
    0x54: (lambda fid: sensor_msg.get_gps_date(fid, "rear"), "[GPS][REAR] date data (UTC)"),
    0x55: (lambda fid: sensor_msg.get_gps_status(fid, "rear"), "[GPS][REAR] fix quality, satellite counts, PDOP"),
    0x56: (lambda fid: sensor_msg.get_thermistor(fid, "rear"), "[Thermistor][REAR] coolant temperatures + fault flags"),
    0x57: (lambda fid: sensor_msg.get_fusion_quat(fid, "rear"), "[Fusion][REAR] orientation quaternion"),
    0x58: (lambda fid: sensor_msg.get_fusion_euler(fid, "rear"), "[Fusion][REAR] Euler angles"),
    0x59: (lambda fid: sensor_msg.get_fusion_lin_accel(fid, "rear"), "[Fusion][REAR] linear acceleration (body frame)"),
    0x5A: (lambda fid: sensor_msg.get_fusion_earth_accel(fid, "rear"), "[Fusion][REAR] linear acceleration (earth frame)"),
    0x5B: (lambda fid: sensor_msg.get_fusion_status(fid, "rear"), "[Fusion][REAR] internal flags + rejection errors"),
    0x5C: (lambda fid: sensor_msg.get_strain_gauge(fid, "rear"), "Strain Gauges"),
    # 0x5D-0x5F: Reserved for future REAR extended sensor messages

    0x60: (pcu_msg.get_adc_debug, "PCU ADC Debug"),


    # ----- RMS/Inverter Messages (0xA0-0xCF) -----
    # IMMUTABLE: All IDs in this block come verbatim from external_dbc/inverter.dbc (Cascadia PM100).
    # Do not add duplicate entries here; external_dbc/inverter.dbc is the single source of truth.
    # 0xC0 (M192_Command_Message) and 0xC1 (M193_Read_Write_Param_Command)
    # come from external_dbc/inverter.dbc — do not duplicate here.
    # 0xC2-0xCF: Reserved for future RMS messages

    # ----- Heartbeat Messages (0xD0-0xDF) -----
    0xD0: (pcu_msg.get_pcu_heartbeat, "PCU heartbeat"),
    0xD1: (dash_msg.get_dash_heartbeat, "DASH heartbeat"),
    0xD2: (lvpdb_msg.get_lvpdb_heartbeat, "LVPDB heartbeat"),
    0xD3: (dcu_msg.get_dcu_heartbeat, "DCU heartbeat"),
    0xD4: (lambda fid: sensor_msg.get_heartbeat(fid, "front"), "Front sensor node heartbeat"),
    0xD5: (lambda fid: sensor_msg.get_heartbeat(fid, "rear"), "Rear sensor node heartbeat"),
    # 0xD6-0xDF: Reserved for future heartbeats

    # ----- Debug/Test Messages (0xE0-0xEF) -----
    0xE0: (ping_pong_msg.get_ping_pong_counter1, "Ping pong counter 1"),
    0xE1: (ping_pong_msg.get_ping_pong_counter2, "Ping pong counter 2"),
    0xE2: (ping_pong_msg.get_ping_pong_counter3, "Ping pong counter 3"),
    0xE3: (ping_pong_msg.get_ping_pong_counter4, "Ping pong counter 4"),
    # 0xE4-0xEF: Reserved for future debug messages

    # ----- EBS / Driverless Safety (0x500-0x50F) -----
    # IMMUTABLE frame ID: 0x500 is the CANopen PDO-Tx slot used by the EBS ECU.
    # Cycle time is fixed at 100 ms per CANopen specification — do not reduce it.
    0x500: (pcu_msg.ebs_pressure_status, "EBS pressure status (4 sensors)"),
    # 0x501-0x50F: Reserved for future EBS / driverless safety messages

    # ----- IVT-S Current/Voltage Sensor (0x520-0x52F) -----
    # Isabellenhutte IVT-S broadcasts these; the BMS receives and decodes them.
    # Fixed frame IDs configured on the sensor — do not remap.
    0x521: (ivt_msg.get_ivt_current, "IVT-S pack current (raw int32, mA)"),
    0x522: (ivt_msg.get_ivt_voltage_1, "IVT-S voltage 1 / pack voltage (raw int32, mV)"),
    0x523: (ivt_msg.get_ivt_voltage_2, "IVT-S voltage 2 (raw int32, mV)"),
    0x524: (ivt_msg.get_ivt_voltage_3, "IVT-S voltage 3 (raw int32, mV)"),
    0x525: (ivt_msg.get_ivt_temperature, "IVT-S temperature (raw int32, 0.1 degC)"),
    # 0x526-0x528: Reserved (IVT-S power / coulomb counter / energy counter, not yet decoded)

    0x7C0: (bms_msg.get_cell_data, "BMS accumulator cell voltage + 3 temperatures (one cell per frame)"),
}

# Frame ID allocation ranges for documentation and validation
ID_RANGES = [
    (0x01, 0x09, "BMS"),
    (0x09, 0x0F, "PCU Safety"),
    (0x10, 0x15, "DASH"),
    (0x16, 0x1D, "LVPDB"),
    (0x1E, 0x2C, "Sensor Nodes"),
    (0x2D, 0x33, "DART"),
    (0x34, 0x3F, "TPS Chips / PCU ADC"),
    (0x40, 0x4F, "Sensor Nodes FRONT (extended): GPS / Fusion / steering"),
    (0x50, 0x5F, "Sensor Nodes REAR (extended): GPS / Fusion"),
    (0x60, 0x60, "debug"),
    (0xA0, 0xC2, "Inverter (Cascadia PM100, via external_dbc/inverter.dbc)"),
    (0xC0, 0xCF, "RMS/Inverter"),
    (0xD0, 0xDF, "Heartbeats"),
    (0xE0, 0xEF, "Debug/Test"),
    (0x500, 0x50F, "EBS / Driverless Safety"),
    (0x520, 0x52F, "IVT-S current/voltage sensor"),
    (0x7C0, 0x7CF, "Low-priority bulk telemetry"),
]

def validate_registry() -> bool:
    """Validate the message registry for common errors."""
    errors = []

    # Check for CANopen reserved IDs
    for frame_id in MESSAGE_REGISTRY.keys():
        if frame_id in CANOPEN_RESERVED_IDS:
            errors.append(
                f"Frame ID 0x{frame_id:03X} is a CANopen reserved identifier"
            )

    # Check frame ID is within valid CAN range
    for frame_id in MESSAGE_REGISTRY.keys():
        if frame_id < 0 or frame_id > 0x7FF:
            errors.append(f"Frame ID 0x{frame_id:X} out of valid CAN range (0x000-0x7FF)")

    # Check IDs are within their designated ranges
    all_ids = set(MESSAGE_REGISTRY.keys())
    for start, end, name in ID_RANGES:
        range_ids = [i for i in all_ids if start <= i <= end]
        for fid in range_ids:
            all_ids.discard(fid)

    # Any IDs not in a defined range?
    for fid in all_ids:
        # Find what range it should be in
        found_range = False
        for start, end, name in ID_RANGES:
            if start <= fid <= end:
                found_range = True
                break
        if not found_range:
            errors.append(f"Frame ID 0x{fid:02X} is not within any defined range")

    # A dropped-in vendor file could silently shadow one of our frames.
    external_ids: Dict[int, Tuple[str, str]] = {}
    for src, msg in load_external_messages():
        if msg.frame_id in external_ids:
            other_src, other_name = external_ids[msg.frame_id]
            errors.append(
                f"Frame ID 0x{msg.frame_id:X} defined by both "
                f"external_dbc/{other_src} ({other_name}) and "
                f"external_dbc/{src} ({msg.name})"
            )
        else:
            external_ids[msg.frame_id] = (src, msg.name)

        if msg.frame_id in MESSAGE_REGISTRY:
            errors.append(
                f"Frame ID 0x{msg.frame_id:X} is defined by both "
                f"external_dbc/{src} ({msg.name}) and MESSAGE_REGISTRY. "
                f"remove the MESSAGE_REGISTRY entry, the external DBC wins"
            )

    if errors:
        for err in errors:
            print(f"[ERROR] {err}", file=sys.stderr)
        return False
    return True


def list_messages() -> None:
    """Print all registered messages with their IDs."""
    print(f"{'ID':<8} {'Message Name':<45} {'Description'}")
    print("-" * 95)
    for frame_id in sorted(MESSAGE_REGISTRY.keys()):
        func, desc = MESSAGE_REGISTRY[frame_id]
        msg = func(frame_id)
        print(f"0x{frame_id:02X}     {msg.name:<45} {desc}")
    print(f"\nTotal: {len(MESSAGE_REGISTRY)} messages")


def show_id_map() -> None:
    """Show frame ID allocation map with usage statistics."""
    print("Frame ID Allocation Map")
    print("=" * 60)

    all_ids = sorted(MESSAGE_REGISTRY.keys())
    total_used = 0
    total_available = 0

    for start, end, name in ID_RANGES:
        used = [i for i in all_ids if start <= i <= end]
        capacity = end - start + 1
        available = capacity - len(used)
        total_used += len(used)
        total_available += available

        print(f"\n{name} (0x{start:02X}-0x{end:02X}): {len(used)}/{capacity} used")
        if used:
            for fid in used:
                func, desc = MESSAGE_REGISTRY[fid]
                msg = func(fid)
                print(f"  0x{fid:02X}: {msg.name}")
        else:
            print("  (none)")

    print(f"\n{'=' * 60}")
    print(f"Total: {total_used} messages, {total_available} IDs available")


def _bus_specifics():
    from cantools.database.can.attribute import Attribute
    from cantools.database.can.attribute_definition import AttributeDefinition
    from cantools.database.can.formats.dbc_specifics import DbcSpecifics

    definition = AttributeDefinition("BusType", default_value="CAN",
                                     kind=None, type_name="STRING")
    value = "CAN FD" if FD_BUS else "CAN"
    return DbcSpecifics(attributes={"BusType": Attribute(value, definition)},
                        attribute_definitions={"BusType": definition})


def generate_dbc() -> None:
    """Generate the DBC file from the message registry merged with external_dbc/inverter.dbc."""
    messages = []
    for frame_id in sorted(MESSAGE_REGISTRY.keys()):
        func, _ = MESSAGE_REGISTRY[frame_id]
        messages.append(func(frame_id))

    external = load_external_messages()
    messages.extend(m for _, m in external)

    messages.sort(key=lambda m: m.frame_id)
    for m in messages:
        m.is_fd = FD_BUS

    if not validate_bus_load(messages):
        sys.exit(1)

    nodes = [cantools.db.Node(name=n) for n in NODES]
    db = cantools.db.Database(messages=messages, nodes=nodes,
                              dbc_specifics=_bus_specifics())
    cantools.db.dump_file(db, "gen/FEB_CAN.dbc")

    counts = external_message_counts()
    detail = ", ".join(f"{n} from external_dbc/{f}" for f, n in sorted(counts.items()))
    print(f"Generated gen/FEB_CAN.dbc with {len(messages)} messages"
          + (f" ({detail})" if detail else ""))


def _cantools_name(name: str) -> str:
    """Replicate cantools generate_c_source identifier naming.

    cantools splits the DBC name on '_', applies camelCase→snake_case conversion
    to each component, rejoins with '_', then lowercases the result.
    Simple .lower() is wrong for names like 'M188_U2C_Message_Rxd' where the
    'U2C' component must become 'u2_c' (digit→letter boundary gets an underscore).
    """
    def _cvt(s: str) -> str:
        s = re.sub(r'(.)([A-Z][a-z]+)', r'\1_\2', s)
        s = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', s)
        return s
    return '_'.join(_cvt(p) for p in name.split('_')).lower()


def _msg_c_name(msg) -> str:
    """Return the C identifier used by cantools generate_c_source for this message."""
    return _cantools_name(msg.name)


def _msg_macro_name(msg) -> str:
    """Return the FRAME_ID macro stem used by cantools."""
    return _cantools_name(msg.name).upper()


def _signal_c_name(sig) -> str:
    """Return the C struct field name used by cantools for a signal."""
    return _cantools_name(sig.name)


DB_BASENAME = "feb_can_db"
DB_GUARD = "FEB_CAN_DB_H"


def _split_cantools_header(text: str) -> str:
    """Header body between the include guard's extern "C" block and its close."""
    lines = text.split("\n")
    start = next(i for i, l in enumerate(lines) if l.startswith("#include <stdint.h>"))
    end = len(lines) - 1 - next(i for i, l in enumerate(reversed(lines))
                                if l.startswith("#ifdef __cplusplus"))
    return "\n".join(lines[start:end]).rstrip()


def _split_cantools_source(text: str) -> str:
    """Return the cantools source body, minus its banner and its self-include."""
    lines = text.split("\n")
    start = next(i for i, l in enumerate(lines) if l.startswith("#include"))
    body = [l for l in lines[start:] if not l.startswith('#include "feb_can.h"')]
    return "\n".join(body).rstrip()


def _cantools_license(text: str) -> str:
    """MIT banner verbatim, minus the timestamp line that breaks --check."""
    return text.split("*/", 1)[0] + "*/"


def _state_header_body(messages) -> str:
    L = []
    L.append("/* Latest decoded payload per message, populated by FEB_CAN_DB_Update(). */")
    L.append("")
    L.append("typedef struct {")
    L.append("    uint32_t rx_count;")
    L.append("    uint32_t last_rx_ms;")
    L.append("    bool     present;")
    L.append("} FEB_CAN_DB_Meta_t;")
    L.append("")

    for _, msg in messages:
        c = _msg_c_name(msg)
        L.append(f"typedef struct {{ FEB_CAN_DB_Meta_t meta; struct feb_can_{c}_t data; }} FEB_CAN_DB_{c}_t;")
    L.append("")

    L.append("typedef struct {")
    for _, msg in messages:
        c = _msg_c_name(msg)
        L.append(f"    FEB_CAN_DB_{c}_t {c};")
    L.append("} FEB_CAN_DB_t;")
    L.append("")
    L.append("extern FEB_CAN_DB_t feb_can_db;")
    L.append("")
    L.append("/* Dispatch on frame_id, call the matching generated unpack, update meta.")
    L.append(" * Returns 0 on success, -1 if frame_id is unknown, -2 if unpack fails. */")
    L.append("int FEB_CAN_DB_Update(uint32_t frame_id, const uint8_t *data, uint8_t dlc, uint32_t now_ms);")
    L.append("")
    L.append("/* Print a one-line summary for each *present* message. */")
    L.append("void FEB_CAN_DB_Print(int (*printf_fn)(const char *fmt, ...));")
    L.append("")
    L.append("/* Print one message's full signal breakdown. Returns 0 on match, -1 if name unknown. */")
    L.append("int FEB_CAN_DB_PrintOne(const char *name, int (*printf_fn)(const char *fmt, ...));")
    return "\n".join(L)


def _state_source_body(messages) -> str:
    L = []
    L.append("FEB_CAN_DB_t feb_can_db;")
    L.append("")

    L.append("int FEB_CAN_DB_Update(uint32_t frame_id, const uint8_t *data, uint8_t dlc, uint32_t now_ms)")
    L.append("{")
    L.append("    switch (frame_id)")
    L.append("    {")
    for _, msg in messages:
        c = _msg_c_name(msg)
        MAC = _msg_macro_name(msg)
        L.append(f"    case FEB_CAN_{MAC}_FRAME_ID:")
        L.append(f"        if (feb_can_{c}_unpack(&feb_can_db.{c}.data, data, dlc) < 0) return -2;")
        L.append(f"        feb_can_db.{c}.meta.present = true;")
        L.append(f"        feb_can_db.{c}.meta.last_rx_ms = now_ms;")
        L.append(f"        feb_can_db.{c}.meta.rx_count++;")
        L.append("        return 0;")
    L.append("    default:")
    L.append("        return -1;")
    L.append("    }")
    L.append("}")
    L.append("")

    L.append("void FEB_CAN_DB_Print(int (*printf_fn)(const char *fmt, ...))")
    L.append("{")
    L.append("    printf_fn(\"CAN state (present messages only):\\r\\n\");")
    L.append("    printf_fn(\"  ID    name                                          last_rx_ms      rx_count\\r\\n\");")
    for frame_id, msg in messages:
        c = _msg_c_name(msg)
        L.append(f"    if (feb_can_db.{c}.meta.present) printf_fn(\"  0x%02X  %-45s %10lu      %8lu\\r\\n\", "
                 f"(unsigned)0x{frame_id:02X}, \"{c}\", "
                 f"(unsigned long)feb_can_db.{c}.meta.last_rx_ms, "
                 f"(unsigned long)feb_can_db.{c}.meta.rx_count);")
    L.append("}")
    L.append("")

    L.append("int FEB_CAN_DB_PrintOne(const char *name, int (*printf_fn)(const char *fmt, ...))")
    L.append("{")
    for frame_id, msg in messages:
        c = _msg_c_name(msg)
        L.append(f"    if (strcmp(name, \"{c}\") == 0)")
        L.append("    {")
        L.append(f"        printf_fn(\"0x%02X  {c}  present=%d  last_rx_ms=%lu  rx_count=%lu\\r\\n\","
                 f" (unsigned)0x{frame_id:02X},"
                 f" (int)feb_can_db.{c}.meta.present,"
                 f" (unsigned long)feb_can_db.{c}.meta.last_rx_ms,"
                 f" (unsigned long)feb_can_db.{c}.meta.rx_count);")
        for sig in msg.signals:
            s = _signal_c_name(sig)
            # Cast to signed long for uniform printing; signal widths are <=32 bits.
            L.append(f"        printf_fn(\"  {s:<32s} = %ld\\r\\n\", (long)feb_can_db.{c}.data.{s});")
        L.append("        return 0;")
        L.append("    }")
    L.append("    return -1;")
    L.append("}")
    return "\n".join(L)


TRAITS_BASENAME = "feb_can_traits"


def _cpp_name(name: str) -> str:
    return "".join(part[:1].upper() + part[1:] for part in _cantools_name(name).split("_"))


def _node_enum(node: str) -> str:
    return "k" + "".join(p[:1].upper() + p[1:].lower() for p in node.split("_"))


def generate_traits_file(messages) -> None:
    """Compile-time metadata per message: the input to Publisher<M>/Subscriber<M>."""
    L = ["/* Auto-generated. Do Not Edit.",
         " * Regenerate via: cd common/FEB_CAN_Definitions && ./generate_can.sh",
         " */",
         "",
         "#ifndef FEB_CAN_TRAITS_HPP",
         "#define FEB_CAN_TRAITS_HPP",
         "",
         '#include "feb_can_db.h"',
         "",
         "#include <cstdint>",
         "",
         "namespace feb::can",
         "{",
         "",
         f"inline constexpr bool kFdBus = {'true' if FD_BUS else 'false'};",
         "",
         "enum class Node : std::uint8_t",
         "{",
         "  kNone = 0,"]
    for n in NODES:
        L.append(f"  {_node_enum(n)},")
    L += ["};", "", "namespace msg", "{", ""]

    for frame_id, msg in messages:
        c = _msg_c_name(msg)
        senders = [n for n in (msg.senders or []) if n in NODES]
        # kNone where ownership is shared or external: the TX static_assert
        # cannot name a single owner, so it does not fire.
        sender = _node_enum(senders[0]) if len(senders) == 1 else "kNone"
        L += [f"struct {_cpp_name(msg.name)}",
              "{",
              f"  using Data = feb_can_{c}_t;",
              f"  static constexpr std::uint32_t kFrameId = 0x{frame_id:X}u;",
              f"  static constexpr std::uint8_t kLength = {msg.length}u;",
              f"  static constexpr bool kExtended = {'true' if msg.is_extended_frame else 'false'};",
              f"  static constexpr std::uint32_t kCycleMs = {msg.cycle_time or 0}u;",
              f"  static constexpr Node kSender = Node::{sender};",
              f"  static constexpr auto kPack = &feb_can_{c}_pack;",
              f"  static constexpr auto kUnpack = &feb_can_{c}_unpack;",
              "};",
              ""]

    L += ["}  // namespace msg", "", "}  // namespace feb::can", "",
          "#endif  /* FEB_CAN_TRAITS_HPP */", ""]

    with open(f"gen/{TRAITS_BASENAME}.hpp", "w") as f:
        f.write("\n".join(L))
    print(f"Generated gen/{TRAITS_BASENAME}.hpp ({len(messages)} messages)")


@dataclass(frozen=True)
class ExportedEnum:
    message_name: str
    signal_name: str
    type_name: str
    strip_prefix: str


EXPORTED_ENUMS = [
    ExportedEnum(message_name="bms_state", signal_name="bms_state", type_name="BMS_State", strip_prefix="BMS_STATE_"),
]


def exported_enum_choices(messages, exported_enum: ExportedEnum) -> List[Tuple[int, str]]:
    for _, message in messages:
        if message.name != exported_enum.message_name:
            continue
        for signal in message.signals:
            if signal.name == exported_enum.signal_name and signal.choices:
                choices = sorted((int(value), str(choice_name)) for value, choice_name in signal.choices.items())
                for _, choice_name in choices:
                    if not choice_name.startswith(exported_enum.strip_prefix) or not choice_name.isidentifier():
                        sys.exit(f"[ERROR] {exported_enum.message_name}.{exported_enum.signal_name}: choice "
                                 f"'{choice_name}' must be a C identifier starting with '{exported_enum.strip_prefix}'")
                return choices
    sys.exit(f"[ERROR] EXPORTED_ENUMS: no value table on {exported_enum.message_name}.{exported_enum.signal_name}")


def exported_enums_header_body(messages) -> str:
    lines = []
    for exported_enum in EXPORTED_ENUMS:
        type_name = exported_enum.type_name
        lines.append("typedef enum {")
        for value, choice_name in exported_enum_choices(messages, exported_enum):
            lines.append(f"    {choice_name} = {value},")
        lines.append(f"}} {type_name}_t;")
        lines.append("")
        lines.append(f"const char *{type_name}_Name({type_name}_t value);")
        lines.append("")
    return "\n".join(lines).rstrip()


def exported_enums_source_body(messages) -> str:
    lines = []
    for exported_enum in EXPORTED_ENUMS:
        type_name = exported_enum.type_name
        lines.append(f"const char *{type_name}_Name({type_name}_t value)")
        lines.append("{")
        lines.append("    switch (value)")
        lines.append("    {")
        for _, choice_name in exported_enum_choices(messages, exported_enum):
            display_name = choice_name.removeprefix(exported_enum.strip_prefix)
            lines.append(f'    case {choice_name}: return "{display_name}";')
        lines.append('    default: return "UNKNOWN";')
        lines.append("    }")
        lines.append("}")
        lines.append("")
    return "\n".join(lines).rstrip()


def generate_db_files(cantools_dir: str) -> None:
    """Consolidate the cantools codec and latest-value aggregate into gen/feb_can_db.{h,c}."""
    messages = []
    for frame_id in sorted(MESSAGE_REGISTRY.keys()):
        func, _ = MESSAGE_REGISTRY[frame_id]
        messages.append((frame_id, func(frame_id)))

    for _, ext_msg in load_external_messages():
        messages.append((ext_msg.frame_id, ext_msg))

    ct_h = open(os.path.join(cantools_dir, "feb_can.h")).read()
    ct_c = open(os.path.join(cantools_dir, "feb_can.c")).read()

    banner = (
        "/* Auto-generated. Do Not Edit.\n"
        " * Regenerate via: cd common/FEB_CAN_Definitions && ./generate_can.sh\n"
        " */"
    )

    # ---- header ----
    h = [
        _cantools_license(ct_h),
        "",
        banner,
        "",
        f"#ifndef {DB_GUARD}",
        f"#define {DB_GUARD}",
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
        _split_cantools_header(ct_h),
        "",
        "",
        _state_header_body(messages),
        "",
        "",
        exported_enums_header_body(messages),
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
        "",
        f"#endif /* {DB_GUARD} */",
        "",
    ]
    with open(f"gen/{DB_BASENAME}.h", "w") as f:
        f.write("\n".join(h))

    # ---- source ----
    c = [
        _cantools_license(ct_c),
        "",
        banner,
        "",
        f'#include "{DB_BASENAME}.h"',
        "",
        _split_cantools_source(ct_c),
        "",
        "",
        _state_source_body(messages),
        "",
        "",
        exported_enums_source_body(messages),
        "",
    ]
    with open(f"gen/{DB_BASENAME}.c", "w") as f:
        f.write("\n".join(c))

    print(f"Generated gen/{DB_BASENAME}.h / gen/{DB_BASENAME}.c ({len(messages)} messages)")

    generate_traits_file(messages)



def show_help() -> None:
    """Print help message."""
    print("FEB CAN Library Generator")
    print("")
    print("Usage: python generate.py [option]")
    print("")
    print("Options:")
    print("  (none)              Generate DBC file (default)")
    print("  -l, --list          List all messages with their IDs")
    print("  -i, --ids           Show frame ID allocation map")
    print("  -v, --validate      Validate message registry only")
    print("  --emit-db DIR       Consolidate cantools output in DIR into gen/feb_can_db.{h,c}")
    print("  -h, --help          Show this help")
    print("")
    print("Note: Run via ./generate_can.sh for full generation pipeline.")


def main():
    """Main entry point supporting multiple commands."""
    if len(sys.argv) > 1:
        cmd = sys.argv[1]
        if cmd in ("-l", "--list"):
            list_messages()
            return
        elif cmd in ("-i", "--ids"):
            show_id_map()
            return
        elif cmd in ("-v", "--validate"):
            if validate_registry():
                print("Registry validation passed!")
            else:
                sys.exit(1)
            return
        elif cmd in ("-h", "--help"):
            show_help()
            return
        elif cmd == "--emit-db":
            if len(sys.argv) < 3:
                print("--emit-db requires the directory holding cantools output",
                      file=sys.stderr)
                sys.exit(1)
            if not validate_registry():
                sys.exit(1)
            generate_db_files(sys.argv[2])
            return
        else:
            print(f"Unknown option: {cmd}", file=sys.stderr)
            show_help()
            sys.exit(1)

    # Default: generate DBC
    if not validate_registry():
        sys.exit(1)
    generate_dbc()


if __name__ == "__main__":
    main()
