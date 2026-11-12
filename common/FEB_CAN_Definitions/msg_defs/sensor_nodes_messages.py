import cantools
from cantools.database.conversion import BaseConversion


def get_steer(frame_id: int, variant: str):
    angle = cantools.db.Signal(
        name="angle",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=360.0 / 4096.0),
        unit="deg",
        comment="12-bit filtered angle",
    )
    raw_angle = cantools.db.Signal(
        name="raw_angle",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=360.0 / 4096.0),
        unit="deg",
        comment="12-bit raw angle",
    )
    agc = cantools.db.Signal(
        name="agc",
        start=32,
        length=8,
        byte_order="little_endian",
        is_signed=False,
        comment="AGC gain value",
    )
    status = cantools.db.Signal(
        name="status",
        start=40,
        length=8,
        byte_order="little_endian",
        is_signed=False,
        comment="magnet flags (bit0=MH, bit1=ML, bit2=MD)",
    )
    magnitude = cantools.db.Signal(
        name="magnitude",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        comment="12-bit CORDIC magnitude",
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"steer_{variant}",
        length=8,
        signals=[angle, raw_angle, agc, status, magnitude],
        comment="Steering encoder filtered/raw angle, AGC gain, magnet status flags, and CORDIC magnitude.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True,
    )
    return msg


def get_tire_temp_left(frame_id: int, variant: str):
    
    leftmost_temp = cantools.db.Signal(
        name="leftmost_temp",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_left_temp = cantools.db.Signal(
        name="center_left_temp",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_right_temp = cantools.db.Signal(
        name="center_right_temp",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    rightmost_temp = cantools.db.Signal(
        name="rightmost_temp",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"tire_temp_left_{variant}",
        length=8,
        signals=[leftmost_temp, center_left_temp, center_right_temp, rightmost_temp],
        comment="Message for left tire temp data.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=1000,
        strict=True
    )

    return msg

def get_tire_temp_right(frame_id: int, variant: str):
    
    leftmost_temp = cantools.db.Signal(
        name="leftmost_temp",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_left_temp = cantools.db.Signal(
        name="center_left_temp",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    center_right_temp = cantools.db.Signal(
        name="center_right_temp",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    rightmost_temp = cantools.db.Signal(
        name="rightmost_temp",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"tire_temp_right_{variant}",
        length=8,
        signals=[leftmost_temp, center_left_temp, center_right_temp, rightmost_temp],
        comment="Message for right tire temp data.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=1000,
        strict=True
    )

    return msg

def get_imu_accel(frame_id: int, variant: str):
    accelX = cantools.db.Signal(
        name="acceleration_x",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.061),
        unit="mg",
        comment="LSM6DSOX @ FS=2g (matches lsm6dsox_from_fs2_to_mg)",
    )
    accelY = cantools.db.Signal(
        name="acceleration_y",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.061),
        unit="mg",
        comment="LSM6DSOX @ FS=2g (matches lsm6dsox_from_fs2_to_mg)",
    )
    accelZ = cantools.db.Signal(
        name="acceleration_z",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.061),
        unit="mg",
        comment="LSM6DSOX @ FS=2g (matches lsm6dsox_from_fs2_to_mg)",
    )

    imu_temp = cantools.db.Signal(
        name="imu_temp",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.01),
        unit="degC",
        comment="LSM6DSOX die temperature",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"imu_accel_{variant}",
        length=8,
        signals=[accelX, accelY, accelZ, imu_temp],
        comment="IMU acceleration (LSM6DSOX, FS=2g) + die temperature.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )

    return msg


def get_imu_gyro(frame_id: int, variant: str):
    gyroX = cantools.db.Signal(
        name="gyro_x",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=70),
        unit="mdps",
        comment="LSM6DSOX @ FS=2000dps (matches lsm6dsox_from_fs2000_to_mdps)",
    )
    gyroY = cantools.db.Signal(
        name="gyro_y",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=70),
        unit="mdps",
        comment="LSM6DSOX @ FS=2000dps (matches lsm6dsox_from_fs2000_to_mdps)",
    )
    gyroZ = cantools.db.Signal(
        name="gyro_z",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=70),
        unit="mdps",
        comment="LSM6DSOX @ FS=2000dps (matches lsm6dsox_from_fs2000_to_mdps)",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"imu_gyro_{variant}",
        length=6,
        signals=[gyroX, gyroY, gyroZ],
        comment="IMU gyro (LSM6DSOX, FS=2000dps).",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )

    return msg


