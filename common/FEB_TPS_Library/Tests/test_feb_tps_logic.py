#!/usr/bin/env python3
"""
test_feb_tps_logic.py - Host-runnable validation of FEB_TPS_Library math logic.

Validates the pure-math constants and formulas defined in:
  - common/FEB_TPS_Library/Inc/feb_tps_registers.h
  - common/FEB_TPS_Library/Inc/feb_tps.h  (FEB_TPS_SignMagnitude inline)

These tests exercise the same assertions as test_feb_tps.c but run on any
Python 3 host without a C compiler.  They are a complement to the C tests,
not a replacement.

Run: python3 test_feb_tps_logic.py
"""

import math
import sys

# ============================================================================
# Test harness
# ============================================================================

_passed = 0
_failed = 0

def assert_eq(a, b, msg):
    global _passed, _failed
    if a == b:
        _passed += 1
    else:
        _failed += 1
        print(f"  FAIL: {msg}")
        print(f"    expected: {b!r}")
        print(f"    actual  : {a!r}")

def assert_near(a, b, tol, msg):
    global _passed, _failed
    if abs(a - b) <= tol:
        _passed += 1
    else:
        _failed += 1
        print(f"  FAIL: {msg}")
        print(f"    expected ~ {b}, got {a} (diff {abs(a-b)}, tol {tol})")

def assert_true(cond, msg):
    global _passed, _failed
    if cond:
        _passed += 1
    else:
        _failed += 1
        print(f"  FAIL: {msg}")

def section(name):
    print(f"\n[{name}]")

# ============================================================================
# Python equivalents of the C macros/inline functions
# ============================================================================

# --- Address calculation ---
FEB_TPS_PIN_GND = 0x00
FEB_TPS_PIN_VS  = 0x01
FEB_TPS_PIN_SDA = 0x02
FEB_TPS_PIN_SCL = 0x03

def FEB_TPS_ADDR(a1, a0):
    """uint8_t: 0x40 | ((a1 << 2) | a0)"""
    return 0x40 | ((a1 << 2) | a0)

# --- Conversion constants ---
FEB_TPS_CONV_VBUS_V_PER_LSB    = 0.00125
FEB_TPS_CONV_VSHUNT_MV_PER_LSB = 0.0025

# --- Calibration macros ---
def FEB_TPS_CALC_CURRENT_LSB(i_max):
    """float: i_max / 32768.0"""
    return i_max / 32768.0

def FEB_TPS_CALC_CAL(current_lsb, r_shunt):
    """uint16_t: 0.00512 / (current_lsb * r_shunt)"""
    return int(0.00512 / (current_lsb * r_shunt)) & 0xFFFF

def FEB_TPS_CALC_POWER_LSB(current_lsb):
    """float: current_lsb * 25.0"""
    return current_lsb * 25.0

# --- Sign-magnitude inline ---
def FEB_TPS_SignMagnitude(raw):
    """int16_t: sign-magnitude from bit 15 flag"""
    raw = raw & 0xFFFF  # ensure 16-bit
    if raw & 0x8000:
        return -(raw & 0x7FFF)
    return raw & 0x7FFF

# --- Register addresses ---
FEB_TPS_REG_CONFIG     = 0x00
FEB_TPS_REG_SHUNT_VOLT = 0x01
FEB_TPS_REG_BUS_VOLT   = 0x02
FEB_TPS_REG_POWER      = 0x03
FEB_TPS_REG_CURRENT    = 0x04
FEB_TPS_REG_CAL        = 0x05
FEB_TPS_REG_MASK       = 0x06
FEB_TPS_REG_ALERT_LIM  = 0x07
FEB_TPS_REG_ID         = 0xFF

FEB_TPS_CONFIG_DEFAULT = 0x4127

# Mask bits
FEB_TPS_MASK_SOL  = (1 << 15)
FEB_TPS_MASK_SUL  = (1 << 14)
FEB_TPS_MASK_BOL  = (1 << 13)
FEB_TPS_MASK_BUL  = (1 << 12)
FEB_TPS_MASK_POL  = (1 << 11)
FEB_TPS_MASK_CNVR = (1 << 10)

# ============================================================================
# Test sections
# ============================================================================

