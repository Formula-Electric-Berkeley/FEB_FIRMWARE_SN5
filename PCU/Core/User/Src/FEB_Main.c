#include "FEB_Main.h"

void FEB_Main_Setup(void){

    // Start CAN
    FEB_CAN_TX_Init();
    FEB_CAN_RX_Init();

    // Start ADCs
    FEB_ADC_Init();
    FEB_ADC_Start(ADC_MODE_DMA);

    // RMS Setup
	FEB_CAN_RMS_Init();
}


void FEB_Main_While() {
    
    FEB_SM_ST_t bms_state = FEB_CAN_BMS_getState();
    if (bms_state == FEB_SM_ST_DRIVE) {
    	FEB_RMS_Process();
    } else {
        FEB_RMS_Disable();
    }
    // FEB_HECS_update(); // NEED FUNCTION
    FEB_RMS_Torque(); // NEED FUNCTION

	// FEB_Normalized_CAN_sendBrake(); // NEED FUNCTION
	// FEB_CAN_HEARTBEAT_Transmit(); // NEED FUNCTION
	// FEB_CAN_ACC(); // NEED FUNCTION
	// FEB_CAN_TPS_Transmit(); // NEED FUNCTION

	HAL_Delay(10);


}