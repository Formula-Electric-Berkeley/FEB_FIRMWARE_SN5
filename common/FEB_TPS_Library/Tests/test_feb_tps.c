/**
 * @file test_feb_tps.c
 * @brief Unit tests for common/FEB_TPS_Library
 *
 * Tests cover the new FEB_TPS_Library introduced in this PR, which centralizes
 * TPS2482 power-monitoring support previously duplicated across BMS, LVPDB, PCU.
 *
 * Test categories:
 *   1. Macros and static inline functions (no HAL calls)
 *   2. Library lifecycle (Init/DeInit/IsInitialized)
 *   3. Device registration (DeviceRegister / DeviceUnregister)
 *   4. Device query helpers (GetCount, GetByIndex, GetCurrentLSB, GetCalibration)
 *   5. Single-device measurement (Poll, PollBusVoltage, PollCurrent, PollScaled, PollRaw)
 *   6. Batch operations (PollAll, PollAllScaled, PollAllRaw)
 *   7. GPIO control (Enable, ReadPowerGood, ReadAlert, EnableAll, ReadAllPowerGood)
 *   8. Configuration (Reconfigure, ReadID)
 *   9. Utility (StatusToString, GetDeviceName)
 *  10. Error / boundary cases
 */

#include "hal_mock.h"   /* Must be first – defines I2C_HandleTypeDef, GPIO_TypeDef, etc. */

/* Force bare-metal path so tests run without FreeRTOS */
#define FEB_TPS_USE_FREERTOS 0

#include "../Inc/feb_tps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Minimal Test Framework
 * ============================================================================ */

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                          \
    do {                                                                \
        g_tests_run++;                                                  \
        if (cond) {                                                     \
            g_tests_passed++;                                           \
        } else {                                                        \
            g_tests_failed++;                                           \
            fprintf(stderr, "  FAIL [%s:%d] %s\n",                    \
                    __FILE__, __LINE__, (msg));                         \
        }                                                               \
    } while (0)

#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_ASSERT_NE(a, b, msg) TEST_ASSERT((a) != (b), msg)
#define TEST_ASSERT_NULL(ptr, msg) TEST_ASSERT((ptr) == NULL, msg)
#define TEST_ASSERT_NOT_NULL(ptr, msg) TEST_ASSERT((ptr) != NULL, msg)

/** Float comparison within a tolerance */
#define TEST_ASSERT_FLOAT_NEAR(a, b, tol, msg) \
    TEST_ASSERT(fabsf((float)(a) - (float)(b)) <= (float)(tol), msg)

static void test_section(const char *name)
{
    printf("\n[%s]\n", name);
}

/* ============================================================================
 * Shared Test Fixtures
 * ============================================================================ */

static I2C_HandleTypeDef g_i2c;
static GPIO_TypeDef g_en_port;
static GPIO_TypeDef g_pg_port;
static GPIO_TypeDef g_alert_port;

/** Minimal valid device config */
static FEB_TPS_DeviceConfig_t make_config(void)
{
    FEB_TPS_DeviceConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.hi2c        = &g_i2c;
    cfg.i2c_addr    = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND); /* 0x40 */
    cfg.r_shunt_ohms = 0.002f;
    cfg.i_max_amps   = 5.0f;
    cfg.config_reg   = FEB_TPS_CONFIG_DEFAULT;
    cfg.name         = "TEST";
    return cfg;
}

/** Helper: init library, register one device, return handle */
static FEB_TPS_Handle_t setup_one_device(void)
{
    FEB_TPS_Init(NULL);
    FEB_TPS_DeviceConfig_t cfg = make_config();
    FEB_TPS_Handle_t h = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h);
    return h;
}

/* ============================================================================
 * Section 1: Macros & Static Inline Functions
 * ============================================================================ */

static void test_macros(void)
{
    test_section("Macros and Inline Functions");

    /* --- FEB_TPS_ADDR --- */
    /* Base address is 0x40; A1 shifts by 2 bits, A0 is low 2 bits */
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND), 0x40,
                   "ADDR(GND,GND) == 0x40");
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_VS),  0x41,
                   "ADDR(GND,VS) == 0x41");
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SDA), 0x42,
                   "ADDR(GND,SDA) == 0x42");
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SCL), 0x43,
                   "ADDR(GND,SCL) == 0x43");
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_VS,  FEB_TPS_PIN_GND), 0x44,
                   "ADDR(VS,GND) == 0x44");
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL), 0x4B,
                   "ADDR(SDA,SCL) == 0x4B");
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_SCL, FEB_TPS_PIN_SCL), 0x4F,
                   "ADDR(SCL,SCL) == 0x4F");

    /* --- FEB_TPS_CALC_CURRENT_LSB --- */
    /* i_max / 32768 */
    TEST_ASSERT_FLOAT_NEAR(FEB_TPS_CALC_CURRENT_LSB(5.0f),
                           5.0f / 32768.0f, 1e-9f,
                           "CURRENT_LSB(5A)");
    TEST_ASSERT_FLOAT_NEAR(FEB_TPS_CALC_CURRENT_LSB(10.0f),
                           10.0f / 32768.0f, 1e-9f,
                           "CURRENT_LSB(10A)");

    /* --- FEB_TPS_CALC_CAL --- */
    /* 0.00512 / (current_lsb * r_shunt) */
    {
        float lsb = FEB_TPS_CALC_CURRENT_LSB(5.0f);
        float r   = 0.002f;
        uint16_t expected = (uint16_t)(0.00512f / (lsb * r));
        TEST_ASSERT_EQ(FEB_TPS_CALC_CAL(lsb, r), expected,
                       "CALC_CAL(5A, 2mOhm)");
    }

    /* --- FEB_TPS_CALC_POWER_LSB --- */
    /* current_lsb * 25 */
    {
        float lsb = FEB_TPS_CALC_CURRENT_LSB(5.0f);
        TEST_ASSERT_FLOAT_NEAR(FEB_TPS_CALC_POWER_LSB(lsb),
                               lsb * 25.0f, 1e-9f,
                               "CALC_POWER_LSB");
    }

    /* --- Conversion constants --- */
    TEST_ASSERT_FLOAT_NEAR(FEB_TPS_CONV_VBUS_V_PER_LSB, 0.00125f, 1e-10f,
                           "CONV_VBUS_V_PER_LSB == 0.00125");
    TEST_ASSERT_FLOAT_NEAR(FEB_TPS_CONV_VSHUNT_MV_PER_LSB, 0.0025f, 1e-10f,
                           "CONV_VSHUNT_MV_PER_LSB == 0.0025");

    /* --- FEB_TPS_SignMagnitude --- */
    /* Zero */
    TEST_ASSERT_EQ(FEB_TPS_SignMagnitude(0x0000), 0,
                   "SignMagnitude(0) == 0");
    /* Positive value: bit 15 = 0 */
    TEST_ASSERT_EQ(FEB_TPS_SignMagnitude(0x0100), 0x0100,
                   "SignMagnitude(0x0100) == 256 (positive)");
    /* Maximum positive: 0x7FFF = 32767 */
    TEST_ASSERT_EQ(FEB_TPS_SignMagnitude(0x7FFF), 32767,
                   "SignMagnitude(0x7FFF) == 32767");
    /* Negative value: bit 15 = 1 */
    TEST_ASSERT_EQ(FEB_TPS_SignMagnitude(0x8001), -1,
                   "SignMagnitude(0x8001) == -1");
    /* Maximum negative magnitude: 0xFFFF -> -(0x7FFF) = -32767 */
    TEST_ASSERT_EQ(FEB_TPS_SignMagnitude(0xFFFF), -32767,
                   "SignMagnitude(0xFFFF) == -32767");
    /* Negative zero: 0x8000 -> -(0) = 0 */
    TEST_ASSERT_EQ(FEB_TPS_SignMagnitude(0x8000), 0,
                   "SignMagnitude(0x8000) == 0 (negative zero)");
    /* Typical current reading: value = 0x0064 = 100 */
    TEST_ASSERT_EQ(FEB_TPS_SignMagnitude(0x0064), 100,
                   "SignMagnitude(100) == 100");
    /* Negative typical: 0x8064 -> -100 */
    TEST_ASSERT_EQ(FEB_TPS_SignMagnitude(0x8064), -100,
                   "SignMagnitude(0x8064) == -100");

    /* --- Register address constants --- */
    TEST_ASSERT_EQ(FEB_TPS_REG_CONFIG,     0x00, "REG_CONFIG == 0x00");
    TEST_ASSERT_EQ(FEB_TPS_REG_SHUNT_VOLT, 0x01, "REG_SHUNT_VOLT == 0x01");
    TEST_ASSERT_EQ(FEB_TPS_REG_BUS_VOLT,   0x02, "REG_BUS_VOLT == 0x02");
    TEST_ASSERT_EQ(FEB_TPS_REG_POWER,      0x03, "REG_POWER == 0x03");
    TEST_ASSERT_EQ(FEB_TPS_REG_CURRENT,    0x04, "REG_CURRENT == 0x04");
    TEST_ASSERT_EQ(FEB_TPS_REG_CAL,        0x05, "REG_CAL == 0x05");
    TEST_ASSERT_EQ(FEB_TPS_REG_MASK,       0x06, "REG_MASK == 0x06");
    TEST_ASSERT_EQ(FEB_TPS_REG_ALERT_LIM,  0x07, "REG_ALERT_LIM == 0x07");
    TEST_ASSERT_EQ(FEB_TPS_REG_ID,         0xFF, "REG_ID == 0xFF");

    /* --- CONFIG default --- */
    TEST_ASSERT_EQ(FEB_TPS_CONFIG_DEFAULT, 0x4127, "CONFIG_DEFAULT == 0x4127");
}