def get_mag(frame_id: int, variant: str):
    magX = cantools.db.Signal(
        name="magnetometer_x",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.5844),
        unit="mG",
        comment="LIS3MDL @ FS=16gauss (matches lis3mdl_from_fs16_to_gauss * 1000)",
    )
    magY = cantools.db.Signal(
        name="magnetometer_y",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.5844),
        unit="mG",
        comment="LIS3MDL @ FS=16gauss (matches lis3mdl_from_fs16_to_gauss * 1000)",
    )
    magZ = cantools.db.Signal(
        name="magnetometer_z",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.5844),
        unit="mG",
        comment="LIS3MDL @ FS=16gauss (matches lis3mdl_from_fs16_to_gauss * 1000)",
    )

    mag_temp = cantools.db.Signal(
        name="mag_temp",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.01),
        unit="degC",
        comment="LIS3MDL die temperature",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"mag_{variant}",
        length=8,
        signals=[magX, magY, magZ, mag_temp],
        comment="Magnetometer (LIS3MDL, FS=16gauss) + die temperature.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )

    return msg


def get_gps_pos(frame_id: int, variant: str):
    lat = cantools.db.Signal(
        name="latitude",
        start=0,
        length=32,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=1e-7),
        unit="deg",
    )
    lon = cantools.db.Signal(
        name="longitude",
        start=32,
        length=32,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=1e-7),
        unit="deg",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"gps_pos_{variant}",
        length=8,
        signals=[lat, lon],
        comment="GPS latitude/longitude.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )

    return msg


def get_gps_motion(frame_id: int, variant: str):
    speed = cantools.db.Signal(
        name="speed",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=0.01),
        unit="km/h",
    )
    course = cantools.db.Signal(
        name="course",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=0.01),
        unit="deg",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"gps_motion_{variant}",
        length=4,
        signals=[speed, course],
        comment="GPS speed and course-over-ground.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )

    return msg


def get_gps_time(frame_id: int, variant: str):
    hours = cantools.db.Signal(
        name="hours",
        start=0,
        length=8,
        byte_order="little_endian",
        is_signed=True,
    )
    minutes = cantools.db.Signal(
        name="minutes",
        start=8,
        length=8,
        byte_order="little_endian",
        is_signed=True,
    )

    seconds = cantools.db.Signal(
        name="seconds",
        start=16,
        length=8,
        byte_order="little_endian",
        is_signed=True,
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"gps_time_{variant}",
        length=3,
        signals=[hours, minutes, seconds],
        comment="GPS Time data message (UTC).",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=1000,
        strict=True
    )

    return msg


def get_gps_date(frame_id: int, variant: str):
    day = cantools.db.Signal(
        name="day",
        start=0,
        length=8,
        byte_order="little_endian",
        is_signed=True,
    )
    month = cantools.db.Signal(
        name="month",
        start=8,
        length=8,
        byte_order="little_endian",
        is_signed=True,
    )

    year = cantools.db.Signal(
        name="year",
        start=16,
        length=8,
        byte_order="little_endian",
        is_signed=True,
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"gps_date_{variant}",
        length=3,
        signals=[day, month, year],
        comment="GPS Date data message.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=1000,
        strict=True
    )

    return msg


def get_gps_altitude(frame_id: int, variant: str):
    altitude = cantools.db.Signal(
        name="altitude",
        start=0,
        length=32,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.01),
        unit="m",
    )
    hdop = cantools.db.Signal(
        name="hdop",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=0.01),
    )
    vdop = cantools.db.Signal(
        name="vdop",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=0.01),
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"gps_altitude_{variant}",
        length=8,
        signals=[altitude, hdop, vdop],
        comment="GPS altitude + horizontal/vertical DOP.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=1000,
        strict=True
    )
    return msg


