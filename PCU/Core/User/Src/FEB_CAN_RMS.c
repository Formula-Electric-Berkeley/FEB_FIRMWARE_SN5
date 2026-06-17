#include "FEB_CAN_RMS.h"
#include "feb_log.h"

/* Number of inverter-disabled (0x0C0, enable=0) command frames sent at startup,
 * 10 ms apart, so the RMS sees a clean inverter_enable 0->1 edge later and comes
 * out of Inverter Enable Lockout before anything can command enable. */
#define RMS_STARTUP_DISABLE_FRAMES 10

extern CAN_HandleTypeDef hcan1;

/* Global RMS message data */
RMS_MESSAGE_TYPE RMS_MESSAGE;

/* Raw per-ID capture of every inverter broadcast frame (0x0A0..0x0AF + 0x0C2).
 * Updated in the RX ISR, dumped on demand by `PCU|rms|raw`. */
RMS_Frame_Record_t RMS_FRAMES[FEB_CAN_RMS_FRAME_TABLE_SIZE];

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

  // Subscribe to the WHOLE inverter broadcast block (0x0A0..0x0AF = M160..M175)
  // with a single mask filter so every frame the inverter emits is captured, not
  // just a hand-picked few. mask 0x7F0 makes the low 4 ID bits don't-care.
  FEB_CAN_RX_Params_t params = {
      .instance = FEB_CAN_INSTANCE_1,
      .id_type = FEB_CAN_ID_STD,
      .filter_type = FEB_CAN_FILTER_MASK,
      .can_id = FEB_CAN_RMS_FRAME_BASE_ID,
      .mask = 0x7F0u,
      .fifo = FEB_CAN_FIFO_0,
      .callback = FEB_CAN_RMS_Callback,
      .user_data = NULL,
  };
  FEB_CAN_RX_Register(&params);

  // Also capture the parameter read/write response (M194 / 0x0C2).
  params.filter_type = FEB_CAN_FILTER_EXACT;
  params.can_id = FEB_CAN_M194_READ_WRITE_PARAM_RESPONSE_FRAME_ID;
  params.mask = 0;
  FEB_CAN_RX_Register(&params);

  LOG_I(TAG_CAN, "Subscribed to RMS broadcast block 0x%03X-0x%03X + param resp 0x%03X",
        (unsigned)FEB_CAN_RMS_FRAME_BASE_ID, (unsigned)(FEB_CAN_RMS_FRAME_BASE_ID + FEB_CAN_RMS_FRAME_BLOCK_N - 1u),
        (unsigned)FEB_CAN_M194_READ_WRITE_PARAM_RESPONSE_FRAME_ID);

  memset(&RMS_MESSAGE, 0, sizeof(RMS_MESSAGE));
  memset(RMS_FRAMES, 0, sizeof(RMS_FRAMES));

  // NOTE: the PCU deliberately sends NO parameter (M193 / 0x0C1) writes. The RMS
  // "Read/Write Parameter Command" writes the inverter's EEPROM (addresses
  // 100..499), so a per-boot broadcast/undervolt write would touch EEPROM (or
  // alter inverter config) on every power cycle. The broadcast set is persisted
  // in the inverter's EEPROM and survives power cycles, so the inverter keeps
  // broadcasting from its saved config without us rewriting it. If a fresh /
  // EEPROM-wiped inverter shows no frames in `PCU|rms|raw`, reprovision its
  // broadcast set once with an external tool.

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

/* Store the raw payload of one inverter frame in its per-ID slot. Maps the
 * broadcast block 0x0A0..0x0AF to slots 0..15 and the param response 0x0C2 to
 * the last slot; anything else is ignored. Runs in ISR context. */
static void rms_capture_raw(uint32_t can_id, const uint8_t *data, uint8_t length, uint32_t tick)
{
  int idx = -1;
  if (can_id >= FEB_CAN_RMS_FRAME_BASE_ID && can_id < FEB_CAN_RMS_FRAME_BASE_ID + FEB_CAN_RMS_FRAME_BLOCK_N)
    idx = (int)(can_id - FEB_CAN_RMS_FRAME_BASE_ID);
  else if (can_id == FEB_CAN_M194_READ_WRITE_PARAM_RESPONSE_FRAME_ID)
    idx = FEB_CAN_RMS_FRAME_PARAM_RESP_IDX;

  if (idx < 0)
    return;

  RMS_Frame_Record_t *rec = &RMS_FRAMES[idx];
  uint8_t n = (length > 8u) ? 8u : length;
  for (uint8_t i = 0; i < n; i++)
    rec->data[i] = data[i];
  for (uint8_t i = n; i < 8u; i++)
    rec->data[i] = 0;
  rec->dlc = length;
  rec->count++;
  rec->last_rx_tick = tick;
  rec->seen = 1u;
}