/* ============================================================================
 * Section 2: Library Lifecycle
 * ============================================================================ */

static void test_lifecycle(void)
{
    test_section("Library Lifecycle");

    /* Not initialized initially */
    FEB_TPS_DeInit(); /* ensure clean state */
    TEST_ASSERT_EQ(FEB_TPS_IsInitialized(), false, "Not initialized before Init");

    /* Init with NULL config uses defaults */
    FEB_TPS_Status_t s = FEB_TPS_Init(NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Init(NULL) returns OK");
    TEST_ASSERT_EQ(FEB_TPS_IsInitialized(), true, "IsInitialized() true after Init");

    /* Calling Init again is idempotent */
    s = FEB_TPS_Init(NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Second Init returns OK (idempotent)");

    /* DeInit resets state */
    FEB_TPS_DeInit();
    TEST_ASSERT_EQ(FEB_TPS_IsInitialized(), false, "IsInitialized() false after DeInit");
    TEST_ASSERT_EQ(FEB_TPS_DeviceGetCount(), 0, "DeviceGetCount == 0 after DeInit");

    /* Init with custom timeout */
    FEB_TPS_LibConfig_t cfg = { .i2c_timeout_ms = 500 };
    s = FEB_TPS_Init(&cfg);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Init with custom timeout returns OK");
    FEB_TPS_DeInit();

    /* Init with zero timeout uses default */
    FEB_TPS_LibConfig_t cfg_zero = { .i2c_timeout_ms = 0 };
    s = FEB_TPS_Init(&cfg_zero);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Init with zero timeout returns OK (uses default)");
    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 3: Device Registration
 * ============================================================================ */

static void test_device_register(void)
{
    test_section("Device Registration");

    FEB_TPS_Init(NULL);
    hal_mock_reset();

    /* --- NULL argument guards --- */
    FEB_TPS_Handle_t h = NULL;
    FEB_TPS_Status_t s;

    s = FEB_TPS_DeviceRegister(NULL, &h);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Register(NULL config) = ERR_INVALID_ARG");

    FEB_TPS_DeviceConfig_t cfg = make_config();
    s = FEB_TPS_DeviceRegister(&cfg, NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Register(NULL handle) = ERR_INVALID_ARG");

    /* --- NULL I2C handle --- */
    cfg = make_config();
    cfg.hi2c = NULL;
    s = FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Register(NULL hi2c) = ERR_INVALID_ARG");

    /* --- Invalid shunt resistor --- */
    cfg = make_config();
    cfg.r_shunt_ohms = 0.0f;
    s = FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Register(r_shunt=0) = ERR_INVALID_ARG");

    cfg.r_shunt_ohms = -1.0f;
    s = FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Register(r_shunt<0) = ERR_INVALID_ARG");

    /* --- Invalid max current --- */
    cfg = make_config();
    cfg.i_max_amps = 0.0f;
    s = FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Register(i_max=0) = ERR_INVALID_ARG");

    /* --- Register before Init --- */
    FEB_TPS_DeInit();
    hal_mock_reset();
    cfg = make_config();
    s = FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_NOT_INIT, "Register before Init = ERR_NOT_INIT");

    /* --- Successful registration --- */
    FEB_TPS_Init(NULL);
    hal_mock_reset();
    cfg = make_config();
    s = FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Register(valid) = OK");
    TEST_ASSERT_NOT_NULL(h, "Handle is not NULL after successful registration");
    TEST_ASSERT_EQ(FEB_TPS_DeviceGetCount(), 1, "DeviceGetCount == 1 after registration");

    /* Verify CONFIG write (first I2C write) */
    TEST_ASSERT(hal_mock_i2c_write_count() >= 2,
                "At least 2 I2C writes (config + cal) during registration");
    /* First write should be to CONFIG register (0x00) */
    I2C_WriteRecord_t w0 = hal_mock_i2c_write_get(0);
    TEST_ASSERT_EQ(w0.mem_addr, FEB_TPS_REG_CONFIG, "First write to CONFIG register");
    TEST_ASSERT_EQ(w0.value, FEB_TPS_CONFIG_DEFAULT, "CONFIG value = default");
    /* Second write should be to CAL register (0x05) */
    I2C_WriteRecord_t w1 = hal_mock_i2c_write_get(1);
    TEST_ASSERT_EQ(w1.mem_addr, FEB_TPS_REG_CAL, "Second write to CAL register");

    /* --- I2C failure during registration --- */
    FEB_TPS_DeInit();
    hal_mock_reset();
    FEB_TPS_Init(NULL);
    hal_mock_i2c_write_push(HAL_ERROR); /* first write (CONFIG) fails */
    cfg = make_config();
    h = NULL;
    s = FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_I2C, "Register returns ERR_I2C on I2C failure");
    /* Note: device_count should NOT increment on failure */
    /* The handle should be NULL or the count stays at 0 */
    TEST_ASSERT_EQ(FEB_TPS_DeviceGetCount(), 0,
                   "DeviceGetCount stays 0 on I2C failure");

    /* --- Register with optional mask register --- */
    FEB_TPS_DeInit();
    hal_mock_reset();
    FEB_TPS_Init(NULL);
    cfg = make_config();
    cfg.mask_reg = FEB_TPS_MASK_SOL;
    h = NULL;
    s = FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Register with mask_reg = OK");
    /* Should have written CONFIG, CAL, MASK, and attempted ID read */
    int wc = hal_mock_i2c_write_count();
    TEST_ASSERT(wc >= 3, "3+ writes when mask_reg set (config, cal, mask)");
    bool found_mask_write = false;
    for (int i = 0; i < wc; i++) {
        I2C_WriteRecord_t wr = hal_mock_i2c_write_get(i);
        if (wr.mem_addr == FEB_TPS_REG_MASK) {
            found_mask_write = true;
            TEST_ASSERT_EQ(wr.value, FEB_TPS_MASK_SOL, "MASK written with SOL bit");
        }
    }
    TEST_ASSERT(found_mask_write, "MASK register was written when mask_reg != 0");

    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 4: Device Query Helpers
 * ============================================================================ */

static void test_device_queries(void)
{
    test_section("Device Query Helpers");

    FEB_TPS_Init(NULL);
    hal_mock_reset();

    /* No devices yet */
    TEST_ASSERT_EQ(FEB_TPS_DeviceGetCount(), 0, "GetCount == 0 initially");
    TEST_ASSERT_NULL(FEB_TPS_DeviceGetByIndex(0), "GetByIndex(0) == NULL when empty");

    /* Register one device */
    FEB_TPS_DeviceConfig_t cfg = make_config();
    FEB_TPS_Handle_t h = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h);

    TEST_ASSERT_EQ(FEB_TPS_DeviceGetCount(), 1, "GetCount == 1 after register");
    FEB_TPS_Handle_t by_idx = FEB_TPS_DeviceGetByIndex(0);
    TEST_ASSERT_NOT_NULL(by_idx, "GetByIndex(0) != NULL");
    TEST_ASSERT_EQ(by_idx, h, "GetByIndex(0) == registered handle");

    /* Out of range index */
    TEST_ASSERT_NULL(FEB_TPS_DeviceGetByIndex(1), "GetByIndex(1) == NULL (out of range)");
    TEST_ASSERT_NULL(FEB_TPS_DeviceGetByIndex(255), "GetByIndex(255) == NULL");

    /* Verify calibration values */
    float expected_lsb = cfg.i_max_amps / 32768.0f;
    float actual_lsb = FEB_TPS_GetCurrentLSB(h);
    TEST_ASSERT_FLOAT_NEAR(actual_lsb, expected_lsb, 1e-9f,
                           "GetCurrentLSB matches i_max/32768");

    /* NULL handle returns 0 */
    TEST_ASSERT_EQ(FEB_TPS_GetCurrentLSB(NULL), 0.0f,
                   "GetCurrentLSB(NULL) == 0");

    /* Calibration register */
    uint16_t expected_cal = (uint16_t)(0.00512f / (expected_lsb * cfg.r_shunt_ohms));
    uint16_t actual_cal = FEB_TPS_GetCalibration(h);
    TEST_ASSERT_EQ(actual_cal, expected_cal, "GetCalibration matches formula");
    TEST_ASSERT_EQ(FEB_TPS_GetCalibration(NULL), 0, "GetCalibration(NULL) == 0");

    /* Unregister */
    FEB_TPS_DeviceUnregister(h);
    /* After unregister the count stays but device is flagged not-initialized */
    TEST_ASSERT_NULL(FEB_TPS_DeviceGetByIndex(0),
                     "GetByIndex(0) == NULL after Unregister");

    /* NULL unregister is safe */
    FEB_TPS_DeviceUnregister(NULL); /* must not crash */

    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 5: Single-Device Measurement
 * ============================================================================ */

/**
 * Helper: push 4 read responses for Poll (bus_v, current, shunt_v, power)
 * all as raw 16-bit values split into MSB/LSB.
 */
static void push_poll_responses(uint16_t bus_v, uint16_t current,
                                 uint16_t shunt_v, uint16_t power)
{
    hal_mock_i2c_read_push(HAL_OK, (uint8_t)(bus_v >> 8),   (uint8_t)(bus_v & 0xFF));
    hal_mock_i2c_read_push(HAL_OK, (uint8_t)(current >> 8), (uint8_t)(current & 0xFF));
    hal_mock_i2c_read_push(HAL_OK, (uint8_t)(shunt_v >> 8), (uint8_t)(shunt_v & 0xFF));
    hal_mock_i2c_read_push(HAL_OK, (uint8_t)(power >> 8),   (uint8_t)(power & 0xFF));
}

static void test_measurement(void)
{
    test_section("Single-Device Measurement");

    /* --- NULL guards --- */
    FEB_TPS_DeInit();
    FEB_TPS_Init(NULL);
    hal_mock_reset();
    FEB_TPS_Handle_t h = setup_one_device();

    FEB_TPS_Measurement_t meas;
    FEB_TPS_Status_t s;

    s = FEB_TPS_Poll(NULL, &meas);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Poll(NULL handle) = ERR_INVALID_ARG");

    s = FEB_TPS_Poll(h, NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Poll(NULL meas) = ERR_INVALID_ARG");

    /* --- Poll when library not initialized --- */
    FEB_TPS_DeInit();
    hal_mock_reset();
    s = FEB_TPS_Poll(h, &meas);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_NOT_INIT,
                   "Poll after DeInit = ERR_NOT_INIT");

    /* --- Successful Poll --- */
    FEB_TPS_Init(NULL);
    hal_mock_reset();
    FEB_TPS_DeviceConfig_t cfg = make_config();
    h = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h);

    /* Push bus=4000 raw (4000*1.25mV = 5.0V), current=0x0064=100 (+),
     * shunt=200 raw (200*0.0025=0.5mV), power=0x0010=16 */
    push_poll_responses(4000, 0x0064, 200, 16);

    s = FEB_TPS_Poll(h, &meas);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Poll succeeds with mock I2C");

    /* Bus voltage */
    TEST_ASSERT_EQ(meas.bus_voltage_raw, 4000, "bus_voltage_raw == 4000");
    TEST_ASSERT_FLOAT_NEAR(meas.bus_voltage_v, 4000 * 0.00125f, 1e-4f,
                           "bus_voltage_v == 5.0V");

    /* Current (positive sign-magnitude) */
    TEST_ASSERT_EQ(meas.current_raw, 100, "current_raw == 100 (positive)");
    float current_lsb = cfg.i_max_amps / 32768.0f;
    TEST_ASSERT_FLOAT_NEAR(meas.current_a, 100.0f * current_lsb, 1e-6f,
                           "current_a == 100 * current_lsb");

    /* Shunt voltage */
    TEST_ASSERT_EQ(meas.shunt_voltage_raw, 200, "shunt_voltage_raw == 200");
    TEST_ASSERT_FLOAT_NEAR(meas.shunt_voltage_mv, 200 * 0.0025f, 1e-5f,
                           "shunt_voltage_mv == 0.5mV");

    /* Power */
    TEST_ASSERT_EQ(meas.power_raw, 16, "power_raw == 16");

    /* --- Negative current Poll --- */
    hal_mock_reset();
    push_poll_responses(3200, 0x8064, 0x8032, 0x0020);
    /* 0x8064 = negative 100 -> current = -100 */
    /* 0x8032 = negative 50 -> shunt = -50 */

    s = FEB_TPS_Poll(h, &meas);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Poll with negative current succeeds");
    TEST_ASSERT_EQ(meas.current_raw, -100, "current_raw == -100 (negative)");
    TEST_ASSERT(meas.current_a < 0.0f, "current_a is negative");
    TEST_ASSERT_EQ(meas.shunt_voltage_raw, -50, "shunt_voltage_raw == -50");
    TEST_ASSERT(meas.shunt_voltage_mv < 0.0f, "shunt_voltage_mv is negative");

    /* --- I2C failure during Poll --- */
    hal_mock_reset();
    hal_mock_i2c_read_push(HAL_ERROR, 0, 0); /* bus voltage read fails */
    s = FEB_TPS_Poll(h, &meas);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_I2C, "Poll returns ERR_I2C on I2C failure");

    /* --- PollBusVoltage --- */
    hal_mock_reset();
    hal_mock_i2c_read_push(HAL_OK, 0x09, 0xC4); /* 0x09C4 = 2500 -> 3.125V */
    float v = 0.0f;
    s = FEB_TPS_PollBusVoltage(h, &v);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "PollBusVoltage succeeds");
    TEST_ASSERT_FLOAT_NEAR(v, 2500 * 0.00125f, 1e-4f,
                           "PollBusVoltage voltage correct");

    s = FEB_TPS_PollBusVoltage(NULL, &v);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "PollBusVoltage(NULL) = ERR_INVALID_ARG");
    s = FEB_TPS_PollBusVoltage(h, NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "PollBusVoltage(NULL ptr) = ERR_INVALID_ARG");

    /* --- PollCurrent --- */
    hal_mock_reset();
    hal_mock_i2c_read_push(HAL_OK, 0x00, 0xC8); /* 0x00C8 = 200, positive */
    float ia = 0.0f;
    s = FEB_TPS_PollCurrent(h, &ia);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "PollCurrent succeeds");
    TEST_ASSERT_FLOAT_NEAR(ia, 200.0f * current_lsb, 1e-6f,
                           "PollCurrent value correct");

    s = FEB_TPS_PollCurrent(NULL, &ia);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "PollCurrent(NULL) = ERR_INVALID_ARG");

    /* --- PollRaw --- */
    hal_mock_reset();
    hal_mock_i2c_read_push(HAL_OK, 0x03, 0xE8); /* bus: 0x03E8 = 1000 */
    hal_mock_i2c_read_push(HAL_OK, 0x00, 0x32); /* current: 0x0032 = 50 */
    hal_mock_i2c_read_push(HAL_OK, 0x00, 0x19); /* shunt: 0x0019 = 25 */

    uint16_t bv_raw = 0;
    int16_t  cur_raw = 0;
    int16_t  sv_raw = 0;
    s = FEB_TPS_PollRaw(h, &bv_raw, &cur_raw, &sv_raw);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "PollRaw succeeds");
    TEST_ASSERT_EQ(bv_raw, 1000, "PollRaw bus_v_raw == 1000");
    TEST_ASSERT_EQ(cur_raw, 50,  "PollRaw current_raw == 50");
    TEST_ASSERT_EQ(sv_raw, 25,   "PollRaw shunt_v_raw == 25");

    /* PollRaw with NULL pointers (skip those reads) */
    hal_mock_reset();
    hal_mock_i2c_read_push(HAL_OK, 0x07, 0xD0); /* bus: 0x07D0 = 2000 */
    bv_raw = 0;
    s = FEB_TPS_PollRaw(h, &bv_raw, NULL, NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "PollRaw with NULL current/shunt succeeds");
    TEST_ASSERT_EQ(bv_raw, 2000, "PollRaw bus only == 2000");
    TEST_ASSERT_EQ(hal_mock_i2c_read_count(), 1,
                   "PollRaw with 2 NULL args reads only 1 register");

    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 6: Scaled Measurements (PollScaled)
 * ============================================================================ */

