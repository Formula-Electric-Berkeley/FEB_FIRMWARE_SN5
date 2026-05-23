#include "FEB_CAN_RMS.h"
#include "feb_log.h"

extern CAN_HandleTypeDef hcan1;

/* Global RMS message data */
RMS_MESSAGE_TYPE RMS_MESSAGE;

/* RMS parameter broadcast data */
uint8_t PARAM_BROADCAST_DATA[2] = {0b10100000, 0b00010101};

/* Forward declaration of callback with new signature */
static void FEB_CAN_RMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data);

/* Accessor functions for console commands */
float FEB_CAN_RMS_getDCBusVoltage(void)
{
  return RMS_MESSAGE.DC_Bus_Voltage_V;
}

int16_t FEB_CAN_RMS_getMotorSpeed(void)
{
  return RMS_MESSAGE.Motor_Speed;
}

int16_t FEB_CAN_RMS_getMotorAngle(void)
{
  return RMS_MESSAGE.Motor_Angle;
}

float FEB_CAN_RMS_getTorqueCommand(void)
{
  return RMS_MESSAGE.Torque_Command / 10.0f;
}

float FEB_CAN_RMS_getTorqueFeedback(void)
{
  return RMS_MESSAGE.Torque_Feedback / 10.0f;
}

void FEB_CAN_RMS_Init(void)
{
  LOG_I(TAG_CAN, "Initializing RMS CAN communication");

  // Register RX callbacks using new API
  FEB_CAN_RX_Params_t params = {
      .instance = FEB_CAN_INSTANCE_1,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_EXACT,
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_RMS_Callback,
      .user_data = NULL,
  };

  params.can_id = FEB_CAN_ID_RMS_VOLTAGE;
  FEB_CAN_RX_Register(&params);

  params.can_id = FEB_CAN_ID_RMS_MOTOR;
  FEB_CAN_RX_Register(&params);

  LOG_I(TAG_CAN, "Registered RMS CAN callbacks (Voltage: 0x%03X, Motor: 0x%03X)", (unsigned)FEB_CAN_ID_RMS_VOLTAGE,
        (unsigned)FEB_CAN_ID_RMS_MOTOR);

  RMS_MESSAGE.HV_Bus_Voltage = 0;
  RMS_MESSAGE.Motor_Speed = 0;
  RMS_MESSAGE.Motor_Angle = 0;
  RMS_MESSAGE.Torque_Command = 0;
  RMS_MESSAGE.Torque_Feedback = 0;
  RMS_MESSAGE.DC_Bus_Voltage_V = 0.0f;
  RMS_MESSAGE.last_rx_timestamp = 0;

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

static void FEB_CAN_RMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)user_data;

  /* NOTE: This callback runs in ISR context - avoid logging and blocking operations */

  RMS_MESSAGE.last_rx_timestamp = HAL_GetTick();

  if (can_id == FEB_CAN_ID_RMS_VOLTAGE)
  {
    struct feb_can_m160_temperature_set_1_t m160;
    feb_can_m160_temperature_set_1_unpack(&m160, data, length);
    RMS_MESSAGE.HV_Bus_Voltage = m160.inv_module_a;
    RMS_MESSAGE.DC_Bus_Voltage_V = (m160.inv_module_a - 50.0f) / 10.0f;
  }
  else if (can_id == FEB_CAN_ID_RMS_MOTOR)
  {
    struct feb_can_m165_motor_position_info_t m165;
    feb_can_m165_motor_position_info_unpack(&m165, data, length);
    RMS_MESSAGE.Motor_Speed = m165.inv_motor_speed;
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

  RMS_MESSAGE.Torque_Command = torque;

  struct feb_can_m192_command_message_t msg = {0};
  msg.vcu_inv_torque_command = torque;
  msg.vcu_inv_direction_command = 1u;
  msg.vcu_inv_inverter_enable = enabled;

  uint8_t data[FEB_CAN_M192_COMMAND_MESSAGE_LENGTH];
  int packed = feb_can_m192_command_message_pack(data, &msg, sizeof(data));
  FEB_CAN_Status_t status =
      FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_M192_COMMAND_MESSAGE_FRAME_ID, FEB_CAN_ID_STD, data, (uint8_t)packed);
  if (status != FEB_CAN_OK)
  {
    LOG_E(TAG_CAN, "Failed to transmit torque command: %s", FEB_CAN_StatusToString(status));
  }
}

static void send_rms_param(uint16_t address, uint8_t command, int16_t param_data, const char *tag)
{
  struct feb_can_m193_read_write_param_command_t msg = {0};
  msg.vcu_inv_parameter_address_command = address;
  msg.vcu_inv_read_write_command = command;
  msg.vcu_inv_data_command = param_data;

  uint8_t buf[FEB_CAN_M193_READ_WRITE_PARAM_COMMAND_LENGTH];
  int packed = feb_can_m193_read_write_param_command_pack(buf, &msg, sizeof(buf));
  FEB_CAN_Status_t status = FEB_CAN_TX_Send(FEB_CAN_INSTANCE_1, FEB_CAN_M193_READ_WRITE_PARAM_COMMAND_FRAME_ID,
                                            FEB_CAN_ID_STD, buf, (uint8_t)packed);
  if (status != FEB_CAN_OK)
    LOG_E(TAG_CAN, "Failed to transmit %s: %s", tag, FEB_CAN_StatusToString(status));
}

void FEB_CAN_RMS_Transmit_Disable_Undervolt(void)
{
  send_rms_param(FAULT_CLEAR_ADDR_UNDERVOLT, 1u, FAULT_CLEAR_DATA, "undervolt disable");
}

void FEB_CAN_RMS_Transmit_ParamSafety(void)
{
  send_rms_param(FAULT_CLEAR_ADDR_PARAM_SAFETY, 1u, FAULT_CLEAR_DATA, "param safety");
}

void FEB_CAN_RMS_Transmit_ParamBroadcast(void)
{
  int16_t val = (int16_t)((PARAM_BROADCAST_DATA[1] << 8) | PARAM_BROADCAST_DATA[0]);
  send_rms_param(PARAM_BROADCAST_ADDR, 1u, val, "param broadcast");
  LOG_D(TAG_CAN, "Param broadcast sent: 0x%02X 0x%02X", PARAM_BROADCAST_DATA[0], PARAM_BROADCAST_DATA[1]);
}

void FEB_CAN_RMS_Transmit_CommDisable(void)
{
  send_rms_param(0u, 0u, 0, "comm disable");
}
