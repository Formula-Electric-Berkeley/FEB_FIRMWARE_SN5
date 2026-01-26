
import cantools
from cantools.database.conversion import BaseConversion

def get_dash_buttons(frame_id: int):
    buzzer_state = cantools.db.Signal(
        name="buzzer_state",
        start=0,
        length=1,
        byte_order="little_endian",
        is_signed=False,
    )
    button1 = cantools.db.Signal(
        name="b1_ready_to_drive",
        start=1,
        length=1,
        byte_order="little_endian",
        is_signed=False,
    )
    button2 = cantools.db.Signal(
        name="b2_data_logging",
        start=2,
        length=1,
        byte_order="little_endian",
        is_signed=False,
    )
    button3 = cantools.db.Signal(
        name="b3",
        start=3,
        length=1,
        byte_order="little_endian",
        is_signed=False,
    )
    button4 = cantools.db.Signal(
        name="b4",
        start=4,
        length=1,
        byte_order="little_endian",
        is_signed=False,
    )
    switch1 = cantools.db.Signal(
        name="s1_coolant_pump",
        start=5,
        length=1,
        byte_order="little_endian",
        is_signed=False,
    )
    switch2 = cantools.db.Signal(
        name="s2_radiator_fan",
        start=6,
        length=1,
        byte_order="little_endian",
        is_signed=False,
    )
    switch3 = cantools.db.Signal(
        name="s3_accumulator_fan",
        start=7,
        length=1,
        byte_order="little_endian",
        is_signed=False,
    )
    

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="dash_io",
        length=1,
        signals=[buzzer_state, button1, button2, button3, button4, switch1, switch2, switch3],
        comment="Dash button and switch message",
        strict=True
    )

    return msg

def get_tps_voltage_current(frame_id: int):    
    dash_voltage_signal = cantools.db.Signal(
        name="voltage",
        start=0,
        length=16,
        byte_order="little_endian",
    )
    
    dash_current_signal = cantools.db.Signal(
        name="current",
        start=16,
        length=16,
        byte_order="little_endian",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="dash_tps",
        length=4,
        signals=[dash_voltage_signal, dash_current_signal],
        comment="Dash TPS Chip",
        strict=True
    )

    return msg