static void test_poll_scaled(void)
{
    test_section("Scaled Measurements (PollScaled)");

    FEB_TPS_Init(NULL);
    hal_mock_reset();
    FEB_TPS_DeviceConfig_t cfg = make_config();
    FEB_TPS_Handle_t h = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h);

    /* bus=4000 raw -> 5000mV; current=0x0064=100 -> 100*lsb A -> *1000 mA;
     * shunt=200 raw -> 0.5mV -> 500uV; power=16 raw -> 16*power_lsb W -> *1000 mW */
    push_poll_responses(4000, 0x0064, 200, 16);

    FEB_TPS_MeasurementScaled_t scaled;
    FEB_TPS_Status_t s = FEB_TPS_PollScaled(h, &scaled);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "PollScaled succeeds");

    /* bus_voltage_mv = 4000 * 0.00125 * 1000 = 5000 */
    TEST_ASSERT_EQ(scaled.bus_voltage_mv, 5000, "bus_voltage_mv == 5000");

    /* current_ma: 100 * (5/32768) * 1000 */
    float expected_ma = 100.0f * (5.0f / 32768.0f) * 1000.0f;
    int16_t expected_ma_i = (int16_t)expected_ma;
    TEST_ASSERT_EQ(scaled.current_ma, expected_ma_i, "current_ma correct");

    /* shunt_voltage_uv = 200 * 0.0025 * 1000 = 500 */
    TEST_ASSERT_EQ(scaled.shunt_voltage_uv, 500, "shunt_voltage_uv == 500");

    /* NULL scaled pointer: should still return FEB_TPS_OK (Poll passes, null check) */
    push_poll_responses(4000, 0x0064, 200, 16);
    s = FEB_TPS_PollScaled(h, NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "PollScaled(NULL scaled) returns OK (no crash)");

    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 7: Batch Operations
 * ============================================================================ */

