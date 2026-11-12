import cantools
from cantools.database.conversion import BaseConversion

def get_lvpdb_lv_24v_bus_and_12v_bus_voltages(frame_id: int):
    lv_24v_voltage = cantools.db.Signal(
        name="lv_24v_voltage",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False
    )
    
    lv_12v_voltage = cantools.db.Signal(
        name="lv_12v_voltage",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="lvpdb_lv_24v_bus_and_12v_bus_voltages",
        length=4,
        signals=[lv_24v_voltage, lv_12v_voltage],
        comment="LVPDB message for voltages of 24v bus and 12v bus",
        senders=['LVPDB'],
        cycle_time=100,
        strict=True
    )

    return msg

def get_lvpdb_lv_sh_af1_af2_cp_currents(frame_id: int):
    lv_current = cantools.db.Signal(
        name="lv_current",
        start=0,
        length=16,
        byte_order="little_endian",
        is_signed=False
    )
    
    sh_current = cantools.db.Signal(
        name="sh_current",
        start=16,
        length=16,
        byte_order="little_endian",
        is_signed=False
    )
    
    af1_af2_current = cantools.db.Signal(
        name="af1_af2_current",
        start=32,
        length=16,
        byte_order="little_endian",
        is_signed=False
    )
    
    cp_current = cantools.db.Signal(
        name="cp_current",
        start=48,
        length=16,
        byte_order="little_endian",
        is_signed=False
    )
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="lvpdb_lv_sh_lt_bm_l_currents",
        length=8,
        signals=[lv_current, sh_current, af1_af2_current, cp_current],
        comment="LVPDB message for currents of LV, SH, AF_1/AF_2, CP",
        senders=['LVPDB'],
        cycle_time=100,
        strict=True
    )

    return msg

def get_lvpdb_rf_current(frame_id : int):
    
    rf_current = cantools.db.Signal (
        name="rf_current",
        start = 0,
        length= 16,
        byte_order="little_endian",
        is_signed=False
    ) 
    
    
    msg = cantools.db.Message(
        frame_id=frame_id,
        name="lvpdb_rf_currents",
        length=8,
        signals=[rf_current],
        comment="LVPDB message for current of RF",
        senders=['LVPDB'],
        cycle_time=100,
        strict=True
    )

    return msg

def get_lvpdb_heartbeat(frame_id: int):
    error0 = cantools.db.Signal(name="tps_init_failed", start=0, length=1, byte_order="little_endian", is_signed=False)
    
    error1 = cantools.db.Signal(name="tps_lv_poll_failed", start=1, length=1, byte_order="little_endian", is_signed=False)
    error2 = cantools.db.Signal(name="tps_sh_poll_failed", start=2, length=1, byte_order="little_endian", is_signed=False)
    error3 = cantools.db.Signal(name="tps_af1_af2_poll_failed", start=3, length=1, byte_order="little_endian", is_signed=False)
    error4 = cantools.db.Signal(name="tps_cp_poll_failed", start=4, length=1, byte_order="little_endian", is_signed=False)
    error5 = cantools.db.Signal(name="tps_rf_poll_failed", start=5, length=1, byte_order="little_endian", is_signed=False)

    error6 = cantools.db.Signal(name="tps_lv_power_not_good", start=6, length=1, byte_order="little_endian", is_signed=False)
    error7 = cantools.db.Signal(name="tps_sh_power_not_good", start=7, length=1, byte_order="little_endian", is_signed=False)
    error8 = cantools.db.Signal(name="tps_af1_af2_power_not_good", start=8, length=1, byte_order="little_endian", is_signed=False)
    error9 = cantools.db.Signal(name="tps_cp_power_not_good", start=9, length=1, byte_order="little_endian", is_signed=False)
    error10 = cantools.db.Signal(name="tps_rf_power_not_good", start=10, length=1, byte_order="little_endian", is_signed=False)

    error11 = cantools.db.Signal(name="dash_state_stale", start=11, length=1, byte_order="little_endian", is_signed=False)

    msg = cantools.db.Message(
        frame_id=frame_id,
        name="lvpdb_heartbeat",
        length=2,
        signals=[
            error0, error1, error2, error3, error4, error5, error6, error7,
            error8, error9, error10, error11
        ],
        comment="LVPDB Heartbeat",
        senders=['LVPDB'],
        cycle_time=100,
        strict=True
    )

    return msg