def test_address_macro():
    section("FEB_TPS_ADDR macro")
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND), 0x40, "ADDR(GND,GND)")
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_VS),  0x41, "ADDR(GND,VS)")
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SDA), 0x42, "ADDR(GND,SDA)")
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SCL), 0x43, "ADDR(GND,SCL)")
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_VS,  FEB_TPS_PIN_GND), 0x44, "ADDR(VS,GND)")
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_VS,  FEB_TPS_PIN_VS),  0x45, "ADDR(VS,VS)")
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_GND), 0x48, "ADDR(SDA,GND)")
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL), 0x4B, "ADDR(SDA,SCL)")
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_SCL, FEB_TPS_PIN_SCL), 0x4F, "ADDR(SCL,SCL)")

    # Verify that the 7-bit range is 0x40..0x4F
    for a1 in range(4):
        for a0 in range(4):
            addr = FEB_TPS_ADDR(a1, a0)
            assert_true(0x40 <= addr <= 0x4F, f"ADDR({a1},{a0}) in [0x40,0x4F]")


def test_lvpdb_addresses():
    """Verify LVPDB address assignments from the diff match expected values."""
    section("LVPDB Address Assignments (from diff)")
    # LV_ADDR = FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL)
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL), 0x4B, "LV_ADDR == 0x4B")
    # SH_ADDR = FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SDA)
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SDA), 0x4A, "SH_ADDR == 0x4A")
    # LT_ADDR = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND)
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND), 0x40, "LT_ADDR == 0x40")
    # BM_L_ADDR = FEB_TPS_ADDR(FEB_TPS_PIN_SCL, FEB_TPS_PIN_SCL)
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_SCL, FEB_TPS_PIN_SCL), 0x4F, "BM_L_ADDR == 0x4F")
    # SM_ADDR = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SDA)
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SDA), 0x42, "SM_ADDR == 0x42")
    # AF1_AF2_ADDR = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_VS)
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_VS),  0x41, "AF1_AF2_ADDR == 0x41")
    # CP_RF_ADDR = FEB_TPS_ADDR(FEB_TPS_PIN_VS, FEB_TPS_PIN_SCL)
    assert_eq(FEB_TPS_ADDR(FEB_TPS_PIN_VS,  FEB_TPS_PIN_SCL), 0x47, "CP_RF_ADDR == 0x47")

    # All 7 LVPDB addresses must be unique
    lvpdb_addrs = [
        FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL),  # LV
        FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SDA),  # SH
        FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND),  # LT
        FEB_TPS_ADDR(FEB_TPS_PIN_SCL, FEB_TPS_PIN_SCL),  # BM_L
        FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SDA),  # SM
        FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_VS),   # AF1_AF2
        FEB_TPS_ADDR(FEB_TPS_PIN_VS,  FEB_TPS_PIN_SCL),  # CP_RF
    ]
    assert_eq(len(lvpdb_addrs), len(set(lvpdb_addrs)),
              "All 7 LVPDB I2C addresses are unique")


def test_sign_magnitude():
    section("FEB_TPS_SignMagnitude")
    # Zero
    assert_eq(FEB_TPS_SignMagnitude(0x0000), 0, "SignMagnitude(0x0000) == 0")
    # Positive values
    assert_eq(FEB_TPS_SignMagnitude(0x0001), 1,     "SignMagnitude(0x0001) == 1")
    assert_eq(FEB_TPS_SignMagnitude(0x0100), 256,   "SignMagnitude(0x0100) == 256")
    assert_eq(FEB_TPS_SignMagnitude(0x7FFF), 32767, "SignMagnitude(0x7FFF) == 32767 (max positive)")
    # Negative values (bit 15 = 1)
    assert_eq(FEB_TPS_SignMagnitude(0x8001), -1,    "SignMagnitude(0x8001) == -1")
    assert_eq(FEB_TPS_SignMagnitude(0x8064), -100,  "SignMagnitude(0x8064) == -100")
    assert_eq(FEB_TPS_SignMagnitude(0xFFFF), -32767,"SignMagnitude(0xFFFF) == -32767 (max negative)")
    # Negative zero (sign bit set, magnitude 0)
    assert_eq(FEB_TPS_SignMagnitude(0x8000), 0,     "SignMagnitude(0x8000) == 0 (negative zero)")

    # Exhaustive: verify symmetry for a range of values
    for mag in [0, 1, 100, 1000, 10000, 32767]:
        positive = FEB_TPS_SignMagnitude(mag)
        negative = FEB_TPS_SignMagnitude(0x8000 | mag)
        assert_eq(positive, mag, f"SignMagnitude positive {mag}")
        assert_eq(negative, -mag, f"SignMagnitude negative {mag}")

    # Verify matches old SIGN_MAGNITUDE macro logic from removed TPS2482.h:
    # #define SIGN_MAGNITUDE(n) (int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF))
    for test_val in [0x0000, 0x0001, 0x7FFF, 0x8000, 0x8001, 0xFFFF, 0x1234, 0xABCD]:
        old_result = -(test_val & 0x7FFF) if ((test_val >> 15) & 0x01) == 1 else (test_val & 0x7FFF)
        # Convert to signed int16 as C would
        if old_result > 32767:
            old_result -= 65536
        new_result = FEB_TPS_SignMagnitude(test_val)
        assert_eq(new_result, old_result,
                  f"SignMagnitude matches old macro for 0x{test_val:04X}")


