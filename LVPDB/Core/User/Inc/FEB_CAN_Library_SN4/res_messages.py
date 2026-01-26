import cantools
from cantools.database.conversion import BaseConversion

def get_res_state(frame_id: int):
    res_state = cantools.db.Signal(
        name="res_state",
        start=0,
        length=8,
        byte_order="little_endian",
        is_signed=False,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="res_state",
        length=2,
        signals=[res_state],
        comment="RES message for RES state.",
        strict=True
    )

    return msg