#include "FEB_CAN_RMS.h"

void FEB_CAN_RMS_Init(void) {
	FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_VOLTAGE, FEB_CAN_ID_STD, FEB_CAN_RMS_Callback);
	FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_MOTOR, FEB_CAN_ID_STD, FEB_CAN_RMS_Callback);

	RMS_MESSAGE.HV_Bus_Voltage = 0;
	RMS_MESSAGE.Motor_Speed = 0;
}

void FEB_CAN_RMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data, uint8_t length) {
	if (can_id == FEB_CAN_ID_RMS_VOLTAGE) {
		memcpy(&(RMS_MESSAGE.HV_Bus_Voltage), data, 2);
	} else if (can_id == FEB_CAN_ID_RMS_MOTOR) {
		memcpy(&(RMS_MESSAGE.Motor_Speed), RxData+2, 2);
	}
}