def get_dash_heartbeat(frame_id: int):
    error0 = cantools.db.Signal(name="error0", start=0, length=1, byte_order="little_endian", is_signed=False)
    error1 = cantools.db.Signal(name="error1", start=1, length=1, byte_order="little_endian", is_signed=False)
    error2 = cantools.db.Signal(name="error2", start=2, length=1, byte_order="little_endian", is_signed=False)
    error3 = cantools.db.Signal(name="error3", start=3, length=1, byte_order="little_endian", is_signed=False)
    error4 = cantools.db.Signal(name="error4", start=4, length=1, byte_order="little_endian", is_signed=False)
    error5 = cantools.db.Signal(name="error5", start=5, length=1, byte_order="little_endian", is_signed=False)
    error6 = cantools.db.Signal(name="error6", start=6, length=1, byte_order="little_endian", is_signed=False)
    error7 = cantools.db.Signal(name="error7", start=7, length=1, byte_order="little_endian", is_signed=False)
    error8 = cantools.db.Signal(name="error8", start=8, length=1, byte_order="little_endian", is_signed=False)
    error9 = cantools.db.Signal(name="error9", start=9, length=1, byte_order="little_endian", is_signed=False)
    error10 = cantools.db.Signal(name="error10", start=10, length=1, byte_order="little_endian", is_signed=False)
    error11 = cantools.db.Signal(name="error11", start=11, length=1, byte_order="little_endian", is_signed=False)
    error12 = cantools.db.Signal(name="error12", start=12, length=1, byte_order="little_endian", is_signed=False)
    error13 = cantools.db.Signal(name="error13", start=13, length=1, byte_order="little_endian", is_signed=False)
    error14 = cantools.db.Signal(name="error14", start=14, length=1, byte_order="little_endian", is_signed=False)
    error15 = cantools.db.Signal(name="error15", start=15, length=1, byte_order="little_endian", is_signed=False)
    error16 = cantools.db.Signal(name="error16", start=16, length=1, byte_order="little_endian", is_signed=False)
    error17 = cantools.db.Signal(name="error17", start=17, length=1, byte_order="little_endian", is_signed=False)
    error18 = cantools.db.Signal(name="error18", start=18, length=1, byte_order="little_endian", is_signed=False)
    error19 = cantools.db.Signal(name="error19", start=19, length=1, byte_order="little_endian", is_signed=False)
    error20 = cantools.db.Signal(name="error20", start=20, length=1, byte_order="little_endian", is_signed=False)
    error21 = cantools.db.Signal(name="error21", start=21, length=1, byte_order="little_endian", is_signed=False)
    error22 = cantools.db.Signal(name="error22", start=22, length=1, byte_order="little_endian", is_signed=False)
    error23 = cantools.db.Signal(name="error23", start=23, length=1, byte_order="little_endian", is_signed=False)
    error24 = cantools.db.Signal(name="error24", start=24, length=1, byte_order="little_endian", is_signed=False)
    error25 = cantools.db.Signal(name="error25", start=25, length=1, byte_order="little_endian", is_signed=False)
    error26 = cantools.db.Signal(name="error26", start=26, length=1, byte_order="little_endian", is_signed=False)
    error27 = cantools.db.Signal(name="error27", start=27, length=1, byte_order="little_endian", is_signed=False)
    error28 = cantools.db.Signal(name="error28", start=28, length=1, byte_order="little_endian", is_signed=False)
    error29 = cantools.db.Signal(name="error29", start=29, length=1, byte_order="little_endian", is_signed=False)
    error30 = cantools.db.Signal(name="error30", start=30, length=1, byte_order="little_endian", is_signed=False)
    error31 = cantools.db.Signal(name="error31", start=31, length=1, byte_order="little_endian", is_signed=False)
    error32 = cantools.db.Signal(name="error32", start=32, length=1, byte_order="little_endian", is_signed=False)
    error33 = cantools.db.Signal(name="error33", start=33, length=1, byte_order="little_endian", is_signed=False)
    error34 = cantools.db.Signal(name="error34", start=34, length=1, byte_order="little_endian", is_signed=False)
    error35 = cantools.db.Signal(name="error35", start=35, length=1, byte_order="little_endian", is_signed=False)
    error36 = cantools.db.Signal(name="error36", start=36, length=1, byte_order="little_endian", is_signed=False)
    error37 = cantools.db.Signal(name="error37", start=37, length=1, byte_order="little_endian", is_signed=False)
    error38 = cantools.db.Signal(name="error38", start=38, length=1, byte_order="little_endian", is_signed=False)
    error39 = cantools.db.Signal(name="error39", start=39, length=1, byte_order="little_endian", is_signed=False)
    error40 = cantools.db.Signal(name="error40", start=40, length=1, byte_order="little_endian", is_signed=False)
    error41 = cantools.db.Signal(name="error41", start=41, length=1, byte_order="little_endian", is_signed=False)
    error42 = cantools.db.Signal(name="error42", start=42, length=1, byte_order="little_endian", is_signed=False)
    error43 = cantools.db.Signal(name="error43", start=43, length=1, byte_order="little_endian", is_signed=False)
    error44 = cantools.db.Signal(name="error44", start=44, length=1, byte_order="little_endian", is_signed=False)
    error45 = cantools.db.Signal(name="error45", start=45, length=1, byte_order="little_endian", is_signed=False)
    error46 = cantools.db.Signal(name="error46", start=46, length=1, byte_order="little_endian", is_signed=False)
    error47 = cantools.db.Signal(name="error47", start=47, length=1, byte_order="little_endian", is_signed=False)
    error48 = cantools.db.Signal(name="error48", start=48, length=1, byte_order="little_endian", is_signed=False)
    error49 = cantools.db.Signal(name="error49", start=49, length=1, byte_order="little_endian", is_signed=False)
    error50 = cantools.db.Signal(name="error50", start=50, length=1, byte_order="little_endian", is_signed=False)
    error51 = cantools.db.Signal(name="error51", start=51, length=1, byte_order="little_endian", is_signed=False)
    error52 = cantools.db.Signal(name="error52", start=52, length=1, byte_order="little_endian", is_signed=False)
    error53 = cantools.db.Signal(name="error53", start=53, length=1, byte_order="little_endian", is_signed=False)
    error54 = cantools.db.Signal(name="error54", start=54, length=1, byte_order="little_endian", is_signed=False)
    error55 = cantools.db.Signal(name="error55", start=55, length=1, byte_order="little_endian", is_signed=False)
    error56 = cantools.db.Signal(name="error56", start=56, length=1, byte_order="little_endian", is_signed=False)
    error57 = cantools.db.Signal(name="error57", start=57, length=1, byte_order="little_endian", is_signed=False)
    error58 = cantools.db.Signal(name="error58", start=58, length=1, byte_order="little_endian", is_signed=False)
    error59 = cantools.db.Signal(name="error59", start=59, length=1, byte_order="little_endian", is_signed=False)
    error60 = cantools.db.Signal(name="error60", start=60, length=1, byte_order="little_endian", is_signed=False)
    error61 = cantools.db.Signal(name="error61", start=61, length=1, byte_order="little_endian", is_signed=False)
    error62 = cantools.db.Signal(name="error62", start=62, length=1, byte_order="little_endian", is_signed=False)
    error63 = cantools.db.Signal(name="error63", start=63, length=1, byte_order="little_endian", is_signed=False)

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="dash_heartbeat",
        length=8,
        signals=[
            error0, error1, error2, error3, error4, error5, error6, error7,
            error8, error9, error10, error11, error12, error13, error14, error15,
            error16, error17, error18, error19, error20, error21, error22, error23,
            error24, error25, error26, error27, error28, error29, error30, error31,
            error32, error33, error34, error35, error36, error37, error38, error39,
            error40, error41, error42, error43, error44, error45, error46, error47,
            error48, error49, error50, error51, error52, error53, error54, error55,
            error56, error57, error58, error59, error60, error61, error62, error63
        ],
        comment="Dash Heartbeat",
        strict=True
    )

    return msg