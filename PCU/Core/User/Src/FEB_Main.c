#include "FEB_Main.h"

void FEB_Main_Setup(void) {

    // Start CAN
    FEB_CAN_TX_Init();
    FEB_CAN_RX_Init();

    // Start ADCs
    FEB_ADC_Init();
    FEB_ADC_Start(ADC_MODE_DMA);

    // RMS Setup
	FEB_CAN_RMS_Init();
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

    /* Check BMS state and enable/disable RMS accordingly */
    FEB_SM_ST_t bms_state = FEB_CAN_BMS_getState();

    /* Handle fault states - disable motor if not in drive state */
    if (bms_state == FEB_SM_ST_FAULT_BMS ||
        bms_state == FEB_SM_ST_FAULT_BSPD ||
        bms_state == FEB_SM_ST_FAULT_IMD) {
        FEB_RMS_Disable();
    }
    /* Enable motor only in DRIVE state */
    else if (bms_state == FEB_SM_ST_DRIVE) {
    	FEB_RMS_Process();
    } else {
        FEB_RMS_Disable();
    }

    /* Update torque command based on pedal inputs and safety checks */
    FEB_RMS_Torque();
    FEB_CAN_Diagnostics_TransmitBrakeData(); // Transmit brake position to dash/telemetry

    /* TODO: Implement additional CAN transmissions:
     * - FEB_CAN_HEARTBEAT_Transmit()      // Already implemented in BMS callback
     * - FEB_CAN_ACC()                     // Transmit accelerator position
     */
    FEB_CAN_TPS_Transmit();            // Transmit throttle position sensor data
     /*
     * - FEB_HECS_update()                 // Update HECS (HV Enable Check System)
     */

    /* Main loop timing: 10ms cycle (100Hz) */
	HAL_Delay(10);
}