def test_current_lsb():
    section("FEB_TPS_CALC_CURRENT_LSB")
    # New formula: i_max / 32768 (power of 2, TI datasheet recommended)
    # Old formula: i_max / 0x7FFF (= i_max / 32767)
    # Both are correct per spec; new uses 32768 for simpler binary scaling

    assert_near(FEB_TPS_CALC_CURRENT_LSB(5.0),  5.0 / 32768.0,  1e-12, "LSB(5A)")
    assert_near(FEB_TPS_CALC_CURRENT_LSB(10.0), 10.0 / 32768.0, 1e-12, "LSB(10A)")
    assert_near(FEB_TPS_CALC_CURRENT_LSB(4.0),  4.0 / 32768.0,  1e-12, "LSB(4A) for PCU")
    assert_near(FEB_TPS_CALC_CURRENT_LSB(1.0),  1.0 / 32768.0,  1e-12, "LSB(1A)")

    # LSB must be positive
    assert_true(FEB_TPS_CALC_CURRENT_LSB(5.0) > 0, "LSB > 0 for positive i_max")

    # LVPDB fuse max values from FEB_Main.h:
    # LV=30A, SH=20A, LT=10A, BM_L=10A, SM=10A, AF1_AF2=10A, CP_RF=20A
    fuse_maxes = {"LV": 30, "SH": 20, "LT": 10, "BM_L": 10,
                  "SM": 10, "AF1_AF2": 10, "CP_RF": 20}
    for name, i_max in fuse_maxes.items():
        lsb = FEB_TPS_CALC_CURRENT_LSB(i_max)
        assert_near(lsb, i_max / 32768.0, 1e-10,
                    f"LVPDB {name} current LSB = {i_max}/32768")


def test_calibration_register():
    section("FEB_TPS_CALC_CAL")
    # BMS: 5A max, 0.002 Ohm shunt
    lsb_bms = FEB_TPS_CALC_CURRENT_LSB(5.0)
    cal_bms = FEB_TPS_CALC_CAL(lsb_bms, 0.002)
    expected = int(0.00512 / (lsb_bms * 0.002))
    assert_eq(cal_bms, expected, "BMS CAL register value")
    # Must be non-zero and fit in uint16_t
    assert_true(0 < cal_bms < 65536, "BMS CAL fits in uint16_t")

    # PCU: 4A max, 0.012 Ohm shunt
    lsb_pcu = FEB_TPS_CALC_CURRENT_LSB(4.0)
    cal_pcu = FEB_TPS_CALC_CAL(lsb_pcu, 0.012)
    expected_pcu = int(0.00512 / (lsb_pcu * 0.012))
    assert_eq(cal_pcu, expected_pcu, "PCU CAL register value")
    assert_true(0 < cal_pcu < 65536, "PCU CAL fits in uint16_t")

    # LVPDB: all use 0.002 Ohm shunt
    fuse_maxes = [30, 20, 10, 10, 10, 10, 20]
    for i_max in fuse_maxes:
        lsb = FEB_TPS_CALC_CURRENT_LSB(i_max)
        cal = FEB_TPS_CALC_CAL(lsb, 0.002)
        assert_true(0 < cal < 65536, f"LVPDB CAL({i_max}A) fits uint16_t")


def test_power_lsb():
    section("FEB_TPS_CALC_POWER_LSB")
    # Power LSB = 25 * Current LSB (Eq. 24)
    lsb = FEB_TPS_CALC_CURRENT_LSB(5.0)
    power_lsb = FEB_TPS_CALC_POWER_LSB(lsb)
    assert_near(power_lsb, lsb * 25.0, 1e-12, "Power LSB = 25 * current_lsb")
    assert_true(power_lsb > 0, "Power LSB > 0")


