#include "FEB_Main.h"

void FEB_Main_Setup(void) {

    // Initialize UART for debug printf
    FEB_Printf_Init(&huart2);
    printf("\r\n=== FEB PCU Starting ===\r\n");
    printf("UART Debug initialized at 115200 baud\r\n");

    // Start CAN
    FEB_CAN_TX_Init();
    FEB_CAN_RX_Init();
    printf("CAN initialized\r\n");

    // Start ADCs
    FEB_ADC_Init();
    FEB_ADC_Start(ADC_MODE_DMA);
    printf("ADC initialized\r\n");

    RMS Setup
	FEB_CAN_RMS_Init();
    printf("RMS initialized\r\n");
    
    printf("=== Setup Complete ===\r\n\r\n");
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
    static uint32_t loop_count = 0;
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
    FEB_RMS_Process();
    // } else {
    //     FEB_RMS_Disable();
    // }

    /* Update torque command based on pedal inputs and safety checks */
    FEB_RMS_Torque();
    FEB_CAN_Diagnostics_TransmitBrakeData(); // Transmit brake position to dash/telemetry
    FEB_CAN_Diagnostics_TransmitAPPSData(); // Transmit accelerator position

    /* TODO: Implement additional CAN transmissions:
     * - FEB_CAN_HEARTBEAT_Transmit()       // Already implemented in BMS callback */

    FEB_CAN_TPS_Transmit();                 // Transmit throttle position sensor data
     /*
     * - FEB_HECS_update()                  // Update HECS (HV Enable Check System)
     */

    /* Debug output every 100 loops (1 second at 100Hz) */
    if (loop_count % 100 == 0) {
        FEB_ADC_GetAPPSData(&apps_data);
        FEB_ADC_GetBrakeData(&brake_data);
        
        printf("--- Status (Loop: %lu) ---\r\n", (unsigned long)loop_count);
        printf("APPS: %.1f%% | Plausible: %s | RTD: %s\r\n", 
               apps_data.acceleration,
               apps_data.plausible ? "YES" : "NO",
               FEB_DASH_Ready_To_Drive() ? "YES" : "NO");
        printf("Brake: %.1f%% | Pressed: %s\r\n\r\n",
               brake_data.brake_position,
               brake_data.brake_pressed ? "YES" : "NO");
    }
    
    loop_count++;

    /* Main loop timing: 10ms cycle (100Hz) */
	HAL_Delay(10);
}