static void test_batch_operations(void)
{
    test_section("Batch Operations");

    FEB_TPS_Init(NULL);
    hal_mock_reset();

    /* Register 2 devices */
    FEB_TPS_DeviceConfig_t cfg1 = make_config();
    cfg1.i2c_addr = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND);
    cfg1.name = "DEV0";
    FEB_TPS_Handle_t h1 = NULL;
    FEB_TPS_DeviceRegister(&cfg1, &h1);

    FEB_TPS_DeviceConfig_t cfg2 = make_config();
    cfg2.i2c_addr = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_VS);
    cfg2.name = "DEV1";
    FEB_TPS_Handle_t h2 = NULL;
    FEB_TPS_DeviceRegister(&cfg2, &h2);

    TEST_ASSERT_EQ(FEB_TPS_DeviceGetCount(), 2, "DeviceGetCount == 2");

    /* --- PollAll --- */
    hal_mock_reset();
    /* 4 reads per device: bus, current, shunt, power */
    push_poll_responses(2000, 0x0050, 100, 10); /* device 0 */
    push_poll_responses(3200, 0x80A0, 200, 20); /* device 1 (negative current) */

    FEB_TPS_Measurement_t measurements[2];
    uint8_t count = FEB_TPS_PollAll(measurements, 2);
    TEST_ASSERT_EQ(count, 2, "PollAll returns 2 (all devices polled)");
    TEST_ASSERT_EQ(measurements[0].bus_voltage_raw, 2000, "PollAll dev0 bus_v_raw");
    TEST_ASSERT_EQ(measurements[0].current_raw, 80,  "PollAll dev0 current=80");
    TEST_ASSERT_EQ(measurements[1].bus_voltage_raw, 3200, "PollAll dev1 bus_v_raw");
    TEST_ASSERT_EQ(measurements[1].current_raw, -160, "PollAll dev1 current=-160");

    /* PollAll with count smaller than device_count */
    hal_mock_reset();
    push_poll_responses(1000, 0x0010, 50, 5);
    count = FEB_TPS_PollAll(measurements, 1);
    TEST_ASSERT_EQ(count, 1, "PollAll with count=1 returns 1");

    /* PollAll NULL array */
    count = FEB_TPS_PollAll(NULL, 2);
    TEST_ASSERT_EQ(count, 0, "PollAll(NULL) returns 0");

    /* --- PollAllRaw --- */
    hal_mock_reset();
    /* PollAllRaw reads 3 regs per device in the mutex-protected batch */
    /* Device 0 */
    hal_mock_i2c_read_push(HAL_OK, 0x0F, 0xA0); /* bus: 0x0FA0 = 4000 */
    hal_mock_i2c_read_push(HAL_OK, 0x00, 0x64); /* current: 100 */
    hal_mock_i2c_read_push(HAL_OK, 0x00, 0xC8); /* shunt: 200 */
    /* Device 1 */
    hal_mock_i2c_read_push(HAL_OK, 0x0C, 0x80); /* bus: 0x0C80 = 3200 */
    hal_mock_i2c_read_push(HAL_OK, 0x80, 0xA0); /* current: -160 */
    hal_mock_i2c_read_push(HAL_OK, 0x00, 0x50); /* shunt: 80 */

    uint16_t bv[2] = {0};
    uint16_t cur[2] = {0};
    uint16_t sv[2] = {0};
    count = FEB_TPS_PollAllRaw(bv, cur, sv, 2);
    TEST_ASSERT_EQ(count, 2, "PollAllRaw returns 2");
    TEST_ASSERT_EQ(bv[0], 4000,   "PollAllRaw dev0 bv == 4000");
    TEST_ASSERT_EQ(cur[0], 100,   "PollAllRaw dev0 cur == 100");
    TEST_ASSERT_EQ(sv[0], 200,    "PollAllRaw dev0 sv == 200");
    TEST_ASSERT_EQ(bv[1], 3200,   "PollAllRaw dev1 bv == 3200");
    TEST_ASSERT_EQ(cur[1], 0x80A0,"PollAllRaw dev1 cur == 0x80A0 (raw)");
    TEST_ASSERT_EQ(sv[1], 80,     "PollAllRaw dev1 sv == 80");

    /* PollAllRaw when not initialized */
    FEB_TPS_DeInit();
    hal_mock_reset();
    count = FEB_TPS_PollAllRaw(bv, cur, sv, 2);
    TEST_ASSERT_EQ(count, 0, "PollAllRaw when not init returns 0");

    /* --- PollAllScaled --- */
    FEB_TPS_Init(NULL);
    hal_mock_reset();
    FEB_TPS_DeviceRegister(&cfg1, &h1);
    FEB_TPS_DeviceRegister(&cfg2, &h2);

    push_poll_responses(4000, 0x0064, 200, 16);
    push_poll_responses(2000, 0x0032, 100,  8);

    FEB_TPS_MeasurementScaled_t scaled[2];
    count = FEB_TPS_PollAllScaled(scaled, 2);
    TEST_ASSERT_EQ(count, 2, "PollAllScaled returns 2");
    TEST_ASSERT_EQ(scaled[0].bus_voltage_mv, 5000, "PollAllScaled dev0 bus 5000mV");
    TEST_ASSERT_EQ(scaled[1].bus_voltage_mv, 2500, "PollAllScaled dev1 bus 2500mV");

    /* PollAllScaled NULL */
    count = FEB_TPS_PollAllScaled(NULL, 2);
    TEST_ASSERT_EQ(count, 0, "PollAllScaled(NULL) returns 0");

    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 8: GPIO Control
 * ============================================================================ */

