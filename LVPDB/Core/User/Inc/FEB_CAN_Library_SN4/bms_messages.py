import cantools
from cantools.database.conversion import BaseConversion

def get_bms_state(frame_id: int):
    bms_state = cantools.db.Signal(
        name="bms_state",
        start=0,
        length=5,
        byte_order="little_endian",
        is_signed=False,
    )

    ping_lv_nodes = cantools.db.Signal(
        name="ping_lv_nodes",
        start=5,
        length=3,
        byte_order="little_endian",
        is_signed=False,
    )

    relay_state = cantools.db.Signal(
        name="relay_state",
        start=8,
        length=3,
        byte_order="little_endian",
        is_signed=False,
    )

    gpio_sense = cantools.db.Signal(
        name="gpio_sense",
        start=11,
        length=5,
        byte_order="little_endian",
        is_signed=False,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="bms_state",
        length=2,
        signals=[bms_state, ping_lv_nodes, relay_state, gpio_sense],
        comment="BMS message for BMS state.",
        strict=True
    )

    return msg

def get_bms_cell_data(frame_id: int):
    bms_flags = cantools.db.Signal(
        name="bms_flags",
        start=0,
        length=4,
        byte_order="little_endian",
        is_signed=False,
    )

    bms_cell = cantools.db.Signal(
        name="bms_cell",
        start=8,
        length=4,
        byte_order="little_endian",
        is_signed=False,
    )

    bms_bank = cantools.db.Signal(
        name="bms_bank",
        start=12,
        length=4,
        byte_order="little_endian",
        is_signed=False,
    )

    bms_voltage = cantools.db.Signal(
        name="bms_voltage",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    bms_temperature = cantools.db.Signal(
        name="bms_temperature",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=True,
    )

    bms_send_time = cantools.db.Signal(
        name="bms_send_time",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="bms_cell_data",
        length=8,
        signals=[bms_flags, bms_cell, bms_bank, bms_voltage, bms_temperature, bms_send_time],
        comment="BMS message for cell data.",
        strict=True
    )

    return msg

def get_accumulator_voltage(frame_id: int):
    total_pack_voltage = cantools.db.Signal(
        name="total_pack_voltage",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    min_cell_voltage = cantools.db.Signal(
        name="min_cell_voltage",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    max_cell_voltage = cantools.db.Signal(
        name="max_cell_voltage",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    send_time = cantools.db.Signal(
        name="send_time",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="bms_accumulator_voltage",
        length=8,
        signals=[total_pack_voltage, min_cell_voltage, max_cell_voltage, send_time],
        comment="BMS message for accumulator voltage.",
        strict=True
    )

    return msg

def get_accumulator_temperature(frame_id: int):
    total_pack_voltage = cantools.db.Signal(
        name="average_pack_temperature",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=True,
    )

    min_cell_temperature = cantools.db.Signal(
        name="min_cell_temperature",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=True,
    )

    max_cell_temperature = cantools.db.Signal(
        name="max_cell_temperature",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=True,
    )

    send_time = cantools.db.Signal(
        name="send_time",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="bms_accumulator_temperature",
        length=8,
        signals=[total_pack_voltage, min_cell_temperature, max_cell_temperature, send_time],
        comment="BMS message for accumulator temperature.",
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
        strict=True
    )

    return msg

def get_accumulator_faults(frame_id: int):    
    bms_fault = cantools.db.Signal(
        name="bms_fault",
        start=0,
        length=1,
        byte_order="little_endian",
    )
    
    imd_fault = cantools.db.Signal(
        name="imd_fault",
        start=1,
        length=1,
        byte_order="little_endian",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="accumulator_faults",
        length=1,
        signals=[bms_fault, imd_fault],
        comment="Accumulator Faults",
        strict=True
    )

    return msg