static void FEB_CAN_RMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type,
                                 const uint8_t *data, uint8_t length, void *user_data)
{
  (void)instance;
  (void)id_type;
  (void)user_data;

  /* NOTE: This callback runs in ISR context - avoid logging and blocking operations */

  uint32_t tick = HAL_GetTick();
  RMS_MESSAGE.last_rx_timestamp = tick;

  /* Capture the raw frame for EVERY inverter ID first (nothing dropped), then
   * decode the ones we surface as engineering fields. */
  rms_capture_raw(can_id, data, length, tick);

  switch (can_id)
  {
  case FEB_CAN_M160_TEMPERATURE_SET_1_FRAME_ID:
  {
    struct feb_can_m160_temperature_set_1_t m160;
    feb_can_m160_temperature_set_1_unpack(&m160, data, length);
    RMS_MESSAGE.temp_module_a = m160.inv_module_a;
    RMS_MESSAGE.temp_module_b = m160.inv_module_b;
    RMS_MESSAGE.temp_module_c = m160.inv_module_c;
    RMS_MESSAGE.temp_gate_driver = m160.inv_gate_driver_board;
    RMS_MESSAGE.temps_rx_timestamp = tick;
    break;
  }
  case FEB_CAN_M161_TEMPERATURE_SET_2_FRAME_ID:
  {
    struct feb_can_m161_temperature_set_2_t m161;
    feb_can_m161_temperature_set_2_unpack(&m161, data, length);
    RMS_MESSAGE.temp_control_board = m161.inv_control_board_temperature;
    RMS_MESSAGE.temp_rtd1 = m161.inv_rtd1_temperature;
    RMS_MESSAGE.temp_rtd2 = m161.inv_rtd2_temperature;
    RMS_MESSAGE.temp_rtd3 = m161.inv_rtd3_temperature;
    RMS_MESSAGE.temps_rx_timestamp = tick;
    break;
  }
  case FEB_CAN_M162_TEMPERATURE_SET_3_FRAME_ID:
  {
    struct feb_can_m162_temperature_set_3_t m162;
    feb_can_m162_temperature_set_3_unpack(&m162, data, length);
    RMS_MESSAGE.temp_rtd4 = m162.inv_rtd4_temperature;
    RMS_MESSAGE.temp_rtd5 = m162.inv_rtd5_temperature;
    RMS_MESSAGE.temp_motor = m162.inv_motor_temperature;
    RMS_MESSAGE.torque_shudder = m162.inv_torque_shudder;
    RMS_MESSAGE.temps_rx_timestamp = tick;
    break;
  }
  case FEB_CAN_ID_RMS_MOTOR: /* M165 motor position */
  {
    struct feb_can_m165_motor_position_info_t m165;
    feb_can_m165_motor_position_info_unpack(&m165, data, length);
    RMS_MESSAGE.Motor_Speed = m165.inv_motor_speed;
    RMS_MESSAGE.Motor_Angle = (int16_t)m165.inv_motor_angle_electrical;
    RMS_MESSAGE.electrical_freq = m165.inv_electrical_output_frequency;
    break;
  }
  case FEB_CAN_M166_CURRENT_INFO_FRAME_ID:
  {
    struct feb_can_m166_current_info_t m166;
    feb_can_m166_current_info_unpack(&m166, data, length);
    RMS_MESSAGE.phase_a_current = m166.inv_phase_a_current;
    RMS_MESSAGE.phase_b_current = m166.inv_phase_b_current;
    RMS_MESSAGE.phase_c_current = m166.inv_phase_c_current;
    RMS_MESSAGE.dc_bus_current = m166.inv_dc_bus_current;
    RMS_MESSAGE.current_rx_timestamp = tick;
    break;
  }
  case FEB_CAN_ID_RMS_VOLTAGE: /* M167 voltage info */
  {
    /* INV_DC_Bus_Voltage is signed, scale 0.1 V, no offset. */
    struct feb_can_m167_voltage_info_t m167;
    feb_can_m167_voltage_info_unpack(&m167, data, length);
    RMS_MESSAGE.HV_Bus_Voltage = m167.inv_dc_bus_voltage;
    RMS_MESSAGE.DC_Bus_Voltage_V = m167.inv_dc_bus_voltage / 10.0f;
    RMS_MESSAGE.output_voltage = m167.inv_output_voltage;
    RMS_MESSAGE.vab_vd_voltage = m167.inv_vab_vd_voltage;
    RMS_MESSAGE.vbc_voltage = m167.inv_vbc_vq_voltage;
    break;
  }
  case FEB_CAN_ID_RMS_STATES: /* M170 internal states — the "why won't it enable" frame */
  {
    struct feb_can_m170_internal_states_t m170;
    feb_can_m170_internal_states_unpack(&m170, data, length);
    RMS_MESSAGE.vsm_state = m170.inv_vsm_state;
    RMS_MESSAGE.inverter_state = m170.inv_inverter_state;
    RMS_MESSAGE.enable_state = m170.inv_inverter_enable_state;
    RMS_MESSAGE.enable_lockout = m170.inv_inverter_enable_lockout;
    RMS_MESSAGE.command_mode = m170.inv_inverter_command_mode;
    RMS_MESSAGE.echo_rolling_counter = m170.inv_rolling_counter;
    RMS_MESSAGE.pwm_frequency = m170.inv_pwm_frequency;
    RMS_MESSAGE.relay_status = (uint8_t)((m170.inv_relay_1_status & 1u) | ((m170.inv_relay_2_status & 1u) << 1) |
                                         ((m170.inv_relay_3_status & 1u) << 2) | ((m170.inv_relay_4_status & 1u) << 3) |
                                         ((m170.inv_relay_5_status & 1u) << 4) | ((m170.inv_relay_6_status & 1u) << 5));
    RMS_MESSAGE.discharge_state = m170.inv_inverter_discharge_state;
    RMS_MESSAGE.run_mode = m170.inv_inverter_run_mode;
    RMS_MESSAGE.direction_command = m170.inv_direction_command;
    RMS_MESSAGE.bms_active = m170.inv_bms_active;
    RMS_MESSAGE.states_rx_timestamp = tick;
    break;
  }
  case FEB_CAN_ID_RMS_FAULTS: /* M171 fault codes — POST/Run bitfields (see PM100 manual) */
  {
    struct feb_can_m171_fault_codes_t m171;
    feb_can_m171_fault_codes_unpack(&m171, data, length);
    RMS_MESSAGE.post_fault_lo = m171.inv_post_fault_lo;
    RMS_MESSAGE.post_fault_hi = m171.inv_post_fault_hi;
    RMS_MESSAGE.run_fault_lo = m171.inv_run_fault_lo;
    RMS_MESSAGE.run_fault_hi = m171.inv_run_fault_hi;
    RMS_MESSAGE.faults_rx_timestamp = tick;
    break;
  }
  case FEB_CAN_M172_TORQUE_AND_TIMER_INFO_FRAME_ID:
  {
    struct feb_can_m172_torque_and_timer_info_t m172;
    feb_can_m172_torque_and_timer_info_unpack(&m172, data, length);
    RMS_MESSAGE.inv_commanded_torque = m172.inv_commanded_torque;
    RMS_MESSAGE.Torque_Feedback = m172.inv_torque_feedback;
    RMS_MESSAGE.power_on_timer = m172.inv_power_on_timer;
    RMS_MESSAGE.torque_timer_rx_timestamp = tick;
    break;
  }
  default:
    /* Other inverter frames (analog/digital in, flux, internal V, modulation,
     * firmware, diag, param response) are captured raw above and visible via
     * `PCU|rms|raw`; no engineering decode needed today. */
    break;
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

/* There are intentionally NO parameter-write (M193 / 0x0C1) functions here. The
 * PCU must never write the inverter's EEPROM or config. Torque/enable via M192
 * (above) is the only thing the PCU transmits to the inverter. */