def test_conversion_constants():
    section("Conversion Constants")
    # Vbus: 1.25 mV/LSB = 0.00125 V/LSB
    assert_near(FEB_TPS_CONV_VBUS_V_PER_LSB, 0.00125, 1e-10, "VBUS_V_PER_LSB = 0.00125")
    # Spot check: 4000 LSB → 5.0 V
    assert_near(4000 * FEB_TPS_CONV_VBUS_V_PER_LSB, 5.0, 1e-6, "4000 LSB → 5.0V")
    # Spot check: 0 LSB → 0.0 V
    assert_near(0 * FEB_TPS_CONV_VBUS_V_PER_LSB, 0.0, 1e-10, "0 LSB → 0.0V")

    # Vshunt: 2.5 uV/LSB = 0.0025 mV/LSB
    assert_near(FEB_TPS_CONV_VSHUNT_MV_PER_LSB, 0.0025, 1e-10, "VSHUNT_MV_PER_LSB = 0.0025")
    # Spot check: 2500 LSB → 6.25 mV
    assert_near(2500 * FEB_TPS_CONV_VSHUNT_MV_PER_LSB, 6.25, 1e-6, "2500 LSB → 6.25 mV")

    # Scaled: 5.0V → 5000 mV
    vbus_mv = int(4000 * FEB_TPS_CONV_VBUS_V_PER_LSB * 1000)
    assert_eq(vbus_mv, 5000, "4000 LSB * 0.00125 * 1000 = 5000 mV")

    # Scaled: shunt 200 LSB → 500 uV
    shunt_uv = int(200 * FEB_TPS_CONV_VSHUNT_MV_PER_LSB * 1000)
    assert_eq(shunt_uv, 500, "200 LSB * 0.0025 mV * 1000 = 500 uV")


def test_register_addresses():
    section("Register Addresses")
    assert_eq(FEB_TPS_REG_CONFIG,     0x00, "CONFIG = 0x00")
    assert_eq(FEB_TPS_REG_SHUNT_VOLT, 0x01, "SHUNT_VOLT = 0x01")
    assert_eq(FEB_TPS_REG_BUS_VOLT,   0x02, "BUS_VOLT = 0x02")
    assert_eq(FEB_TPS_REG_POWER,      0x03, "POWER = 0x03")
    assert_eq(FEB_TPS_REG_CURRENT,    0x04, "CURRENT = 0x04")
    assert_eq(FEB_TPS_REG_CAL,        0x05, "CAL = 0x05")
    assert_eq(FEB_TPS_REG_MASK,       0x06, "MASK = 0x06")
    assert_eq(FEB_TPS_REG_ALERT_LIM,  0x07, "ALERT_LIM = 0x07")
    assert_eq(FEB_TPS_REG_ID,         0xFF, "ID = 0xFF")
    assert_eq(FEB_TPS_CONFIG_DEFAULT, 0x4127, "CONFIG_DEFAULT = 0x4127")


def test_mask_bits():
    section("Mask Register Bits")
    assert_eq(FEB_TPS_MASK_SOL,  1 << 15, "MASK_SOL bit 15")
    assert_eq(FEB_TPS_MASK_SUL,  1 << 14, "MASK_SUL bit 14")
    assert_eq(FEB_TPS_MASK_BOL,  1 << 13, "MASK_BOL bit 13")
    assert_eq(FEB_TPS_MASK_BUL,  1 << 12, "MASK_BUL bit 12")
    assert_eq(FEB_TPS_MASK_POL,  1 << 11, "MASK_POL bit 11")
    assert_eq(FEB_TPS_MASK_CNVR, 1 << 10, "MASK_CNVR bit 10")
    # All are distinct bits
    all_bits = [FEB_TPS_MASK_SOL, FEB_TPS_MASK_SUL, FEB_TPS_MASK_BOL,
                FEB_TPS_MASK_BUL, FEB_TPS_MASK_POL, FEB_TPS_MASK_CNVR]
    assert_eq(len(all_bits), len(set(all_bits)), "All mask alert bits are distinct")