def get_gps_status(frame_id: int, variant: str):
    fix_type = cantools.db.Signal(
        name="fix_type",
        start=0, length=8, byte_order="little_endian", is_signed=False,
        comment="0=Invalid, 1=GPS, 2=DGPS, 3=PPS",
    )
    fix_mode = cantools.db.Signal(
        name="fix_mode",
        start=8, length=8, byte_order="little_endian", is_signed=False,
        comment="1=NoFix, 2=2D, 3=3D",
    )
    sats_in_use = cantools.db.Signal(
        name="sats_in_use", start=16, length=8, byte_order="little_endian", is_signed=False,
    )
    sats_in_view = cantools.db.Signal(
        name="sats_in_view", start=24, length=8, byte_order="little_endian", is_signed=False,
    )
    valid = cantools.db.Signal(
        name="valid", start=32, length=8, byte_order="little_endian", is_signed=False,
    )
    has_fix = cantools.db.Signal(
        name="has_fix", start=40, length=8, byte_order="little_endian", is_signed=False,
    )
    pdop = cantools.db.Signal(
        name="pdop", start=48, length=16, byte_order="little_endian", is_signed=False,
        conversion=BaseConversion.factory(scale=0.01),
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"gps_status_{variant}",
        length=8,
        signals=[fix_type, fix_mode, sats_in_use, sats_in_view, valid, has_fix, pdop],
        comment="GPS fix quality, satellite counts, validity flags, position DOP.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=1000,
        strict=True
    )
    return msg


def get_fusion_quat(frame_id: int, variant: str):
    qw = cantools.db.Signal(
        name="q_w", start=0, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=1.0/32767.0),
    )
    qx = cantools.db.Signal(
        name="q_x", start=16, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=1.0/32767.0),
    )
    qy = cantools.db.Signal(
        name="q_y", start=32, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=1.0/32767.0),
    )
    qz = cantools.db.Signal(
        name="q_z", start=48, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=1.0/32767.0),
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"fusion_quat_{variant}",
        length=8,
        signals=[qw, qx, qy, qz],
        comment="Fusion AHRS orientation quaternion (w, x, y, z).",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )
    return msg


def get_fusion_euler(frame_id: int, variant: str):
    roll = cantools.db.Signal(
        name="roll", start=0, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=0.01), unit="deg",
    )
    pitch = cantools.db.Signal(
        name="pitch", start=16, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=0.01), unit="deg",
    )
    yaw = cantools.db.Signal(
        name="yaw", start=32, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=0.01), unit="deg",
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"fusion_euler_{variant}",
        length=6,
        signals=[roll, pitch, yaw],
        comment="Fusion AHRS Euler angles.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )
    return msg


def get_fusion_lin_accel(frame_id: int, variant: str):
    ax = cantools.db.Signal(
        name="lin_accel_x", start=0, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=0.001),
        unit="G",
    )
    ay = cantools.db.Signal(
        name="lin_accel_y", start=16, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=0.001),
        unit="G",
    )
    az = cantools.db.Signal(
        name="lin_accel_z", start=32, length=16, byte_order="little_endian", is_signed=True,
        conversion=BaseConversion.factory(scale=0.001),
        unit="G",
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"fusion_lin_accel_{variant}",
        length=6,
        signals=[ax, ay, az],
        comment="Fusion linear acceleration (gravity removed, body frame).",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=10,
        strict=True
    )
    return msg


def get_fusion_earth_accel(frame_id: int, variant: str):
    ax = cantools.db.Signal(
        name="earth_accel_x", start=0, length=16, byte_order="little_endian", is_signed=True,
        unit="mg",
    )
    ay = cantools.db.Signal(
        name="earth_accel_y", start=16, length=16, byte_order="little_endian", is_signed=True,
        unit="mg",
    )
    az = cantools.db.Signal(
        name="earth_accel_z", start=32, length=16, byte_order="little_endian", is_signed=True,
        unit="mg",
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"fusion_earth_accel_{variant}",
        length=6,
        signals=[ax, ay, az],
        comment="Fusion linear acceleration (gravity removed, earth frame).",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )
    return msg


