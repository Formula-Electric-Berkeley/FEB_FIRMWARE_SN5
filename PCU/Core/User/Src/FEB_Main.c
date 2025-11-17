#include "FEB_Main.h"
#include "FEB_Debug.h"
#include "TPS2482.h"
#include <stdint.h>

extern UART_HandleTypeDef huart2;
extern I2C_HandleTypeDef hi2c1;

/* ===== TPS2482 I2C CONFIGURATION =====
 *
 * Hardware Setup:
 *   - Number of devices: 1
 *   - I2C Address pins: A0=GND, A1=GND
 *   - Resulting 7-bit address: 0x40 (calculated by TPS2482_I2C_ADDR macro)
 *   - Note: STM32 HAL I2C functions expect 7-bit addresses (they handle the R/W bit internally)
 *
 * Address Calculation:
 *   - TPS2482_I2C_ADDR(A1, A0) macro from TPS2482.h
 *   - Base address: 0b1000000 (0x40)
 *   - A1 and A0 pins can be GND (0x00), VCC (0x01), SDA (0x02), or SCL (0x03)
 *   - Current config: Both pins = GND → 0x40
 */
#define NUM_TPS_DEVICES 1
static uint8_t tps_i2c_address = TPS2482_I2C_ADDR(TPS2482_I2C_ADDR_GND, TPS2482_I2C_ADDR_GND);  /* 7-bit address: 0x40 */

/* TPS2482 Configuration
 * CAL calculation:
 *   - R_shunt = 0.012Ω (12 milliohm)
 *   - I_max = 4A
 *   - Current_LSB = I_max / 2^15 = 4 / 32768 = 0.000122 A/LSB
 *   - CAL = 0.00512 / (Current_LSB × R_shunt) = 0.00512 / (0.000122 × 0.012) = 3495
 */
static TPS2482_Configuration tps_config = {
    .config = TPS2482_CONFIG_DEFAULT,  /* 0x4127 - Continuous shunt+bus voltage, 128 samples avg, 1.1ms conversion */
    .cal = 3495,                        /* Calibration value for 4A max, 12mΩ shunt */
    .mask = 0x0000,                     /* No alerts configured */
    .alert_lim = 0x0000                 /* No alert limit */
};

void FEB_Main_Setup(void) {

    // Initialize UART for debug printf
    FEB_Printf_Init(&huart2);

    LOG_RAW("\r\n");
    LOG_I(TAG_MAIN, "=== FEB PCU Starting ===");
    LOG_I(TAG_MAIN, "UART Debug initialized at 115200 baud");

    // Start CAN
    FEB_CAN_TX_Init();
    FEB_CAN_RX_Init();
    LOG_I(TAG_MAIN, "CAN initialized");

    // Start ADCs
    FEB_ADC_Init();
    FEB_ADC_Start(ADC_MODE_DMA);
    LOG_I(TAG_MAIN, "ADC initialized");

    // Diagnostic: Print APPS calibration values
    HAL_Delay(100);  // Wait for ADC to stabilize
    LOG_I(TAG_MAIN, "=== APPS Calibration Diagnostics ===");
    LOG_I(TAG_MAIN, "APPS1 Cal: %d - %d mV (range: %d mV)",
          APPS1_DEFAULT_MIN_VOLTAGE_MV, APPS1_DEFAULT_MAX_VOLTAGE_MV,
          APPS1_DEFAULT_MAX_VOLTAGE_MV - APPS1_DEFAULT_MIN_VOLTAGE_MV);
    LOG_I(TAG_MAIN, "APPS2 Cal: %d - %d mV (range: %d mV)",
          APPS2_DEFAULT_MIN_VOLTAGE_MV, APPS2_DEFAULT_MAX_VOLTAGE_MV,
          APPS2_DEFAULT_MAX_VOLTAGE_MV - APPS2_DEFAULT_MIN_VOLTAGE_MV);
    LOG_I(TAG_MAIN, "Initial APPS1 read: %d ADC (%.2fV)",
          FEB_ADC_GetAccelPedal1Raw(), FEB_ADC_GetAccelPedal1Voltage());
    LOG_I(TAG_MAIN, "Initial APPS2 read: %d ADC (%.2fV)",
          FEB_ADC_GetAccelPedal2Raw(), FEB_ADC_GetAccelPedal2Voltage());
    LOG_RAW("\r\n");

    // RMS Setup
	FEB_CAN_RMS_Init();
    LOG_I(TAG_MAIN, "RMS initialized");

    // TPS2482 Setup
    uint16_t tps_device_id = 0;
    bool tps_init_success = false;

    /* Initialize TPS2482 hardware (configure CAL, CONFIG, MASK, ALERT_LIM registers) */
    TPS2482_Init(&hi2c1, &tps_i2c_address, &tps_config, &tps_device_id, &tps_init_success, NUM_TPS_DEVICES);

    /* Initialize TPS CAN message structure */
    FEB_CAN_TPS_Init();

    if (tps_init_success) {
        LOG_I(TAG_MAIN, "TPS2482 initialized successfully");
        LOG_I(TAG_MAIN, "  Device ID: 0x%04X", tps_device_id);
        LOG_I(TAG_MAIN, "  CAL value: %d (0x%04X) for 4A max, 12mΩ shunt", tps_config.cal, tps_config.cal);
        LOG_I(TAG_MAIN, "  Config: 0x%04X (continuous measurement mode)", tps_config.config);
    } else {
        LOG_E(TAG_MAIN, "TPS2482 initialization FAILED");
        LOG_E(TAG_MAIN, "  Check: I2C1 pins, pull-ups, TPS2482 power, address (0x%02X)", tps_i2c_address);
    }

    LOG_I(TAG_MAIN, "=== Setup Complete ===");
    LOG_RAW("\r\n");
}


