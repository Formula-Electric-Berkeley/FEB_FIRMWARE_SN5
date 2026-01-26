import cantools
from cantools.database.conversion import BaseConversion

def get_measured_fan_speeds_1234(frame_id: int):
    fan1_speed_signal = cantools.db.Signal(
        name="fan1_speed",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    fan2_speed_signal = cantools.db.Signal(
        name="fan2_speed",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    fan3_speed_signal = cantools.db.Signal(
        name="fan3_speed",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    fan4_speed_signal = cantools.db.Signal(
        name="fan4_speed",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="DART_TACH_measurements_1234",
        length=8,
        signals=[fan1_speed_signal,fan2_speed_signal,fan3_speed_signal,fan4_speed_signal],
        comment="DART message for Tachometer measurements first 4 fan speeds.",
        strict=True
    )

    return msg 

def get_measured_fan_speeds_5(frame_id: int):

    fan5_speed_signal = cantools.db.Signal(
        name="fan5_speed",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="DART_TACH_measurements_5",
        length=2,
        signals=[fan5_speed_signal],
        comment="DART message for Tachometer measurements 5th fan speed.",
        strict=True
    )

    return msg 