def get_fusion_status(frame_id: int, variant: str):
    flags = cantools.db.Signal(
        name="flags", start=0, length=8, byte_order="little_endian", is_signed=False,
        comment="bit0 startup, bit1 angRateRecov, bit2 accelRecov, bit3 magRecov, bit4 accelIgnored, bit5 magIgnored",
    )
    accel_error = cantools.db.Signal(
        name="accel_error", start=8, length=8, byte_order="little_endian", is_signed=False,
        conversion=BaseConversion.factory(scale=0.1), unit="deg",
    )
    mag_error = cantools.db.Signal(
        name="mag_error", start=16, length=8, byte_order="little_endian", is_signed=False,
        conversion=BaseConversion.factory(scale=0.1), unit="deg",
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"fusion_status_{variant}",
        length=3,
        signals=[flags, accel_error, mag_error],
        comment="Fusion AHRS internal flags + accel/mag rejection errors.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )
    return msg


def get_wss(frame_id: int, variant: str):
    wss_left_front = cantools.db.Signal(
        name="wss_left",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=0.01),
        unit="mph",
    )
    wss_right_front = cantools.db.Signal(
        name="wss_right",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=0.01),
        unit="mph",
    )
    wss_dir_flags = cantools.db.Signal(
        name="wss_dir_flags",
        start=32,
        length=8,
        byte_order="little_endian",
        is_signed=False,
        comment="bit0: left wheel direction (0=fwd, 1=rev); bit1: right wheel direction",
    )
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"wss_{variant}",
        length=8,
        signals=[wss_left_front, wss_right_front, wss_dir_flags],
        comment="Wheel speeds + direction flags.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=20,
        strict=True
    )

    return msg

def get_strain_gauge(frame_id: int, variant: str):
    strain_gauge_1 = cantools.db.Signal(
        name="strain_gauge_1",
        start=0,
        length=16, 
        byte_order="little_endian",
        is_signed=False,
     )
    
    strain_gauge_2 = cantools.db.Signal(
        name="strain_gauge_2",
        start=16,
        length=16, 
        byte_order="little_endian",
        is_signed=False,
     )
    
    strain_gauge_3 = cantools.db.Signal(
        name="strain_gauge_3",
        start=32,
        length=16, 
        byte_order="little_endian",
        is_signed=False,
     )
    
    strain_gauge_4 = cantools.db.Signal(
        name="strain_gauge_4",
        start=48,
        length=16, 
        byte_order="little_endian",
        is_signed=False,
     )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"strain_gauge_{variant}",
        length=8,
        signals=[strain_gauge_1, strain_gauge_2, strain_gauge_3, strain_gauge_4],
        comment="Strain Gauges",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=20,
        strict=True
     )
 
    return msg

def get_linpot(frame_id: int, variant: str):
    linpot_left = cantools.db.Signal(
        name="linpot_left",
        start=0,
        length=16, 
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=0.01),
        unit="mm",
     )
    
    linpot_right = cantools.db.Signal(
        name="linpot_right",
        start=16,
        length=16, 
        byte_order="little_endian",
        is_signed=False,
        conversion=BaseConversion.factory(scale=0.01),
        unit="mm",
     )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"linpot_{variant}",
        length=4,
        signals=[linpot_left, linpot_right],
        comment="Linear potentiometer positions.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=20,
        strict=True
     )
 
    return msg

