import cantools
from cantools.database.conversion import BaseConversion



def get_steering_data(frame_id: int):
    can_counter = cantools.db.Signal(
        name="can_counter",
        start=0,
        length=32,
        byte_order="little_endian",
        is_signed=False,
    )
    flags = cantools.db.Signal(
        name="flags",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    angle = cantools.db.Signal(
        name="angle",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="FEB_Steering_data",
        length=8,
        signals=[can_counter, flags, angle],
        comment="Message for steering data.",
        strict=True
    )

    return msg

def get_rear_left_tire_temp_data(frame_id: int):

    leftmost_temp = cantools.db.Signal(
        name="leftmost_temp_RL",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_left_temp = cantools.db.Signal(
        name="center_left_temp_RL",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_right_temp = cantools.db.Signal(
        name="center_right_temp_RL",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    rightmost_temp = cantools.db.Signal(
        name="rightmost_temp_RL",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="rear_left_tire_temp",
        length=8,
        signals=[leftmost_temp, center_left_temp, center_right_temp, rightmost_temp],
        comment="Message for rear left tire temp data.",
        strict=True
    )

    return msg

def get_rear_right_tire_temp_data(frame_id: int):

    leftmost_temp = cantools.db.Signal(
        name="leftmost_temp_RR",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_left_temp = cantools.db.Signal(
        name="center_left_temp_RR",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_right_temp = cantools.db.Signal(
        name="center_right_temp_RR",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    rightmost_temp = cantools.db.Signal(
        name="rightmost_temp_RR",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="rear_right_tire_temp",
        length=8,
        signals=[leftmost_temp, center_left_temp, center_right_temp, rightmost_temp],
        comment="Message for rear right tire temp data.",
        strict=True
    )

    return msg

def get_front_left_tire_temp_data(frame_id: int):

    leftmost_temp = cantools.db.Signal(
        name="leftmost_temp_FL",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_left_temp = cantools.db.Signal(
        name="center_left_temp_FL",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_right_temp = cantools.db.Signal(
        name="center_right_temp_FL",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    rightmost_temp = cantools.db.Signal(
        name="rightmost_temp_FL",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="front_left_tire_temp",
        length=8,
        signals=[leftmost_temp, center_left_temp, center_right_temp, rightmost_temp],
        comment="Message for front left tire temp data.",
        strict=True
    )

    return msg

def get_front_right_tire_temp_data(frame_id: int):

    leftmost_temp = cantools.db.Signal(
        name="leftmost_temp_FR",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_left_temp = cantools.db.Signal(
        name="center_left_temp_FR",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_right_temp = cantools.db.Signal(
        name="center_right_temp_FR",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    rightmost_temp = cantools.db.Signal(
        name="rightmost_temp_FR",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="front_right_tire_temp",
        length=8,
        signals=[leftmost_temp, center_left_temp, center_right_temp, rightmost_temp],
        comment="Message for front right tire temp data.",
        strict=True
    )

    return msg

def get_imu_data(frame_id: int):
    accelX = cantools.db.Signal(
        name="acceleration_x",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=True,
    )
    accelY = cantools.db.Signal(
        name="acceleration_y",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=True,
    )
    accelZ = cantools.db.Signal(
        name="acceleration_z",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=True,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="FEB_imu_data",
        length=6,
        signals=[accelX, accelY, accelZ],
        comment="Message for imu data.",
        strict=True
    )
    return msg

def get_wss_data_front(frame_id: int):
    wss_right_front = cantools.db.Signal(
        name="wss_right_front",
        start=0,
        length=8,
        byte_order="little_endian",
        is_signed=False,
    )
    wss_left_front = cantools.db.Signal(
        name="wss_left_front",
        start=8,
        length=8,
        byte_order="little_endian",
        is_signed=False,
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="wss_front_data",
        length=2,
        signals=[wss_right_front, wss_left_front],
        comment="Wheel speed sensor data for left and right front wheels.",
        strict=True
    )

    return msg

def get_wss_data_rear(frame_id: int):
    wss_right_rear = cantools.db.Signal(
        name="wss_right_rear",
        start=0,
        length=8,
        byte_order="little_endian",
        is_signed=False,
    )
    wss_left_rear = cantools.db.Signal(
        name="wss_left_rear",
        start=8,
        length=8,
        byte_order="little_endian",
        is_signed=False,
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="wss_rear_data",
        length=2,
        signals=[wss_right_rear, wss_left_rear],
        comment="Wheel speed sensor data for left and right rear wheels.",
        strict=True
    )

    return msg

def get_LinPot_Front(frame_id: int):
    lin_pot_1 = cantools.db.Signal(
        name="linear_potentiometer_1_front",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    lin_pot_2 = cantools.db.Signal(
        name="linear_potentiometer_2_front",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="linear_potentiometer_front",
        length=4,
        signals=[lin_pot_1, lin_pot_2],
        comment="Linear Potentiometer Front.",
        strict=True
    )

    return msg

def get_LinPot_Rear(frame_id: int):
    lin_pot_1 = cantools.db.Signal(
        name="linear_potentiometer_1_rear",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    lin_pot_2 = cantools.db.Signal(
        name="linear_potentiometer_2_rear",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="linear_potentiometer_rear",
        length=4,
        signals=[lin_pot_1, lin_pot_2],
        comment="Linear Potentiometer Rear.",
        strict=True
    )

    return msg

def get_CoolantPressure(frame_id: int):
    coolant_pressure_1 = cantools.db.Signal(
        name="coolant_pressure_1",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    coolant_pressure_2 = cantools.db.Signal(
        name="coolant_pressure_2",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="coolant_pressure",
        length=4,
        signals=[coolant_pressure_1, coolant_pressure_2],
        comment="Coolant Pressure",
        strict=True
    )
    return msg

def get_front_sensor_heartbeat(frame_id: int):
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
        name="front_sensor_heartbeat_message",
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
        comment="Front Sensor Heartbeat",
        strict=True
    )

    return msg

def get_rear_sensor_heartbeat(frame_id: int):
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
        name="rear_sensor_heartbeat_message",
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
        comment="Rear Sensor Heartbeat",
        strict=True
    )

    return msg