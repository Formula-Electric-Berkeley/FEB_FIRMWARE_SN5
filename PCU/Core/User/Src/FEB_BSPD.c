#include "FEB_BSPD.h"

/* Global BSPD data */
BSPD_TYPE BSPD;

// Reads the status of the BSPD if the BSPD reset is active.
void FEB_BSPD_CheckReset(){
	if (HAL_GPIO_ReadPin(BSPD_RESET_PORT, BSPD_RESET_PIN)){
		BSPD.state = 1; //BSPD reset is active
	} else {
		BSPD.state = 0;
	}
	FEB_BSPD_CAN_Transmit();

}

//Sends the BSPD status over CAN
void FEB_BSPD_CAN_Transmit(){
    uint8_t data[8];
    data[0] = BSPD.state;

    FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_BSPD_STATUS, data, 1);
}