static void test_gpio_control(void)
{
    test_section("GPIO Control");

    FEB_TPS_Init(NULL);
    hal_mock_reset();

    /* Device without GPIO */
    FEB_TPS_DeviceConfig_t cfg = make_config();
    /* No GPIO ports set */
    FEB_TPS_Handle_t h_no_gpio = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h_no_gpio);

    FEB_TPS_Status_t s;

    /* Enable without port configured → ERR_INVALID_ARG */
    s = FEB_TPS_Enable(h_no_gpio, true);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG,
                   "Enable without EN port = ERR_INVALID_ARG");

    /* ReadPowerGood without port → ERR_INVALID_ARG */
    bool pg = false;
    s = FEB_TPS_ReadPowerGood(h_no_gpio, &pg);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG,
                   "ReadPowerGood without PG port = ERR_INVALID_ARG");

    /* ReadAlert without port → ERR_INVALID_ARG */
    bool alert = false;
    s = FEB_TPS_ReadAlert(h_no_gpio, &alert);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG,
                   "ReadAlert without alert port = ERR_INVALID_ARG");

    /* NULL handle guards */
    s = FEB_TPS_Enable(NULL, true);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Enable(NULL) = ERR_INVALID_ARG");
    s = FEB_TPS_ReadPowerGood(NULL, &pg);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "ReadPowerGood(NULL) = ERR_INVALID_ARG");
    s = FEB_TPS_ReadAlert(NULL, &alert);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "ReadAlert(NULL) = ERR_INVALID_ARG");

    /* NULL output pointer */
    s = FEB_TPS_ReadPowerGood(h_no_gpio, NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "ReadPowerGood(NULL out) = ERR_INVALID_ARG");
    s = FEB_TPS_ReadAlert(h_no_gpio, NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "ReadAlert(NULL out) = ERR_INVALID_ARG");

    /* Device with GPIO configured */
    FEB_TPS_DeInit();
    hal_mock_reset();
    FEB_TPS_Init(NULL);

    cfg = make_config();
    cfg.en_gpio_port    = &g_en_port;
    cfg.en_gpio_pin     = 0x0001;
    cfg.pg_gpio_port    = &g_pg_port;
    cfg.pg_gpio_pin     = 0x0002;
    cfg.alert_gpio_port = &g_alert_port;
    cfg.alert_gpio_pin  = 0x0004;
    FEB_TPS_Handle_t h_gpio = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h_gpio);

    /* Enable */
    int write_before = hal_mock_gpio_write_count();
    s = FEB_TPS_Enable(h_gpio, true);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Enable(true) = OK");
    TEST_ASSERT_EQ(hal_mock_gpio_write_count(), write_before + 1,
                   "Enable calls HAL_GPIO_WritePin once");
    TEST_ASSERT_EQ(hal_mock_gpio_get_written(&g_en_port, 0x0001), GPIO_PIN_SET,
                   "Enable(true) writes GPIO_PIN_SET");

    s = FEB_TPS_Enable(h_gpio, false);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Enable(false) = OK");
    TEST_ASSERT_EQ(hal_mock_gpio_get_written(&g_en_port, 0x0001), GPIO_PIN_RESET,
                   "Enable(false) writes GPIO_PIN_RESET");

    /* ReadPowerGood */
    hal_mock_gpio_set_pin(&g_pg_port, 0x0002, GPIO_PIN_SET);
    s = FEB_TPS_ReadPowerGood(h_gpio, &pg);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "ReadPowerGood = OK");
    TEST_ASSERT_EQ(pg, true, "ReadPowerGood returns true when pin is SET");

    hal_mock_gpio_set_pin(&g_pg_port, 0x0002, GPIO_PIN_RESET);
    s = FEB_TPS_ReadPowerGood(h_gpio, &pg);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "ReadPowerGood = OK");
    TEST_ASSERT_EQ(pg, false, "ReadPowerGood returns false when pin is RESET");

    /* ReadAlert (active low: RESET = alert active) */
    hal_mock_gpio_set_pin(&g_alert_port, 0x0004, GPIO_PIN_RESET);
    s = FEB_TPS_ReadAlert(h_gpio, &alert);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "ReadAlert = OK");
    TEST_ASSERT_EQ(alert, true, "ReadAlert true when pin is RESET (active low)");

    hal_mock_gpio_set_pin(&g_alert_port, 0x0004, GPIO_PIN_SET);
    s = FEB_TPS_ReadAlert(h_gpio, &alert);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "ReadAlert = OK");
    TEST_ASSERT_EQ(alert, false, "ReadAlert false when pin is SET");

    /* EnableAll (only enables devices that have EN port) */
    hal_mock_reset();
    /* h_no_gpio (no EN) + h_gpio (has EN) */
    /* h_no_gpio is unregistered from previous DeInit; re-register */
    /* Currently only h_gpio is registered */
    uint8_t en_count = FEB_TPS_EnableAll(true);
    TEST_ASSERT_EQ(en_count, 1, "EnableAll returns 1 (only 1 device has EN)");
    TEST_ASSERT_EQ(hal_mock_gpio_get_written(&g_en_port, 0x0001), GPIO_PIN_SET,
                   "EnableAll(true) sets EN pin");

    /* ReadAllPowerGood */
    hal_mock_gpio_set_pin(&g_pg_port, 0x0002, GPIO_PIN_SET);
    bool pg_states[2] = {false, false};
    uint8_t pg_count = FEB_TPS_ReadAllPowerGood(pg_states, 1);
    TEST_ASSERT_EQ(pg_count, 1, "ReadAllPowerGood returns 1");
    TEST_ASSERT_EQ(pg_states[0], true, "ReadAllPowerGood pg_states[0] = true");

    /* NULL states */
    pg_count = FEB_TPS_ReadAllPowerGood(NULL, 1);
    TEST_ASSERT_EQ(pg_count, 0, "ReadAllPowerGood(NULL) = 0");

    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 9: Reconfigure and ReadID
 * ============================================================================ */

