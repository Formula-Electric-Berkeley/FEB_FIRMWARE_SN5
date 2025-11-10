#include "FEB_Main.h"
#include <stdint.h>
extern UART_HandleTypeDef huart2;

void FEB_UART_Transmit(const char* message) {
    HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
}

void FEB_Main_Setup(void) {

    // Initialize UART for debug printf
    // FEB_Printf_Init(&huart2);

    FEB_UART_Transmit("\r\n=== FEB PCU Starting ===\r\n");
    FEB_UART_Transmit("UART Debug initialized at 115200 baud\r\n");

    // Start CAN
    FEB_CAN_TX_Init();
    FEB_CAN_RX_Init();
    FEB_UART_Transmit("CAN initialized\r\n");

    // Start ADCs
    FEB_ADC_Init();
    FEB_ADC_Start(ADC_MODE_DMA);
    FEB_UART_Transmit("ADC initialized\r\n");

    // RMS Setup
	FEB_CAN_RMS_Init();
    FEB_UART_Transmit("RMS initialized\r\n");
    
    FEB_UART_Transmit("=== Setup Complete ===\r\n\r\n");
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

    FEB_CAN_TPS_Transmit();                 // Transmit throttle position sensor data
     /*
     * - FEB_HECS_update()                  // Update HECS (HV Enable Check System)
     */

    /* Debug output every 100 loops (1 second at 100Hz) */
        FEB_ADC_GetAPPSData(&apps_data);
        FEB_ADC_GetBrakeData(&brake_data);
        
        char APPS_message[1024];

	    sprintf(APPS_message, "APPS: %.1f%% | Plausible: %s | RTD: %s\r\n", 
               apps_data.acceleration,
               apps_data.plausible ? "YES" : "NO",
               FEB_DASH_Ready_To_Drive() ? "YES" : "NO");
        FEB_UART_Transmit(APPS_message);
        
        char Brake_message[1024];
        sprintf(Brake_message, "Brake: %.1f%% | Pressed: %s\r\n\r\n",
               brake_data.brake_position,
               brake_data.brake_pressed ? "YES" : "NO");
        FEB_UART_Transmit(Brake_message);
    

    /* Main loop timing: 10ms cycle (100Hz) */
	HAL_Delay(1000);
}