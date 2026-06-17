#include "FEB_CAN_RMS.h"
#include "feb_log.h"

/* Number of inverter-disabled (0x0C0, enable=0) command frames sent at startup,
 * 10 ms apart, so the RMS sees a clean inverter_enable 0->1 edge later and comes
 * out of Inverter Enable Lockout before anything can command enable. */
#define RMS_STARTUP_DISABLE_FRAMES 10

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

/* Inverter Internal States (M170) accessors */
bool FEB_CAN_RMS_StatesSeen(void)
{
  return RMS_MESSAGE.states_rx_timestamp != 0;
}

uint8_t FEB_CAN_RMS_getVsmState(void)
{
  return RMS_MESSAGE.vsm_state;
}

uint8_t FEB_CAN_RMS_getInverterState(void)
{
  return RMS_MESSAGE.inverter_state;
}

bool FEB_CAN_RMS_getEnableState(void)
{
  return RMS_MESSAGE.enable_state != 0;
}

bool FEB_CAN_RMS_getEnableLockout(void)
{
  return RMS_MESSAGE.enable_lockout != 0;
}

bool FEB_CAN_RMS_getCommandModeVsm(void)
{
  return RMS_MESSAGE.command_mode != 0;
}

uint8_t FEB_CAN_RMS_getEchoRollingCounter(void)
{
  return RMS_MESSAGE.echo_rolling_counter;
}

/* Fault codes (M171) accessors */
bool FEB_CAN_RMS_FaultsSeen(void)
{
  return RMS_MESSAGE.faults_rx_timestamp != 0;
}

bool FEB_CAN_RMS_HasActiveFault(void)
{
  return (RMS_MESSAGE.post_fault_lo | RMS_MESSAGE.post_fault_hi | RMS_MESSAGE.run_fault_lo |
          RMS_MESSAGE.run_fault_hi) != 0;
}

uint16_t FEB_CAN_RMS_getPostFaultLo(void)
{
  return RMS_MESSAGE.post_fault_lo;
}

uint16_t FEB_CAN_RMS_getPostFaultHi(void)
{
  return RMS_MESSAGE.post_fault_hi;
}

uint16_t FEB_CAN_RMS_getRunFaultLo(void)
{
  return RMS_MESSAGE.run_fault_lo;
}