static void test_reconfigure_and_id(void)
{
    test_section("Reconfigure and ReadID");

    FEB_TPS_Init(NULL);
    hal_mock_reset();
    FEB_TPS_DeviceConfig_t cfg = make_config();
    FEB_TPS_Handle_t h = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h);

    /* --- Reconfigure --- */
    float new_lsb = 10.0f / 32768.0f;
    uint16_t new_cal = (uint16_t)(0.00512f / (new_lsb * 0.001f));
    hal_mock_reset();

    FEB_TPS_Status_t s = FEB_TPS_Reconfigure(h, 0.001f, 10.0f);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Reconfigure returns OK");
    TEST_ASSERT_FLOAT_NEAR(FEB_TPS_GetCurrentLSB(h), new_lsb, 1e-9f,
                           "Reconfigure updates current_lsb");
    TEST_ASSERT_EQ(FEB_TPS_GetCalibration(h), new_cal, "Reconfigure updates cal_reg");

    /* Verify CAL written to device */
    bool found_cal_write = false;
    for (int i = 0; i < hal_mock_i2c_write_count(); i++) {
        I2C_WriteRecord_t wr = hal_mock_i2c_write_get(i);
        if (wr.mem_addr == FEB_TPS_REG_CAL) {
            found_cal_write = true;
            TEST_ASSERT_EQ(wr.value, new_cal, "Reconfigure writes correct CAL value");
        }
    }
    TEST_ASSERT(found_cal_write, "Reconfigure writes to CAL register");

    /* Invalid args */
    s = FEB_TPS_Reconfigure(NULL, 0.002f, 5.0f);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Reconfigure(NULL handle)");
    s = FEB_TPS_Reconfigure(h, 0.0f, 5.0f);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Reconfigure(r_shunt=0)");
    s = FEB_TPS_Reconfigure(h, 0.002f, 0.0f);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Reconfigure(i_max=0)");
    s = FEB_TPS_Reconfigure(h, -0.002f, 5.0f);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "Reconfigure(r_shunt<0)");

    /* Reconfigure I2C failure */
    hal_mock_reset();
    hal_mock_i2c_write_push(HAL_ERROR);
    s = FEB_TPS_Reconfigure(h, 0.002f, 5.0f);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_I2C, "Reconfigure returns ERR_I2C on I2C fail");

    /* --- ReadID --- */
    hal_mock_reset();
    hal_mock_i2c_read_push(HAL_OK, 0x22, 0x60); /* 0x2260 */
    uint16_t id = 0;
    s = FEB_TPS_ReadID(h, &id);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "ReadID returns OK");
    TEST_ASSERT_EQ(id, 0x2260, "ReadID value == 0x2260");

    s = FEB_TPS_ReadID(NULL, &id);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "ReadID(NULL handle)");
    s = FEB_TPS_ReadID(h, NULL);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_INVALID_ARG, "ReadID(NULL out)");

    hal_mock_reset();
    hal_mock_i2c_read_push(HAL_ERROR, 0, 0);
    s = FEB_TPS_ReadID(h, &id);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_I2C, "ReadID ERR_I2C on failure");

    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 10: Utility Functions
 * ============================================================================ */

