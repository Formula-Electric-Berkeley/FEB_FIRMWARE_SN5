import cantools
from cantools.database.conversion import BaseConversion

# Isabellenhutte IVT-S current/voltage/temperature sensor.
#
# The IVT-S broadcasts each measurement in its own 6-byte standard frame:
#   - bytes 0-1: message counter / header (big-endian)
#   - bytes 2-5: signed int32 measurement value (big-endian)
#
# Raw measurement units, as transmitted by the IVT-S on the wire:
#   - current      -> mA
#   - voltage 1-3  -> mV
#   - temperature  -> 0.1 degC
#
# The DBC now applies the same unit conversion and FEB sign convention that the
# BMS consumer (BMS/Core/User/Src/FEB_CAN_IVT.c) applies in C, so DBC-based
# decoders get engineering units (A / V / degC) directly:
#   - current      : raw mA  -> A,    scale = -0.001 (negated for FEB reversed direction)
#   - voltage 1-3  : raw mV  -> V,    scale =  0.001
#   - temperature  : raw 0.1 -> degC, scale =  0.1
#
# Note: the generated C *_unpack functions still return the raw int32; the BMS
# applies its conversion on that raw value, so the C side is unaffected by the
# scaling added here. Big-endian start bits follow the repo's Motorola "sawtooth"
# convention (byte N MSB -> start = N*8 + 7), matching msg_defs/pcu_messages.py.


def _ivt_message(frame_id: int, msg_name: str, value_name: str, comment: str,
                 scale: float, unit: str):
    counter = cantools.db.Signal(
        name="counter",
        start=7,
        length=16,
        byte_order="big_endian",
        is_signed=False,
    )

    value = cantools.db.Signal(
        name=value_name,
        start=23,
        length=32,
        byte_order="big_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=scale),
        unit=unit,
    )

    return cantools.db.Message(
        frame_id=frame_id,
        name=msg_name,
        length=6,
        signals=[counter, value],
        comment=comment,
        senders=['IVT'],
        strict=True,
    )


def get_ivt_current(frame_id: int):
    # Raw int32 in mA; negated for FEB reversed-direction convention -> A.
    return _ivt_message(frame_id, "IVTCurrent", "current",
                        "Isabellenhutte IVT-S pack current (A, FEB sign convention).",
                        scale=-0.001, unit="A")


def get_ivt_voltage_1(frame_id: int):
    # Raw int32 in mV -> V.
    return _ivt_message(frame_id, "IVTVoltage1", "voltage1",
                        "Isabellenhutte IVT-S voltage 1 / pack voltage (V).",
                        scale=0.001, unit="V")


def get_ivt_voltage_2(frame_id: int):
    # Raw int32 in mV -> V.
    return _ivt_message(frame_id, "IVTVoltage2", "voltage2",
                        "Isabellenhutte IVT-S voltage 2 (V).",
                        scale=0.001, unit="V")


def get_ivt_voltage_3(frame_id: int):
    # Raw int32 in mV -> V.
    return _ivt_message(frame_id, "IVTVoltage3", "voltage3",
                        "Isabellenhutte IVT-S voltage 3 (V).",
                        scale=0.001, unit="V")


def get_ivt_temperature(frame_id: int):
    # Raw int32 in 0.1 degC -> degC.
    return _ivt_message(frame_id, "IVTTemperature", "temperature",
                        "Isabellenhutte IVT-S temperature (degC).",
                        scale=0.1, unit="degC")
