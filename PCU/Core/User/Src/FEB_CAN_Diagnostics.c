#include "FEB_CAN_Diagnostics.h"
#include "feb_log.h"

Brake_DataTypeDef Brake_Data;

void FEB_CAN_Diagnostics_TransmitBrakeData(void)
{
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
  data[6] = (Brake_Data.plausible ? 0x01 : 0x00) |     // Bit 0
            (Brake_Data.brake_pressed ? 0x02 : 0x00) | // Bit 1
            (Brake_Data.bots_active ? 0x04 : 0x00);    // Bit 2
  data[7] = Brake_Data.brake_switch ? 0x02 : 0x01;

  // Transmit CAN message
  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BRAKE_FRAME_ID, FEB_CAN_ID_STD, data, 8);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to transmit brake data: %s", FEB_CAN_StatusToString(status));
  }
}

void FEB_CAN_Diagnostics_TransmitAPPSData(void)
{
  APPS_DataTypeDef apps_data;
  FEB_ADC_GetAPPSData(&apps_data);

  struct feb_can_pcu_raw_acc_t msg = {0};
  msg.acc0 = (uint16_t)(apps_data.position1 * 100.0f);
  msg.acc1 = (uint16_t)(apps_data.position2 * 100.0f);
  msg.accel = (uint16_t)(apps_data.acceleration * 100.0f);
  msg.plausible = apps_data.plausible ? 1u : 0u;
  msg.short_circuit = apps_data.short_circuit ? 1u : 0u;
  msg.open_circuit = apps_data.open_circuit ? 1u : 0u;

  uint8_t data[FEB_CAN_PCU_RAW_ACC_LENGTH];
  int packed = feb_can_pcu_raw_acc_pack(data, &msg, sizeof(data));
  FEB_CAN_Status_t status =
      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_PCU_RAW_ACC_FRAME_ID, FEB_CAN_ID_STD, data, (uint8_t)packed);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to transmit APPS data: %s", FEB_CAN_StatusToString(status));
  }
}
