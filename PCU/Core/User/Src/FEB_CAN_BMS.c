#include "FEB_CAN_BMS.h"

uint16_t FEB_CAN_BMS_getTemp(){
	return BMS_MESSAGE.temperature;
}

uint16_t FEB_CAN_BMS_getVoltage(){
	return BMS_MESSAGE.voltage;
}

uint8_t FEB_CAN_BMS_getDeviceSelect() {
	return BMS_MESSAGE.ping_ack;
}

FEB_SM_ST_t FEB_CAN_BMS_getState(){
	return BMS_MESSAGE.state;
}

void FEB_CAN_BMS_Init(void) {
	FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_BMS_ACCUMULATOR_TEMPERATURE, FEB_CAN_ID_STD, FEB_CAN_BMS_Callback);
	FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_BMS_STATE, FEB_CAN_ID_STD, FEB_CAN_BMS_Callback);
	FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_BMS_ACCUMULATOR_VOLTAGE, FEB_CAN_ID_STD, FEB_CAN_BMS_Callback);

	BMS_MESSAGE.temperature = 0;
	BMS_MESSAGE.voltage = 0;
}

void FEB_CAN_BMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data, uint8_t length) {
	if (can_id == FEB_CAN_ID_BMS_ACCUMULATOR_TEMPERATURE) {

		BMS_MESSAGE.temperature = data[2] << 8 | data[3];

	} else if (can_id == FEB_CAN_ID_BMS_STATE) {

		BMS_MESSAGE.state = data[0] & 0x1F;
        BMS_MESSAGE.ping_ack = (data[0] & 0xE0) >> 5;

        if (BMS_MESSAGE.state == FEB_SM_ST_HEALTH_CHECK || BMS_MESSAGE.ping_ack == FEB_HB_PCU ) {
        	FEB_CAN_HEARTBEAT_Transmit();
        }

	} else if (can_id == FEB_CAN_ID_BMS_ACCUMULATOR_VOLTAGE) {
		BMS_MESSAGE.voltage = (data[0] << 8) | (data[1]);
	}
}

// TODO: NEED TO ADD DATA THAT IS BEING TRANSMITTED
void FEB_CAN_HEARTBEAT_Transmit(void) {
	uint8_t data[8];
    data[0] = 1;

    FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_PCU_HEARTBEAT, data, 1);
}