static void test_utility_functions(void)
{
    test_section("Utility Functions");

    /* --- StatusToString --- */
    TEST_ASSERT(strcmp(FEB_TPS_StatusToString(FEB_TPS_OK), "OK") == 0,
                "StatusToString(OK)");
    TEST_ASSERT(strcmp(FEB_TPS_StatusToString(FEB_TPS_ERR_INVALID_ARG),
                       "Invalid argument") == 0,
                "StatusToString(ERR_INVALID_ARG)");
    TEST_ASSERT(strcmp(FEB_TPS_StatusToString(FEB_TPS_ERR_I2C), "I2C error") == 0,
                "StatusToString(ERR_I2C)");
    TEST_ASSERT(strcmp(FEB_TPS_StatusToString(FEB_TPS_ERR_NOT_INIT),
                       "Not initialized") == 0,
                "StatusToString(ERR_NOT_INIT)");
    TEST_ASSERT(strcmp(FEB_TPS_StatusToString(FEB_TPS_ERR_CONFIG_MISMATCH),
                       "Config mismatch") == 0,
                "StatusToString(ERR_CONFIG_MISMATCH)");
    TEST_ASSERT(strcmp(FEB_TPS_StatusToString(FEB_TPS_ERR_MAX_DEVICES),
                       "Max devices exceeded") == 0,
                "StatusToString(ERR_MAX_DEVICES)");
    TEST_ASSERT(strcmp(FEB_TPS_StatusToString(FEB_TPS_ERR_TIMEOUT), "Timeout") == 0,
                "StatusToString(ERR_TIMEOUT)");
    /* Unknown code */
    const char *unknown = FEB_TPS_StatusToString((FEB_TPS_Status_t)99);
    TEST_ASSERT(unknown != NULL, "StatusToString(unknown) returns non-NULL");

    /* --- GetDeviceName --- */
    FEB_TPS_Init(NULL);
    hal_mock_reset();

    TEST_ASSERT(strcmp(FEB_TPS_GetDeviceName(NULL), "Unknown") == 0,
                "GetDeviceName(NULL) == 'Unknown'");

    FEB_TPS_DeviceConfig_t cfg = make_config();
    cfg.name = "LV";
    FEB_TPS_Handle_t h = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT(strcmp(FEB_TPS_GetDeviceName(h), "LV") == 0,
                "GetDeviceName returns configured name");

    /* Device with NULL name */
    FEB_TPS_DeInit();
    hal_mock_reset();
    FEB_TPS_Init(NULL);
    cfg = make_config();
    cfg.name = NULL;
    h = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h);
    TEST_ASSERT(strcmp(FEB_TPS_GetDeviceName(h), "Unknown") == 0,
                "GetDeviceName(NULL name) == 'Unknown'");

    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 11: Error and Boundary Cases
 * ============================================================================ */

static void test_error_boundary(void)
{
    test_section("Error and Boundary Cases");

    /* --- Max devices limit --- */
    FEB_TPS_DeInit();
    hal_mock_reset();
    FEB_TPS_Init(NULL);

    FEB_TPS_Handle_t handles[FEB_TPS_MAX_DEVICES + 1];
    uint8_t addr = 0x40;

    for (int i = 0; i < FEB_TPS_MAX_DEVICES; i++) {
        FEB_TPS_DeviceConfig_t cfg = make_config();
        cfg.i2c_addr = (uint8_t)(addr + i);
        FEB_TPS_Status_t s = FEB_TPS_DeviceRegister(&cfg, &handles[i]);
        TEST_ASSERT_EQ(s, FEB_TPS_OK, "Register device up to MAX_DEVICES");
    }

    TEST_ASSERT_EQ(FEB_TPS_DeviceGetCount(), FEB_TPS_MAX_DEVICES,
                   "DeviceGetCount == MAX_DEVICES");

    /* One more registration should fail */
    FEB_TPS_DeviceConfig_t overflow_cfg = make_config();
    overflow_cfg.i2c_addr = 0x4F;
    FEB_TPS_Handle_t overflow_h = NULL;
    FEB_TPS_Status_t s = FEB_TPS_DeviceRegister(&overflow_cfg, &overflow_h);
    TEST_ASSERT_EQ(s, FEB_TPS_ERR_MAX_DEVICES,
                   "Register beyond MAX_DEVICES returns ERR_MAX_DEVICES");
    TEST_ASSERT_NULL(overflow_h, "Handle stays NULL when max exceeded");

    FEB_TPS_DeInit();

    /* --- Poll with zero bus voltage (edge case) --- */
    hal_mock_reset();
    FEB_TPS_Init(NULL);
    FEB_TPS_DeviceConfig_t cfg = make_config();
    FEB_TPS_Handle_t h = NULL;
    FEB_TPS_DeviceRegister(&cfg, &h);

    push_poll_responses(0, 0, 0, 0);
    FEB_TPS_Measurement_t meas;
    s = FEB_TPS_Poll(h, &meas);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Poll with zero readings succeeds");
    TEST_ASSERT_EQ(meas.bus_voltage_raw, 0, "Zero bus voltage raw == 0");
    TEST_ASSERT_FLOAT_NEAR(meas.bus_voltage_v, 0.0f, 1e-6f, "Zero bus voltage = 0.0V");
    TEST_ASSERT_EQ(meas.current_raw, 0, "Zero current raw == 0");
    TEST_ASSERT_FLOAT_NEAR(meas.current_a, 0.0f, 1e-6f, "Zero current = 0.0A");

    /* --- Poll with maximum raw values --- */
    hal_mock_reset();
    push_poll_responses(0xFFFF, 0x7FFF, 0x7FFF, 0xFFFF);
    s = FEB_TPS_Poll(h, &meas);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Poll with max values succeeds");
    TEST_ASSERT_EQ(meas.bus_voltage_raw, 0xFFFF, "Max bus voltage raw");
    TEST_ASSERT_EQ(meas.current_raw, 32767, "Max positive current raw");
    TEST_ASSERT(meas.current_a > 0.0f, "Max current is positive");

    /* --- Poll with mid-scale negative values --- */
    hal_mock_reset();
    push_poll_responses(0x1000, 0x8001, 0x8001, 0x0100);
    s = FEB_TPS_Poll(h, &meas);
    TEST_ASSERT_EQ(s, FEB_TPS_OK, "Poll with min negative values succeeds");
    TEST_ASSERT_EQ(meas.current_raw, -1, "Minimum negative current = -1");
    TEST_ASSERT(meas.current_a < 0.0f, "Minimum negative current_a < 0");

    /* --- Calibration boundary: verify formula precision --- */
    /* With 2mOhm shunt and 5A max:
     * LSB = 5/32768 ≈ 1.526e-4 A
     * CAL = 0.00512 / (1.526e-4 * 0.002) = 0.00512 / 3.052e-7 ≈ 16777 */
    float lsb = 5.0f / 32768.0f;
    float r = 0.002f;
    uint16_t expected_cal = (uint16_t)(0.00512f / (lsb * r));
    TEST_ASSERT_EQ(FEB_TPS_GetCalibration(h), expected_cal,
                   "CAL register boundary value correct");

    /* --- PollAll with count=0 --- */
    hal_mock_reset();
    FEB_TPS_Measurement_t dummy[1];
    uint8_t n = FEB_TPS_PollAll(dummy, 0);
    TEST_ASSERT_EQ(n, 0, "PollAll with count=0 returns 0");

    /* --- Sign-magnitude boundary: 0x8000 is negative zero --- */
    /* This is a regression test for the sign-magnitude conversion */
    int16_t neg_zero = FEB_TPS_SignMagnitude(0x8000);
    TEST_ASSERT_EQ(neg_zero, 0, "SignMagnitude(0x8000) = 0 (negative zero = 0)");

    /* --- LVPDB address assignments match expected values from diff --- */
    /* LV_ADDR = FEB_TPS_ADDR(SDA, SCL) */
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SCL), 0x4B,
                   "LVPDB LV_ADDR = FEB_TPS_ADDR(SDA,SCL) = 0x4B");
    /* SH_ADDR = FEB_TPS_ADDR(SDA, SDA) */
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_SDA, FEB_TPS_PIN_SDA), 0x4A,
                   "LVPDB SH_ADDR = FEB_TPS_ADDR(SDA,SDA) = 0x4A");
    /* SM_ADDR = FEB_TPS_ADDR(GND, SDA) */
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_SDA), 0x42,
                   "LVPDB SM_ADDR = FEB_TPS_ADDR(GND,SDA) = 0x42");
    /* AF1_AF2_ADDR = FEB_TPS_ADDR(GND, VS) */
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_VS), 0x41,
                   "LVPDB AF1_AF2_ADDR = FEB_TPS_ADDR(GND,VS) = 0x41");
    /* CP_RF_ADDR = FEB_TPS_ADDR(VS, SCL) */
    TEST_ASSERT_EQ(FEB_TPS_ADDR(FEB_TPS_PIN_VS, FEB_TPS_PIN_SCL), 0x47,
                   "LVPDB CP_RF_ADDR = FEB_TPS_ADDR(VS,SCL) = 0x47");

    FEB_TPS_DeInit();
}

