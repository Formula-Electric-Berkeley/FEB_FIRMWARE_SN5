#include "FEB_CAN_RMS.h"
#include "FEB_CAN_IDs.h"

/* Global RMS message data */
RMS_MESSAGE_TYPE RMS_MESSAGE;

/* RMS parameter broadcast data */
uint8_t PARAM_BROADCAST_DATA[2] = {0b10100000, 0b00010101};

void FEB_CAN_RMS_Init(void) {
	FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_VOLTAGE, FEB_CAN_ID_STD, FEB_CAN_RMS_Callback);
	FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_MOTOR, FEB_CAN_ID_STD, FEB_CAN_RMS_Callback);

	RMS_MESSAGE.HV_Bus_Voltage = 0;
	RMS_MESSAGE.Motor_Speed = 0;

	for(int i = 0; i < 10; i++){
		FEB_CAN_RMS_Transmit_ParamSafety();
		HAL_Delay(10);
	}

	for(int i = 0; i < 10; i++){
		FEB_CAN_RMS_Transmit_Disable_Undervolt();
		HAL_Delay(10);
	}


	// send disable command to remove lockout
	for (int i = 0; i < 10; i++) {
		FEB_CAN_RMS_Transmit_CommDisable();
		HAL_Delay(10);
	}

	// Select CAN msg to broadcast
	FEB_CAN_RMS_Transmit_ParamBroadcast();
}

void FEB_CAN_RMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, uint8_t *data, uint8_t length) {
	if (can_id == FEB_CAN_ID_RMS_VOLTAGE) {
		int16_t temp_voltage;
		memcpy(&temp_voltage, data, 2);
		RMS_MESSAGE.HV_Bus_Voltage = temp_voltage;
	} else if (can_id == FEB_CAN_ID_RMS_MOTOR) {
		int16_t temp_speed;
		memcpy(&temp_speed, data+2, 2);
		RMS_MESSAGE.Motor_Speed = temp_speed;
	}
}

/**
 * @brief Transmit torque command to RMS motor controller
 *
 * @param torque Commanded torque in tenths of Nm (e.g., 2300 = 230.0 Nm)
 *               Valid range: -32768 to +32767 (int16_t limits)
 * @param enabled Enable flag: 1 = inverter enabled, 0 = disabled
 */
void FEB_CAN_RMS_Transmit_UpdateTorque(int16_t torque, uint8_t enabled) {

	// Bounds checking: Limit torque to motor capabilities
	// Negative torque (regen) is allowed within motor limits
	#define MAX_REGEN_TORQUE -3000  // Max regen: -300.0 Nm
	#define MAX_MOTOR_TORQUE  3000  // Max motor: +300.0 Nm

	if (torque > MAX_MOTOR_TORQUE) {
		torque = MAX_MOTOR_TORQUE;
	} else if (torque < MAX_REGEN_TORQUE) {
		torque = MAX_REGEN_TORQUE;
	}

	uint8_t data[8];
	data[0] = (uint8_t) (torque & 0xFF);
	data[1] = (uint8_t) ((torque >> 8) & 0xFF);
	data[2] = 0;
	data[3] = 0;
	data[4] = 1;  // Direction: 1 = forward, 0 = reverse
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