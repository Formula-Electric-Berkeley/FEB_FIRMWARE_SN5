from collections import OrderedDict
import cantools
from cantools.database.conversion import BaseConversion

def get_bms_state(frame_id: int):
    bms_state = cantools.db.Signal(
        name="bms_state",
        start=0,
        length=8,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(
            choices=OrderedDict([
                (0, "BMS_STATE_BOOT"),
                (1, "BMS_STATE_LV_POWER"),
                (2, "BMS_STATE_BUS_HEALTH_CHECK"),
                (3, "BMS_STATE_PRECHARGE"),
                (4, "BMS_STATE_ENERGIZED"),
                (5, "BMS_STATE_DRIVE"),
                (6, "BMS_STATE_BATTERY_FREE"),
                (7, "BMS_STATE_CHARGER_PRECHARGE"),
                (8, "BMS_STATE_CHARGING"),
                (9, "BMS_STATE_BALANCE"),
                (20, "BMS_STATE_FAULT_CELL_OVERVOLTAGE"),
                (21, "BMS_STATE_FAULT_CELL_UNDERVOLTAGE"),
                (22, "BMS_STATE_FAULT_CELL_OVERTEMP"),
                (23, "BMS_STATE_FAULT_CELL_UNDERTEMP"),
                (24, "BMS_STATE_FAULT_TEMP_SENSOR_LOSS"),
                (25, "BMS_STATE_FAULT_ADBMS_INIT"),
                (26, "BMS_STATE_FAULT_ADBMS_TIMEOUT"),
                (27, "BMS_STATE_FAULT_IVT_TIMEOUT"),
                (28, "BMS_STATE_FAULT_OVERCURRENT"),
                (29, "BMS_STATE_FAULT_IMD"),
                (30, "BMS_STATE_FAULT_BSPD"),
                (31, "BMS_STATE_FAULT_CONTACTOR_MISMATCH"),
                (32, "BMS_STATE_FAULT_BALANCE_HV_ACTIVE"),
                (33, "BMS_STATE_FAULT_PRECHARGE_TIMEOUT"),
                (34, "BMS_STATE_FAULT_PRECHARGE_TOO_FAST"),
                (35, "BMS_STATE_FAULT_CHARGER_PRECHARGE_TIMEOUT"),
                (36, "BMS_STATE_FAULT_SHUTDOWN_OPEN"),
                (37, "BMS_STATE_FAULT_AIR_MINUS_OPEN"),
                (38, "BMS_STATE_FAULT_CHARGER_HW"),
                (39, "BMS_STATE_FAULT_MANUAL"),
                (40, "BMS_STATE_FAULT_IVT_VOLTAGE_NEGATIVE"),
            ])
        )
    )

    total_pack_voltage = cantools.db.Signal(
        name="total_pack_voltage",
        start=8,
        length=22,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=0.00015),
        unit="V",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="bms_state",
        length=4,
        signals=[bms_state, total_pack_voltage],
        senders=['BMS'],
        cycle_time=100,
        strict=True
    )

    return msg

def get_accumulator_voltage(frame_id: int):
    signals = []
    for i, name in enumerate(["average_cell_voltage", "min_cell_voltage", "max_cell_voltage"]):
        signals.append(cantools.db.Signal(
            name=name,
            start=i * 16,
            length=16,
            byte_order="little_endian",
            is_signed=False,
            conversion=BaseConversion.factory(scale=0.00015),
            unit="V",
        ))

    for i, name in enumerate(["min_voltage_module", "min_voltage_cell", "max_voltage_module", "max_voltage_cell"]):
        signals.append(cantools.db.Signal(
            name=name,
            start=48 + i * 4,
            length=4,
            byte_order="little_endian",
            is_signed=False,
        ))

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="bms_accumulator_voltage",
        length=8,
        signals=signals,
        comment="BMS message for accumulator voltage.",
        senders=['BMS'],
        cycle_time=100,
        strict=True
    )

    return msg

def get_accumulator_temperature(frame_id: int):
    signals = []
    for i, name in enumerate(["average_pack_temperature", "min_cell_temperature", "max_cell_temperature"]):
        signals.append(cantools.db.Signal(
            name=name,
            start=i * 13,
            length=13,
            byte_order="little_endian",
            is_signed=True,
            conversion=BaseConversion.factory(scale=0.05),
            unit="degC",
        ))

    for i, name in enumerate(["min_temperature_module", "min_temperature_cell",
                              "max_temperature_module", "max_temperature_cell"]):
        signals.append(cantools.db.Signal(
            name=name,
            start=40 + i * 4,
            length=4,
            byte_order="little_endian",
            is_signed=False,
        ))

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="bms_accumulator_temperature",
        length=7,
        signals=signals,
        comment="BMS message for accumulator temperature.",
        senders=['BMS'],
        cycle_time=100,
        strict=True
    )

    return msg

def get_tps_voltage_current(frame_id: int):    
    bbb_voltage_signal = cantools.db.Signal(
        name="voltage",
        start=0,
        length=16,
        byte_order="little_endian",
    )
    
    bbb_current_signal = cantools.db.Signal(
        name="current",
        start=16,
        length=16,
        byte_order="little_endian",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="bbb_tps",
        length=4,
        signals=[bbb_voltage_signal, bbb_current_signal],
        comment="BBB TPS Chip",
        senders=['BMS'],
        cycle_time=100,
        strict=True
    )

    return msg

def get_cell_data(frame_id: int):
    selector = cantools.db.Signal(
        name="selector",
        start=0,
        length=8,
        byte_order="little_endian",
        is_signed=False,
        is_multiplexer=True,
    )

    signals = [selector]
    for module in range(1, 11):
        for cell in range(1, 15):
            mux_id = module | (cell << 4)

            signals.append(cantools.db.Signal(
                name=f"module_{module}_cell_{cell}_voltage",
                start=8,
                length=16,
                byte_order="little_endian",
                is_signed=False,
                conversion=BaseConversion.factory(scale=0.00015),
                unit="V",
                multiplexer_ids=[mux_id],
                multiplexer_signal="selector",
            ))

            for temp in range(3):
                signals.append(cantools.db.Signal(
                    name=f"module_{module}_cell_{cell}_temp_{temp + 1}",
                    start=24 + temp * 13,
                    length=13,
                    byte_order="little_endian",
                    is_signed=True,
                    conversion=BaseConversion.factory(scale=0.05),
                    unit="degC",
                    multiplexer_ids=[mux_id],
                    multiplexer_signal="selector",
                ))

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="bms_cell_data",
        length=8,
        signals=signals,
        senders=['BMS'],
        cycle_time=10,
        strict=True
    )

    return msg
