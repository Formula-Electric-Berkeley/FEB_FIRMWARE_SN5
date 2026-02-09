#include "FEB_CAN_RMS.h"
#include "FEB_Debug.h"

extern CAN_HandleTypeDef hcan1;

/* Global RMS message data */
RMS_MESSAGE_TYPE RMS_MESSAGE;

/* RMS parameter broadcast data */
uint8_t PARAM_BROADCAST_DATA[2] = {0b10100000, 0b00010101};

void FEB_CAN_RMS_Init(void)
{
  LOG_I(TAG_CAN, "Initializing RMS CAN communication");

  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_VOLTAGE, FEB_CAN_ID_STD, FEB_CAN_RMS_Callback);
  FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_MOTOR, FEB_CAN_ID_STD, FEB_CAN_RMS_Callback);
  LOG_I(TAG_CAN, "Registered RMS CAN callbacks (Voltage: 0x%03lX, Motor: 0x%03lX)", FEB_CAN_ID_RMS_VOLTAGE,
        FEB_CAN_ID_RMS_MOTOR);

  RMS_MESSAGE.HV_Bus_Voltage = 0;
  RMS_MESSAGE.Motor_Speed = 0;

  LOG_I(TAG_CAN, "Sending RMS parameter safety commands");
  for (int i = 0; i < 10; i++)
  {
    FEB_CAN_RMS_Transmit_ParamSafety();
    HAL_Delay(10);
  }

  LOG_I(TAG_CAN, "Sending RMS undervolt disable commands");
  for (int i = 0; i < 10; i++)
  {
    FEB_CAN_RMS_Transmit_Disable_Undervolt();
    HAL_Delay(10);
  }

  LOG_I(TAG_CAN, "Sending RMS communication disable commands");
  // send disable command to remove lockout
  for (int i = 0; i < 10; i++)
  {
    FEB_CAN_RMS_Transmit_CommDisable();
    HAL_Delay(10);
  }

  // Select CAN msg to broadcast
  FEB_CAN_RMS_Transmit_ParamBroadcast();
  LOG_I(TAG_CAN, "RMS CAN initialization complete");
}

void FEB_CAN_RMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, uint8_t *data,
                          uint8_t length)
{
  LOG_D(TAG_RMS, "RMS Callback: ID=0x%03lX, Len=%d, Payload: %02X %02X %02X %02X %02X %02X %02X %02X", can_id, length,
        data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

  if (can_id == FEB_CAN_ID_RMS_VOLTAGE)
  {
    int16_t temp_voltage;
    memcpy(&temp_voltage, data, 2);
    RMS_MESSAGE.HV_Bus_Voltage = temp_voltage;
    float voltage_v = (temp_voltage - 50.0f) / 10.0f;
    LOG_D(TAG_CAN, "RMS voltage: %.1fV (raw: %d)", voltage_v, temp_voltage);
  }
  else if (can_id == FEB_CAN_ID_RMS_MOTOR)
  {
    int16_t temp_speed;
    memcpy(&temp_speed, data + 2, 2);
    RMS_MESSAGE.Motor_Speed = temp_speed;
    LOG_D(TAG_CAN, "RMS motor speed: %d RPM", temp_speed);
  }
  else
  {
    LOG_W(TAG_CAN, "Unknown RMS CAN ID: 0x%03lX", can_id);
  }
}

/**
 * @brief Transmit torque command to RMS motor controller
 *
 * @param torque Commanded torque in tenths of Nm (e.g., 2300 = 230.0 Nm)
 *               Valid range: -32768 to +32767 (int16_t limits)
 * @param enabled Enable flag: 1 = inverter enabled, 0 = disabled
 */
void FEB_CAN_RMS_Transmit_UpdateTorque(int16_t torque, uint8_t enabled)
{

// Bounds checking: Limit torque to motor capabilities
// Negative torque (regen) is allowed within motor limits
#define MAX_REGEN_TORQUE -3000 // Max regen: -300.0 Nm
#define MAX_MOTOR_TORQUE 3000  // Max motor: +300.0 Nm

  int16_t original_torque = torque;
  if (torque > MAX_MOTOR_TORQUE)
  {
    torque = MAX_MOTOR_TORQUE;
    LOG_W(TAG_CAN, "Torque clamped to max: %d -> %d", original_torque, torque);
  }
  else if (torque < MAX_REGEN_TORQUE)
  {
    torque = MAX_REGEN_TORQUE;
    LOG_W(TAG_CAN, "Torque clamped to max regen: %d -> %d", original_torque, torque);
  }

  uint8_t data[8];
  data[0] = (uint8_t)(torque & 0xFF);
  data[1] = (uint8_t)((torque >> 8) & 0xFF);
  data[2] = 0;
  data[3] = 0;
  data[4] = 1; // Direction: 1 = forward, 0 = reverse
  data[5] = enabled;
  data[6] = 0;
  data[7] = 0;

  FEB_CAN_Status_t status = FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_TORQUE, data, 8);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to transmit torque command: %d", status);
  }
  else
  {
    // Wait briefly then check if TX completed
    HAL_Delay(5);
    uint32_t free_mailboxes = HAL_CAN_GetTxMailboxesFreeLevel(&hcan1);
    if (free_mailboxes < 3)
    {
      LOG_W(TAG_CAN, "Torque TX may have failed - mailbox still pending (free: %lu)", free_mailboxes);
    }
    LOG_D(TAG_CAN, "Torque command sent: %.1f Nm, enabled: %d", torque / 10.0f, enabled);
  }
}