uint16_t FEB_CAN_RMS_getRunFaultHi(void)
{
  return RMS_MESSAGE.run_fault_hi;
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

  params.can_id = FEB_CAN_ID_RMS_STATES;
  FEB_CAN_RX_Register(&params);

  params.can_id = FEB_CAN_ID_RMS_FAULTS;
  FEB_CAN_RX_Register(&params);

  LOG_I(TAG_CAN, "Registered RMS CAN callbacks (Voltage: 0x%03X, Motor: 0x%03X, States: 0x%03X, Faults: 0x%03X)",
        (unsigned)FEB_CAN_ID_RMS_VOLTAGE, (unsigned)FEB_CAN_ID_RMS_MOTOR, (unsigned)FEB_CAN_ID_RMS_STATES,
        (unsigned)FEB_CAN_ID_RMS_FAULTS);

  RMS_MESSAGE.HV_Bus_Voltage = 0;
  RMS_MESSAGE.Motor_Speed = 0;
  RMS_MESSAGE.Motor_Angle = 0;
  RMS_MESSAGE.Torque_Command = 0;
  RMS_MESSAGE.Torque_Feedback = 0;
  RMS_MESSAGE.DC_Bus_Voltage_V = 0.0f;
  RMS_MESSAGE.last_rx_timestamp = 0;
  RMS_MESSAGE.vsm_state = 0;
  RMS_MESSAGE.inverter_state = 0;
  RMS_MESSAGE.enable_state = 0;
  RMS_MESSAGE.enable_lockout = 0;
  RMS_MESSAGE.command_mode = 0;
  RMS_MESSAGE.echo_rolling_counter = 0;
  RMS_MESSAGE.states_rx_timestamp = 0;
  RMS_MESSAGE.post_fault_lo = 0;
  RMS_MESSAGE.post_fault_hi = 0;
  RMS_MESSAGE.run_fault_lo = 0;
  RMS_MESSAGE.run_fault_hi = 0;
  RMS_MESSAGE.faults_rx_timestamp = 0;

  // LOG_I(TAG_CAN, "Sending RMS parameter safety commands");
  // for (int i = 0; i < 10; i++)
  // {
  //   FEB_CAN_RMS_Transmit_ParamSafety();
  //   HAL_Delay(10);
  // }

  // LOG_I(TAG_CAN, "Sending RMS undervolt disable commands");
  // for (int i = 0; i < 10; i++)
  // {
  //   FEB_CAN_RMS_Transmit_Disable_Undervolt();
  //   HAL_Delay(10);
  // }

  // LOG_I(TAG_CAN, "Sending RMS communication disable commands");
  // send disable command to remove lockout
  // for (int i = 0; i < 10; i++)
  // {
  //   FEB_CAN_RMS_Transmit_CommDisable();
  //   HAL_Delay(10);
  // }

  // Select CAN msg to broadcast
  FEB_CAN_RMS_Transmit_ParamBroadcast();

  // Command the inverter DISABLED before anything can enable it. The RMS powers
  // up in Inverter Enable Lockout and must see the command message with
  // inverter_enable = 0, then a clean 0->1 edge, to come out of lockout. This
  // also guarantees enable=1 is never the first 0x0C0 frame on the bus.
  LOG_I(TAG_CAN, "Commanding inverter DISABLED (lockout-safe startup)");
  for (int i = 0; i < RMS_STARTUP_DISABLE_FRAMES; i++)
  {
    FEB_CAN_RMS_Transmit_UpdateTorque(0, 0);
    HAL_Delay(10);
  }

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
    /* M167 Voltage Info: INV_DC_Bus_Voltage is signed, scale 0.1 V, no offset. */
    struct feb_can_m167_voltage_info_t m167;
    feb_can_m167_voltage_info_unpack(&m167, data, length);
    RMS_MESSAGE.HV_Bus_Voltage = m167.inv_dc_bus_voltage;
    RMS_MESSAGE.DC_Bus_Voltage_V = m167.inv_dc_bus_voltage / 10.0f;
  }
  else if (can_id == FEB_CAN_ID_RMS_MOTOR)
  {
    struct feb_can_m165_motor_position_info_t m165;
    feb_can_m165_motor_position_info_unpack(&m165, data, length);
    RMS_MESSAGE.Motor_Speed = m165.inv_motor_speed;
  }
  else if (can_id == FEB_CAN_ID_RMS_STATES)
  {
    /* M170 Internal States: the "why won't it enable" frame. */
    struct feb_can_m170_internal_states_t m170;
    feb_can_m170_internal_states_unpack(&m170, data, length);
    RMS_MESSAGE.vsm_state = m170.inv_vsm_state;
    RMS_MESSAGE.inverter_state = m170.inv_inverter_state;
    RMS_MESSAGE.enable_state = m170.inv_inverter_enable_state;
    RMS_MESSAGE.enable_lockout = m170.inv_inverter_enable_lockout;
    RMS_MESSAGE.command_mode = m170.inv_inverter_command_mode;
    RMS_MESSAGE.echo_rolling_counter = m170.inv_rolling_counter;
    RMS_MESSAGE.states_rx_timestamp = RMS_MESSAGE.last_rx_timestamp;
  }
  else if (can_id == FEB_CAN_ID_RMS_FAULTS)
  {
    /* M171 Fault Codes: POST/Run fault bitfields (see PM100 manual). */
    struct feb_can_m171_fault_codes_t m171;
    feb_can_m171_fault_codes_unpack(&m171, data, length);
    RMS_MESSAGE.post_fault_lo = m171.inv_post_fault_lo;
    RMS_MESSAGE.post_fault_hi = m171.inv_post_fault_hi;
    RMS_MESSAGE.run_fault_lo = m171.inv_run_fault_lo;
    RMS_MESSAGE.run_fault_hi = m171.inv_run_fault_hi;
    RMS_MESSAGE.faults_rx_timestamp = RMS_MESSAGE.last_rx_timestamp;
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

  /* The RMS' CAN message-validity check requires a rolling counter that changes
   * every command frame; a counter stuck at 0 makes it reject every frame as
   * stale and refuse to enable. Increment 0..15 (field range) on every TX. */
  static uint8_t rolling_counter = 0;

  struct feb_can_m192_command_message_t msg = {0};
  msg.vcu_inv_torque_command = torque;
  msg.vcu_inv_direction_command = 1u;
  msg.vcu_inv_inverter_enable = enabled;
  msg.vcu_inv_rolling_counter = rolling_counter;
  rolling_counter = (uint8_t)((rolling_counter + 1u) & 0x0Fu);

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
  send_rms_param(20u, 1u, 0, "undervolt disable");
}

void FEB_CAN_RMS_Transmit_ParamSafety(void)
{
  send_rms_param(140u, 1u, 1u, "precharge bypass");
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
