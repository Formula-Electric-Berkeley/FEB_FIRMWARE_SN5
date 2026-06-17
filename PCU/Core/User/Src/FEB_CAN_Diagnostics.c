#include "FEB_CAN_Diagnostics.h"
#include "feb_log.h"

Brake_DataTypeDef Brake_Data;

void FEB_CAN_Diagnostics_TransmitBrakeData(void)
{
  // Get latest brake data
  FEB_ADC_GetBrakeData(&Brake_Data);

  // Position + per-sensor pressure are sent as centi-percent (0-10000); status
  // flags ride in bytes 6-7. Layout/endianness are owned by the generated brake
  // definition (common/FEB_CAN_Library_SN4) — pack through it, don't hand-roll.
  struct feb_can_brake_t msg = {0};
  msg.brake_position = (uint16_t)(Brake_Data.brake_position * 100.0f);
  msg.brake1_pct = (uint16_t)(Brake_Data.pressure1_percent * 100.0f);
  msg.brake2_pct = (uint16_t)(Brake_Data.pressure2_percent * 100.0f);
  msg.plausible = Brake_Data.plausible ? 1u : 0u;
  msg.brake_pressed = Brake_Data.brake_pressed ? 1u : 0u;
  msg.bots_active = Brake_Data.bots_active ? 1u : 0u;
  msg.brake_switch = Brake_Data.brake_switch ? 1u : 0u;

  uint8_t data[FEB_CAN_BRAKE_LENGTH];
  int packed = feb_can_brake_pack(data, &msg, sizeof(data));
  FEB_CAN_Status_t status =
      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_BRAKE_FRAME_ID, FEB_CAN_ID_STD, data, (uint8_t)packed);
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

void FEB_CAN_Diagnostics_TransmitPedalVoltages(void)
{
  // Raw sensor-side voltages (post on-board divider) in mV — same domain as the
  // APPS/brake calibration thresholds in FEB_PINOUT.h. The Get*Voltage() getters
  // return volts already corrected for the divider ratio; ×1000 yields mV.
  struct feb_can_pcu_pedal_voltages_t msg = {0};
  msg.acc1_mv = (uint16_t)(FEB_ADC_GetAccelPedal1Voltage() * 1000.0f);
  msg.acc2_mv = (uint16_t)(FEB_ADC_GetAccelPedal2Voltage() * 1000.0f);
  msg.brake1_mv = (uint16_t)(FEB_ADC_GetBrakePressure1Voltage() * 1000.0f);
  msg.brake2_mv = (uint16_t)(FEB_ADC_GetBrakePressure2Voltage() * 1000.0f);

  uint8_t data[FEB_CAN_PCU_PEDAL_VOLTAGES_LENGTH];
  int packed = feb_can_pcu_pedal_voltages_pack(data, &msg, sizeof(data));
  FEB_CAN_Status_t status =
      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_PCU_PEDAL_VOLTAGES_FRAME_ID, FEB_CAN_ID_STD, data, (uint8_t)packed);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to transmit pedal voltages: %s", FEB_CAN_StatusToString(status));
  }
}
