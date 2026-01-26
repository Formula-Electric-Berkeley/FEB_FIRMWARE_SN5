import cantools
from cantools.database.conversion import BaseConversion

def get_iv_meter_data(frame_id: int):
    low_range_current = cantools.db.Signal(
        name="low_range_current",
        start=0,
        length=8,
        byte_order="big_endian",
        is_signed=False,
    )
    
    high_range_current = cantools.db.Signal(
        name="high_range_current",
        start=8,
        length=8,
        byte_order="big_endian",
        is_signed=False,
    )

    hv_voltage = cantools.db.Signal(
        name="hv_voltage",
        start=16,
        length=8,
        byte_order="big_endian",
        is_signed=False,
        unit="V",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="IVMeter",
        length=4,
        signals=[low_range_current, high_range_current, hv_voltage],
        comment="ivMeter message for low and high range current and HV voltage.",
        strict=True
    )

    return msg
