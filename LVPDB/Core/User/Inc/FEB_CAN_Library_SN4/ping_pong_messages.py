import cantools
from cantools.database.conversion import BaseConversion

def get_ping_pong_counter1(frame_id: int):
    counter_signal = cantools.db.Signal(
        name="counter",
        start=0,
        length=32,
        byte_order="little_endian",
        is_signed=True,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="FEB_PING_PONG_counter1",
        length=8,
        signals=[counter_signal],
        comment="PING PONG counter1.",
        strict=True
    )

    return msg

def get_ping_pong_counter2(frame_id: int):
    counter_signal = cantools.db.Signal(
        name="counter",
        start=0,
        length=32,
        byte_order="little_endian",
        is_signed=True,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="FEB_PING_PONG_counter2",
        length=8,
        signals=[counter_signal],
        comment="PING PONG counter2.",
        strict=True
    )

    return msg

def get_ping_pong_counter3(frame_id: int):
    counter_signal = cantools.db.Signal(
        name="counter",
        start=0,
        length=32,
        byte_order="little_endian",
        is_signed=True,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="FEB_PING_PONG_counter3",
        length=8,
        signals=[counter_signal],
        comment="PING PONG counter3.",
        strict=True
    )

    return msg

def get_ping_pong_counter4(frame_id: int):
    counter_signal = cantools.db.Signal(
        name="counter",
        start=0,
        length=32,
        byte_order="little_endian",
        is_signed=True,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="FEB_PING_PONG_counter4",
        length=8,
        signals=[counter_signal],
        comment="PING PONG counter4.",
        strict=True
    )

    return msg
