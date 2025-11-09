#include "FEB_CAN_TPS.h"

/* Global TPS message data */
TPS_MESSAGE_TYPE TPS_MESSAGE;

void FEB_CAN_TPS_Init(void) {
    /* Initialize TPS message structure */
    TPS_MESSAGE.bus_voltage_mv = 0;
    TPS_MESSAGE.current_ma = 0;
}

void FEB_CAN_TPS_Update(I2C_HandleTypeDef *hi2c, uint8_t *i2c_addresses, uint8_t num_devices) {
    uint16_t voltage_raw;
    uint16_t current_raw;
    
    /* Poll TPS2482 for voltage and current */
    TPS2482_Poll_Bus_Voltage(hi2c, i2c_addresses, &voltage_raw, num_devices);
    TPS2482_Poll_Current(hi2c, i2c_addresses, &current_raw, num_devices);
    
    /* Convert raw values to physical units */
    TPS_MESSAGE.bus_voltage_mv = FLOAT_TO_UINT16_T(voltage_raw * TPS2482_CONV_VBUS);
    TPS_MESSAGE.current_ma = FLOAT_TO_INT16_T(SIGN_MAGNITUDE(current_raw) * TPS2482_CURRENT_LSB_EQ(5.0));
}

void FEB_CAN_TPS_Transmit(void) {
    uint8_t data[8] = {0};
    
    /* Pack voltage (bytes 0-1) and current (bytes 2-3) */
    memcpy(&data[0], &TPS_MESSAGE.bus_voltage_mv, sizeof(uint16_t));
    memcpy(&data[2], &TPS_MESSAGE.current_ma, sizeof(int16_t));
    
    /* Transmit CAN message */
    FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_TPS_DATA, data, 4);
}