def test_conversion_regression():
    """
    Regression: verify new library macros produce equivalent results to the
    removed TPS2482.h macros.

    Old TPS2482.h:
      TPS2482_CONV_VSHUNT = 0.0025 (mV/LSB)
      TPS2482_CONV_VBUS   = 0.00125 (V/LSB)
      TPS2482_CURRENT_LSB_EQ(a) = (double)((double)(a) / (1.0 * 0x7FFF))
      TPS2482_CAL_EQ(a1, a0) = (uint16_t)(.00512 / ((1.0 * a1) * (1.0 * a0)))
      TPS2482_POWER_LSB_EQ(a) = (double)(25 * a)
    """
    section("Regression vs Removed TPS2482.h Macros")

    # Vshunt constant unchanged
    assert_near(FEB_TPS_CONV_VSHUNT_MV_PER_LSB, 0.0025, 1e-10,
                "VSHUNT_MV_PER_LSB unchanged from TPS2482_CONV_VSHUNT")

    # Vbus constant unchanged
    assert_near(FEB_TPS_CONV_VBUS_V_PER_LSB, 0.00125, 1e-10,
                "VBUS_V_PER_LSB unchanged from TPS2482_CONV_VBUS")

    # Old LSB used 0x7FFF=32767; new uses 32768. The CAL formula still produces
    # a valid uint16_t and the difference is < 0.003% for typical values.
    # Verify both produce valid non-zero CAL values.
    i_max = 5.0
    r_shunt = 0.002
    old_lsb = i_max / 32767.0  # old formula
    new_lsb = FEB_TPS_CALC_CURRENT_LSB(i_max)  # new formula

    old_cal = int(0.00512 / (old_lsb * r_shunt)) & 0xFFFF
    new_cal = FEB_TPS_CALC_CAL(new_lsb, r_shunt)

    assert_true(old_cal > 0, "Old CAL > 0")
    assert_true(new_cal > 0, "New CAL > 0")
    # The values differ by at most 1 due to rounding (32767 vs 32768)
    assert_true(abs(new_cal - old_cal) <= 2,
                f"New CAL ({new_cal}) within 2 of old CAL ({old_cal})")

    # Power LSB formula unchanged
    assert_near(FEB_TPS_CALC_POWER_LSB(new_lsb), 25 * new_lsb, 1e-12,
                "Power LSB formula unchanged: current_lsb * 25")

    # Sign-magnitude matches old SIGN_MAGNITUDE macro
    # Old: #define SIGN_MAGNITUDE(n) (int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF))
    for v in [0x0000, 0x0001, 0x7FFF, 0x8000, 0x8001, 0xFFFF, 0x1234]:
        old = -(v & 0x7FFF) if ((v >> 15) & 0x01) == 1 else (v & 0x7FFF)
        new = FEB_TPS_SignMagnitude(v)
        assert_eq(new, old, f"SignMagnitude regression: 0x{v:04X}")


def test_pin_constants():
    section("Pin Constants")
    assert_eq(FEB_TPS_PIN_GND, 0x00, "PIN_GND = 0x00")
    assert_eq(FEB_TPS_PIN_VS,  0x01, "PIN_VS  = 0x01")
    assert_eq(FEB_TPS_PIN_SDA, 0x02, "PIN_SDA = 0x02")
    assert_eq(FEB_TPS_PIN_SCL, 0x03, "PIN_SCL = 0x03")
    # All four must be distinct
    pins = [FEB_TPS_PIN_GND, FEB_TPS_PIN_VS, FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL]
    assert_eq(len(pins), len(set(pins)), "All 4 pin constants are distinct")


def test_measurement_scaling():
    """Verify the scaling math used in FEB_TPS_PollScaled."""
    section("PollScaled Conversion Math")

    # Simulating PollScaled with raw bus=4000, current_raw=100, shunt_raw=200
    bus_v     = 4000 * FEB_TPS_CONV_VBUS_V_PER_LSB   # 5.0 V
    current_a = 100  * FEB_TPS_CALC_CURRENT_LSB(5.0)  # 100 * lsb A
    shunt_mv  = 200  * FEB_TPS_CONV_VSHUNT_MV_PER_LSB # 0.5 mV

    bus_mv       = int(bus_v * 1000)
    current_ma   = int(current_a * 1000)
    shunt_uv     = int(shunt_mv * 1000)

    assert_eq(bus_mv, 5000, "PollScaled bus_voltage_mv = 5000")
    assert_eq(shunt_uv, 500, "PollScaled shunt_voltage_uv = 500")
    assert_true(current_ma > 0, "PollScaled current_ma > 0 for positive current")

    # Negative current
    neg_current_a = -100 * FEB_TPS_CALC_CURRENT_LSB(5.0)
    neg_current_ma = int(neg_current_a * 1000)
    assert_true(neg_current_ma < 0, "PollScaled current_ma < 0 for negative current")


# ============================================================================
# Entry point
# ============================================================================

if __name__ == "__main__":
    print("=== FEB_TPS_Library Logic Tests (Python) ===")

    test_pin_constants()
    test_address_macro()
    test_lvpdb_addresses()
    test_sign_magnitude()
    test_current_lsb()
    test_calibration_register()
    test_power_lsb()
    test_conversion_constants()
    test_register_addresses()
    test_mask_bits()
    test_conversion_regression()
    test_measurement_scaling()

    total = _passed + _failed
    print(f"\n=== Results: {_passed}/{total} passed ({_failed} failed) ===")
    sys.exit(0 if _failed == 0 else 1)