void FEB_CAN_RMS_Transmit_Disable_Undervolt(void)
{
  uint8_t data[8];
  data[0] = FAULT_CLEAR_ADDR_UNDERVOLT;
  data[1] = 0;
  data[2] = 1;
  data[3] = 0;
  data[4] = FAULT_CLEAR_DATA;
  data[5] = 0;
  data[6] = 0;
  data[7] = 0;
  FEB_CAN_Status_t status = FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_PARAM, data, 8);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to transmit undervolt disable: %d", status);
  }
}

void FEB_CAN_RMS_Transmit_ParamSafety(void)
{
  uint8_t data[8];
  data[0] = FAULT_CLEAR_ADDR_PARAM_SAFETY;
  data[1] = 0;
  data[2] = 1;
  data[3] = 0;
  data[4] = FAULT_CLEAR_DATA;
  data[5] = 0;
  data[6] = 0;
  data[7] = 0;
  FEB_CAN_Status_t status = FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_PARAM, data, 8);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to transmit param safety: %d", status);
  }
}

void FEB_CAN_RMS_Transmit_ParamBroadcast(void)
{
  uint8_t data[8];
  data[0] = PARAM_BROADCAST_ADDR;
  data[1] = 0;
  data[2] = 1;
  data[3] = 0;
  data[4] = PARAM_BROADCAST_DATA[0];
  data[5] = PARAM_BROADCAST_DATA[1];
  data[6] = 0;
  data[7] = 0;
  FEB_CAN_Status_t status = FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_PARAM, data, 8);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to transmit param broadcast: %d", status);
  }
  else
  {
    LOG_D(TAG_CAN, "Param broadcast sent: 0x%02X 0x%02X", PARAM_BROADCAST_DATA[0], PARAM_BROADCAST_DATA[1]);
  }
}

void FEB_CAN_RMS_Transmit_CommDisable(void)
{
  uint8_t data[8];
  data[0] = 0;
  data[1] = 0;
  data[2] = 0;
  data[3] = 0;
  data[4] = 0;
  data[5] = 0;
  data[6] = 0;
  data[7] = 0;
  FEB_CAN_Status_t status = FEB_CAN_TX_TransmitDefault(FEB_CAN_INSTANCE_1, FEB_CAN_ID_RMS_PARAM, data, 8);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to transmit comm disable: %d", status);
  }
}