def get_thermistor(frame_id: int, variant: str):
    # NTC 10k B3950, 5k pull-up to 3V3. int16 0.01 degC.
    coolant_temp_1 = cantools.db.Signal(
        name="coolant_temp_1",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.01),
        unit="degC",
    )
    coolant_temp_2 = cantools.db.Signal(
        name="coolant_temp_2",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.01),
        unit="degC",
    )
    coolant_temp_3 = cantools.db.Signal(
        name="coolant_temp_3",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=True,
        conversion=BaseConversion.factory(scale=0.01),
        unit="degC",
    )
    therm_status = cantools.db.Signal(
        name="therm_status",
        start=48,
        length=8,
        byte_order="little_endian",
        is_signed=False,
        comment="bit0-2: channel 1-3 open circuit; bit3-5: channel 1-3 short circuit",
    )

    msg = cantools.db.Message(
        frame_id=frame_id,
        name=f"thermistor_{variant}",
        length=7,
        signals=[coolant_temp_1, coolant_temp_2, coolant_temp_3, therm_status],
        comment="Coolant NTC thermistor temperatures + fault flags.",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
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

def get_heartbeat(frame_id: int, variant: str):

    # Byte 0: device init / read faults
    error0 = cantools.db.Signal(name="imu_init_failed", start=0, length=1, byte_order="little_endian", is_signed=False)
    error1 = cantools.db.Signal(name="imu_read_failed", start=1, length=1, byte_order="little_endian", is_signed=False)
    error2 = cantools.db.Signal(name="mag_init_failed", start=2, length=1, byte_order="little_endian", is_signed=False)
    error3 = cantools.db.Signal(name="mag_read_failed", start=3, length=1, byte_order="little_endian", is_signed=False)
    error4 = cantools.db.Signal(name="gps_init_failed", start=4, length=1, byte_order="little_endian", is_signed=False)
    error5 = cantools.db.Signal(name="fusion_uncalibrated", start=5, length=1, byte_order="little_endian", is_signed=False)
    error6 = cantools.db.Signal(name="lp_out_of_range", start=6, length=1, byte_order="little_endian", is_signed=False)
    error7 = cantools.db.Signal(name="error7", start=7, length=1, byte_order="little_endian", is_signed=False)

    # Byte 1: data freshness
    error8 = cantools.db.Signal(name="gps_no_fix", start=8, length=1, byte_order="little_endian", is_signed=False)
    error9 = cantools.db.Signal(name="gps_stale", start=9, length=1, byte_order="little_endian", is_signed=False)
    error10 = cantools.db.Signal(name="gps_link_slow", start=10, length=1, byte_order="little_endian", is_signed=False)
    error11 = cantools.db.Signal(name="wss_left_no_signal", start=11, length=1, byte_order="little_endian", is_signed=False)
    error12 = cantools.db.Signal(name="wss_right_no_signal", start=12, length=1, byte_order="little_endian", is_signed=False)
    error13 = cantools.db.Signal(name="error13", start=13, length=1, byte_order="little_endian", is_signed=False)
    error14 = cantools.db.Signal(name="error14", start=14, length=1, byte_order="little_endian", is_signed=False)
    error15 = cantools.db.Signal(name="error15", start=15, length=1, byte_order="little_endian", is_signed=False)

    # Byte 2: CAN controller health
    error16 = cantools.db.Signal(name="can_bus_off", start=16, length=1, byte_order="little_endian", is_signed=False)
    error17 = cantools.db.Signal(name="can_tx_overflow", start=17, length=1, byte_order="little_endian", is_signed=False)
    error18 = cantools.db.Signal(name="can_rx_overflow", start=18, length=1, byte_order="little_endian", is_signed=False)
    error19 = cantools.db.Signal(name="error19", start=19, length=1, byte_order="little_endian", is_signed=False)
    error20 = cantools.db.Signal(name="error20", start=20, length=1, byte_order="little_endian", is_signed=False)
    error21 = cantools.db.Signal(name="error21", start=21, length=1, byte_order="little_endian", is_signed=False)
    error22 = cantools.db.Signal(name="error22", start=22, length=1, byte_order="little_endian", is_signed=False)
    error23 = cantools.db.Signal(name="error23", start=23, length=1, byte_order="little_endian", is_signed=False)

    # Bytes 3-7: unallocated
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
        name=f"sn_{variant}_heartbeat",
        length=8,
        signals=[
            error0, error1, error2, error3, error4, error5, error6, error7,
            error8, error9, error10, error11, error12, error13, error14, error15,
            error16, error17, error18, error19, error20, error21, error22, error23,
            error24, error25, error26, error27, error28, error29, error30, error31,
            error32, error33, error34, error35, error36, error37, error38, error39,
            error40, error41, error42, error43, error44, error45, error46, error47,
            error48, error49, error50, error51, error52, error53, error54, error55,
            error56, error57, error58, error59, error60, error61, error62, error63,
        ],
        comment="Sensor Node Heartbeat",
        senders=[f"SN_{variant.upper()}"],
        cycle_time=100,
        strict=True
    )

    return msg


