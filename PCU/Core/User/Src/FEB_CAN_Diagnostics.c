#include "FEB_CAN_Diagnostics.h"

Brake_DataTypeDef Brake_Data;

void FEB_CAN_Diagnostics_TransmitBrakeData(void) {
    uint8_t data[8] = {0};
    
    // Get latest brake data
    FEB_ADC_GetBrakeData(&Brake_Data);
    
    // Pack brake position (0-100%) into first two bytes (0-10000 centi-percent)
    uint16_t brake_position_centi_percent = (uint16_t)(Brake_Data.brake_position * 100.0f);
    data[0] = (brake_position_centi_percent >> 8) & 0xFF;
    data[1] = brake_position_centi_percent & 0xFF;
    
    // Pack brake pressure sensors (0-100%) into next four bytes (0-10000 centi-percent)
    uint16_t pressure1_centi_percent = (uint16_t)(Brake_Data.pressure1_percent * 100.0f);
    uint16_t pressure2_centi_percent = (uint16_t)(Brake_Data.pressure2_percent * 100.0f);
    data[2] = (pressure1_centi_percent >> 8) & 0xFF;
    data[3] = pressure1_centi_percent & 0xFF;
    data[4] = (pressure2_centi_percent >> 8) & 0xFF;
    data[5] = pressure2_centi_percent & 0xFF;
    
    // Pack status flags into last two bytes
    data[6] = (Brake_Data.plausible ? 0x01 : 0x00) |  // Bit 0
          (Brake_Data.brake_pressed ? 0x02 : 0x00) |  // Bit 1
          (Brake_Data.bots_active ? 0x04 : 0x00);     // Bit 2    
    data[7] = Brake_Data.brake_switch ? 0x02 : 0x01;
    
    // Transmit CAN message
    FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_BRAKE_DATA, 
                                data, 8);
}
void FEB_CAN_Diagnostics_TransmitAPPSData(void) {
    uint8_t data[8] = {0};
    APPS_DataTypeDef apps_data;
    
    // Get latest APPS data
    FEB_ADC_GetAPPSData(&apps_data);
    
    // Pack APPS1 position (0-100%) into first two bytes (0-10000 centi-percent)
    uint16_t apps1_centi_percent = (uint16_t)(apps_data.position1 * 100.0f);
    data[0] = (apps1_centi_percent >> 8) & 0xFF;
    data[1] = apps1_centi_percent & 0xFF;
    
    // Pack APPS2 position (0-100%) into next two bytes (0-10000 centi-percent)
    uint16_t apps2_centi_percent = (uint16_t)(apps_data.position2 * 100.0f);
    data[2] = (apps2_centi_percent >> 8) & 0xFF;
    data[3] = apps2_centi_percent & 0xFF;
    
    // Pack average acceleration (0-100%) into next two bytes (0-10000 centi-percent)
    uint16_t accel_centi_percent = (uint16_t)(apps_data.acceleration * 100.0f);
    data[4] = (accel_centi_percent >> 8) & 0xFF;
    data[5] = accel_centi_percent & 0xFF;
    
    // Pack status flags into last two bytes
    data[6] = (apps_data.plausible ? 0x01 : 0x00) |      // Bit 0
              (apps_data.short_circuit ? 0x02 : 0x00) |  // Bit 1
              (apps_data.open_circuit ? 0x04 : 0x00);    // Bit 2    
    data[7] = 0;
    
    // Transmit CAN message
    FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_APPS_DATA, 
                                data, 8);
}