/* ============================================================================
 * Section 12: Conversion Factor Regression
 * ============================================================================ */

static void test_conversion_regression(void)
{
    test_section("Conversion Factor Regression");

    /*
     * Verify that the new library conversion constants produce the same
     * results as the old TPS2482.h macros that were removed in this PR:
     *
     *   Old: TPS2482_CONV_VSHUNT = 0.0025 (mV/LSB)
     *   New: FEB_TPS_CONV_VSHUNT_MV_PER_LSB = 0.0025f
     *
     *   Old: TPS2482_CONV_VBUS = 0.00125 (V/LSB)
     *   New: FEB_TPS_CONV_VBUS_V_PER_LSB = 0.00125f
     *
     *   Old: TPS2482_CURRENT_LSB_EQ(a) = (double)((double)(a) / (1.0 * 0x7FFF))
     *   New: FEB_TPS_CALC_CURRENT_LSB(i_max) = (float)(i_max) / 32768.0f
     *   NOTE: 0x7FFF = 32767, new formula uses 32768 (power of 2, TI recommended)
     *
     *   Old: TPS2482_CAL_EQ(a1, a0) = (uint16_t)(0.00512 / ((1.0*a1)*(1.0*a0)))
     *   New: FEB_TPS_CALC_CAL(lsb, r) = (uint16_t)(0.00512f / ((lsb)*(r)))
     */

    /* Vshunt: spot-check 2500 LSBs → 6.25 mV */
    float vshunt_mv = 2500.0f * FEB_TPS_CONV_VSHUNT_MV_PER_LSB;
    TEST_ASSERT_FLOAT_NEAR(vshunt_mv, 6.25f, 1e-4f,
                           "2500 LSB * 0.0025 mV/LSB = 6.25 mV");

    /* Vbus: spot-check 4000 LSBs → 5.0 V */
    float vbus_v = 4000.0f * FEB_TPS_CONV_VBUS_V_PER_LSB;
    TEST_ASSERT_FLOAT_NEAR(vbus_v, 5.0f, 1e-4f,
                           "4000 LSB * 0.00125 V/LSB = 5.0 V");

    /* Scaled conversion: 5.0V → 5000mV */
    uint16_t vbus_mv = (uint16_t)(vbus_v * 1000.0f);
    TEST_ASSERT_EQ(vbus_mv, 5000, "5.0V * 1000 = 5000 mV");

    /* Current LSB with BMS config (5A, 2mOhm) */
    float bms_lsb = FEB_TPS_CALC_CURRENT_LSB(5.0f);
    TEST_ASSERT(bms_lsb > 0.0f, "BMS current LSB > 0");
    /* At 100 LSB raw → 100 * lsb A */
    float bms_current = 100.0f * bms_lsb;
    TEST_ASSERT(bms_current > 0.0f && bms_current < 0.2f,
                "BMS current 100 LSB is in reasonable range");

    /* Power LSB */
    float power_lsb = FEB_TPS_CALC_POWER_LSB(bms_lsb);
    TEST_ASSERT_FLOAT_NEAR(power_lsb, bms_lsb * 25.0f, 1e-9f,
                           "Power LSB = current_lsb * 25");

    /* FEB_TPS_SignMagnitude matches SIGN_MAGNITUDE macro from removed code:
     * Old: #define SIGN_MAGNITUDE(n) (int16_t)((((n >> 15) & 0x01) == 1) ? -(n & 0x7FFF) : (n & 0x7FFF))
     * New: FEB_TPS_SignMagnitude(raw) */
    uint16_t test_values[] = {0x0000, 0x0001, 0x7FFF, 0x8000, 0x8001, 0xFFFF, 0x1234};
    int test_count = sizeof(test_values) / sizeof(test_values[0]);
    for (int i = 0; i < test_count; i++) {
        uint16_t v = test_values[i];
        int16_t old_result = (int16_t)((((v >> 15) & 0x01) == 1) ? -(v & 0x7FFF) : (v & 0x7FFF));
        int16_t new_result = FEB_TPS_SignMagnitude(v);
        TEST_ASSERT_EQ(new_result, old_result,
                       "FEB_TPS_SignMagnitude matches old SIGN_MAGNITUDE");
    }
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(void)
{
    printf("=== FEB_TPS_Library Unit Tests ===\n");

    test_macros();
    test_lifecycle();
    test_device_register();
    test_device_queries();
    test_measurement();
    test_poll_scaled();
    test_batch_operations();
    test_gpio_control();
    test_reconfigure_and_id();
    test_utility_functions();
    test_error_boundary();
    test_conversion_regression();

    printf("\n=== Results: %d/%d passed (%d failed) ===\n",
           g_tests_passed, g_tests_run, g_tests_failed);

    return (g_tests_failed == 0) ? 0 : 1;
}