/**
 * @brief Main control loop function - called repeatedly from main()
 *
 * This function implements the PCU's primary control logic including:
 * - BMS state monitoring
 * - RMS motor controller management
 * - Torque command generation
 *
 * Runs at ~100Hz (10ms cycle time) in a delay-based superloop
 */
void FEB_Main_While() {
    APPS_DataTypeDef apps_data;
    Brake_DataTypeDef brake_data;

    /* Check BMS state and enable/disable RMS accordingly */
    // FEB_SM_ST_t bms_state = FEB_CAN_BMS_getState();

    // /* Handle fault states - disable motor if not in drive state */
    // if (bms_state == FEB_SM_ST_FAULT_BMS ||
    //     bms_state == FEB_SM_ST_FAULT_BSPD ||
    //     bms_state == FEB_SM_ST_FAULT_IMD) {
    //     FEB_RMS_Disable();
    // }
    /* Enable motor only in DRIVE state */
    // else if (bms_state == FEB_SM_ST_DRIVE) {
    // FEB_RMS_Process();
    // } else {
    //     FEB_RMS_Disable();
    // }

    /* Update torque command based on pedal inputs and safety checks */
    FEB_RMS_Torque();
    FEB_CAN_Diagnostics_TransmitBrakeData(); // Transmit brake position to dash/telemetry
    FEB_CAN_Diagnostics_TransmitAPPSData(); // Transmit accelerator position

    /* TODO: Implement additional CAN transmissions:
     * - FEB_CAN_HEARTBEAT_Transmit()       // Already implemented in BMS callback */

    // TPS2482 power monitoring - Read voltage/current and transmit over CAN
    FEB_CAN_TPS_Update(&hi2c1, &tps_i2c_address, NUM_TPS_DEVICES);
    FEB_CAN_TPS_Transmit();
     /*
     * - FEB_HECS_update()                  // Update HECS (HV Enable Check System)
     */

    /* Debug output every 100 loops (1 second at 100Hz) */
        FEB_ADC_GetAPPSData(&apps_data);
        FEB_ADC_GetBrakeData(&brake_data);

        // Enhanced debug output with raw ADC values
        LOG_D(TAG_MAIN, "APPS1: %4d ADC (%.2fV / %.1f%%) | APPS2: %4d ADC (%.2fV / %.1f%%) | Avg: %.1f%% | %s",
               FEB_ADC_GetAccelPedal1Raw(),
               FEB_ADC_GetAccelPedal1Voltage(),
               apps_data.position1,
               FEB_ADC_GetAccelPedal2Raw(),
               FEB_ADC_GetAccelPedal2Voltage(),
               apps_data.position2,
               apps_data.acceleration,
               apps_data.plausible ? "PLAUS" : "IMPLAUS");

        LOG_D(TAG_MAIN, "Brake1: %4d ADC (%.2fV / %.1f%%) | Brake2: %4d ADC (%.2fV / %.1f%%) | Avg: %.1f%% | %s",
               FEB_ADC_GetBrakePressure1Raw(),
               FEB_ADC_GetBrakePressure1Voltage(),
               brake_data.pressure1_percent,
               FEB_ADC_GetBrakePressure2Raw(),
               FEB_ADC_GetBrakePressure2Voltage(),
               brake_data.pressure2_percent,
               brake_data.brake_position,
               brake_data.brake_pressed ? "PRESSED" : "RELEASED");
    

    /* Main loop timing: 10ms cycle (100Hz) */
	HAL_Delay(100);
}