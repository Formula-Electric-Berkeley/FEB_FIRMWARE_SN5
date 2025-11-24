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

void FEB_CAN_RMS_Transmit_UpdateTorque(int16_t torque, uint8_t enabled) {
	
	uint8_t data[8];
	data[0] = (uint8_t) torque & 0xFF;
	data[1] = (uint8_t) (torque >> 8) & 0xFF;
	data[2] = 0;
	data[3] = 0;
	data[4] = 1; // What does this do?
	data[5] = enabled;
	data[6] = 0;
	data[7] = 0;
	FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_TORQUE, data, 8);
}

void FEB_CAN_RMS_Transmit_Disable_Undervolt(void) {
	uint8_t data[8];
	data[0] = FAULT_CLEAR_ADDR_UNDERVOLT;
	data[1] = 0;
	data[2] = 1;
	data[3] = 0;
	data[4] = FAULT_CLEAR_DATA;
	data[5] = 0;
	data[6] = 0;
	data[7] = 0;
	FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_PARAM, data, 8);
}

void FEB_CAN_RMS_Transmit_ParamSafety(void) {
	uint8_t data[8];
	data[0] = FAULT_CLEAR_ADDR_PARAM_SAFETY;
	data[1] = 0;
	data[2] = 1;
	data[3] = 0;
	data[4] = FAULT_CLEAR_DATA;
	data[5] = 0;
	data[6] = 0;
	data[7] = 0;
	FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_PARAM, data, 8);
}

void FEB_CAN_RMS_Transmit_ParamBroadcast(void) {
	uint8_t data[8];
	data[0] = PARAM_BROADCAST_ADDR;
	data[1] = 0;
	data[2] = 1;
	data[3] = 0;
	data[4] = PARAM_BROADCAST_DATA[0];
	data[5] = PARAM_BROADCAST_DATA[1];
	data[6] = 0;
	data[7] = 0;
	FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_PARAM, data, 8);
}

void FEB_CAN_RMS_Transmit_CommDisable(void) {
	uint8_t data[8];
	data[0] = 0;
	data[1] = 0;
	data[2] = 0;
	data[3] = 0;
	data[4] = 0;
	data[5] = 0;
	data[6] = 0;
	data[7] = 0;
	FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_PARAM, data, 8);
}