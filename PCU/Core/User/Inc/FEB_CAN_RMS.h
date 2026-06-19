#ifndef INC_FEB_CAN_RMS_H_
#define INC_FEB_CAN_RMS_H_

#include "stm32f4xx_hal.h"

#include "feb_can_lib.h"
#include "feb_can.h"
#include <stdbool.h>
#include <string.h>

/* RMS Motor Controller CAN IDs — resolved from generated feb_can.h.
 * VOLTAGE is the real DC bus voltage broadcast (M167 / 0x0A7); STATES is the
 * inverter Internal States (M170 / 0x0AA, has VSM state + enable lockout) and
 * FAULTS is the POST/Run fault codes (M171 / 0x0AB). Subscribing to STATES and
 * FAULTS lets PCU|rms report *why* the inverter refuses to enable. */
#define FEB_CAN_ID_RMS_VOLTAGE FEB_CAN_M167_VOLTAGE_INFO_FRAME_ID
#define FEB_CAN_ID_RMS_MOTOR FEB_CAN_M165_MOTOR_POSITION_INFO_FRAME_ID
#define FEB_CAN_ID_RMS_STATES FEB_CAN_M170_INTERNAL_STATES_FRAME_ID
#define FEB_CAN_ID_RMS_FAULTS FEB_CAN_M171_FAULT_CODES_FRAME_ID

/* Inverter broadcast block: every frame the RMS emits lives in 0x0A0..0x0AF
 * (M160..M175). We subscribe to the whole block with a mask filter and capture
 * each frame, so nothing the inverter says is silently dropped. The parameter
 * response (M194 / 0x0C2) is captured too. */
#define FEB_CAN_RMS_FRAME_BASE_ID 0x0A0u    // M160
#define FEB_CAN_RMS_FRAME_BLOCK_N 16u       // 0x0A0..0x0AF (M160..M175)
#define FEB_CAN_RMS_FRAME_PARAM_RESP_IDX 16 // extra slot for 0x0C2 (M194)
#define FEB_CAN_RMS_FRAME_TABLE_SIZE 17

/* One captured inverter frame (latest contents per CAN ID). Written in the RX
 * ISR, read by the console; a torn read on a diagnostic dump is acceptable. */
typedef struct
{
  volatile uint8_t data[8];       // latest raw payload
  volatile uint8_t dlc;           // received length
  volatile uint8_t seen;          // 0 = never received
  volatile uint32_t count;        // total frames received for this ID
  volatile uint32_t last_rx_tick; // HAL_GetTick() at last RX
} RMS_Frame_Record_t;

typedef struct RMS_MESSAGE_TYPE
{
  volatile int16_t HV_Bus_Voltage;     // M167 DC bus voltage, raw (tenths of V)
  volatile int16_t Motor_Speed;        // M165 motor speed (RPM)
  volatile int16_t Motor_Angle;        // M165 electrical angle, raw (tenths of deg)
  volatile int16_t Torque_Command;     // Last torque we commanded (tenths of Nm)
  volatile int16_t Torque_Feedback;    // M172 estimated torque feedback (tenths of Nm)
  volatile float DC_Bus_Voltage_V;     // DC bus voltage in Volts (from M167)
  volatile uint32_t last_rx_timestamp; // 0 = never received, else HAL_GetTick() when last RX

  /* Motor position extras (M165 / 0x0A5) */
  volatile int16_t electrical_freq; // INV_Electrical_Output_Frequency (tenths of Hz)

  /* Voltage extras (M167 / 0x0A7) — tenths of V */
  volatile int16_t output_voltage; // peak line-neutral
  volatile int16_t vab_vd_voltage;
  volatile int16_t vbc_voltage;

  /* Phase / DC currents (M166 / 0x0A6) — tenths of A */
  volatile int16_t phase_a_current;
  volatile int16_t phase_b_current;
  volatile int16_t phase_c_current;
  volatile int16_t dc_bus_current;
  volatile uint32_t current_rx_timestamp; // 0 = never received an M166 frame

  /* Temperatures (M160..M162 / 0x0A0..0x0A2) — tenths of degC */
  volatile int16_t temp_module_a;
  volatile int16_t temp_module_b;
  volatile int16_t temp_module_c;
  volatile int16_t temp_gate_driver;
  volatile int16_t temp_control_board;
  volatile int16_t temp_rtd1;
  volatile int16_t temp_rtd2;
  volatile int16_t temp_rtd3;
  volatile int16_t temp_rtd4;
  volatile int16_t temp_rtd5;
  volatile int16_t temp_motor;
  volatile int16_t torque_shudder; // M162 shudder compensation (tenths of Nm)
  volatile uint32_t temps_rx_timestamp;

  /* Inverter Internal States (M170 / 0x0AA) — written in ISR, read in main loop */
  volatile uint8_t vsm_state;            // INV_VSM_State (0..15)
  volatile uint8_t inverter_state;       // INV_Inverter_State (0..255)
  volatile uint8_t enable_state;         // 0 = disabled, 1 = enabled
  volatile uint8_t enable_lockout;       // 1 = inverter enable lockout active
  volatile uint8_t command_mode;         // 0 = CAN mode, 1 = VSM mode
  volatile uint8_t echo_rolling_counter; // inverter-echoed rolling counter (0..15)
  volatile uint8_t pwm_frequency;        // INV_PWM_Frequency (kHz)
  volatile uint8_t relay_status;         // relays 1..6 packed into bits 0..5
  volatile uint8_t discharge_state;      // INV_Inverter_Discharge_State
  volatile uint8_t run_mode;             // 0 = torque, 1 = speed
  volatile uint8_t direction_command;    // inverter-reported direction
  volatile uint8_t bms_active;           // INV_BMS_Active
  volatile uint32_t states_rx_timestamp; // 0 = never received an M170 frame

  /* Fault codes (M171 / 0x0AB) — each word is a fault bitfield (see PM100 manual) */
  volatile uint16_t post_fault_lo;
  volatile uint16_t post_fault_hi;
  volatile uint16_t run_fault_lo;
  volatile uint16_t run_fault_hi;
  volatile uint32_t faults_rx_timestamp; // 0 = never received an M171 frame

  /* Torque & timer (M172 / 0x0AC) */
  volatile int16_t inv_commanded_torque;       // inverter-echoed commanded torque (tenths of Nm)
  volatile uint32_t power_on_timer;            // INV_Power_On_Timer (units of 3 ms)
  volatile uint32_t torque_timer_rx_timestamp; // 0 = never received an M172 frame

  /* M170 extra limiting / mode bits (0x0AA) */
  volatile uint8_t start_mode_active;   // INV_Start_Mode_Active
  volatile uint8_t bms_torque_limiting; // INV_BMS_Torque_Limiting
  volatile uint8_t max_speed_limiting;  // INV_Max_Speed_Limiting
  volatile uint8_t low_speed_limiting;  // INV_Low_Speed_Limiting

  /* Analog inputs (M163 / 0x0A3) — hundredths of V (0..10.23 V) */
  volatile uint16_t analog_in[6];
  volatile uint32_t analog_rx_timestamp;

  /* Digital inputs (M164 / 0x0A4) — bits 0..7 = DIN1..DIN8 */
  volatile uint8_t digital_in;
  volatile uint32_t digital_rx_timestamp;

  /* Flux / Id / Iq (M168 / 0x0A8) */
  volatile int16_t flux_command;  // thousandths of Wb
  volatile int16_t flux_feedback; // thousandths of Wb
  volatile int16_t i_d;           // tenths of A (D-axis)
  volatile int16_t i_q;           // tenths of A (Q-axis)
  volatile uint32_t flux_rx_timestamp;

  /* Internal reference voltages (M169 / 0x0A9) — hundredths of V */
  volatile int16_t ref_voltage_1_5;
  volatile int16_t ref_voltage_2_5;
  volatile int16_t ref_voltage_5_0;  // transducer supply
  volatile int16_t ref_voltage_12_0; // 12 V input
  volatile uint32_t intv_rx_timestamp;

  /* Modulation index & flux weakening (M173 / 0x0AD) */
  volatile int16_t modulation_index;      // raw; actual = raw / 100
  volatile int16_t flux_weakening_output; // tenths of A
  volatile int16_t id_command;            // tenths of A
  volatile int16_t iq_command;            // tenths of A
  volatile uint32_t mod_rx_timestamp;

  /* Firmware info (M174 / 0x0AE) */
  volatile uint16_t fw_eeprom_version;
  volatile uint16_t fw_sw_version;
  volatile uint16_t fw_date_mmdd;
  volatile uint16_t fw_date_yyyy;
  volatile uint32_t fw_rx_timestamp;
} RMS_MESSAGE_TYPE;

// Global variable - defined in FEB_CAN_RMS.c
extern RMS_MESSAGE_TYPE RMS_MESSAGE;

// Raw per-ID capture table for every inverter broadcast frame - defined in FEB_CAN_RMS.c
extern RMS_Frame_Record_t RMS_FRAMES[FEB_CAN_RMS_FRAME_TABLE_SIZE];

// Initialization and callback
void FEB_CAN_RMS_Init(void);

// Transmit functions. M192 torque/enable is the only frame sent automatically.
// M193 parameter writes are emitted ONLY from explicit console commands (never
// at init): ClearFaults (param 20, transient) is always safe; PrechargeBypass
// (param 140) is a persistent EEPROM write gated by `PCU|rms|eeprom|precharge`.
void FEB_CAN_RMS_Transmit_UpdateTorque(int16_t torque, uint8_t enabled);

// Clear the inverter's latched faults (M193 param 20, write 0).
void FEB_CAN_RMS_Transmit_ClearFaults(void);

// Enable/disable the inverter's internal precharge (M193 param 140, EEPROM).
// bypass=true disables the inverter's own precharge; bypass=false restores it.
void FEB_CAN_RMS_Transmit_PrechargeBypass(bool bypass);

// Decode the most recent M194 parameter response (0x0C2). Returns false if no
// response has been received. Used to confirm an M193 write took effect.
bool FEB_CAN_RMS_GetLastParamResponse(uint16_t *addr, bool *write_ok, int16_t *data, uint32_t *age_ticks);

// Generic parameter access (M193 command -> M194 response). Each call sends the
// command then blocks (bare-metal poll) up to timeout_ms for the M194 whose
// echoed address matches `addr`. Returns false on timeout or unrecognized
// address (the inverter echoes address 0 for an unknown parameter).
//  - Reads are always safe.
//  - EEPROM writes (addresses 100..499) require the motor disabled — the
//    inverter rejects them otherwise; the caller enforces the guard.
bool FEB_CAN_RMS_ReadParam(uint16_t addr, int16_t *out_value, uint32_t timeout_ms);
bool FEB_CAN_RMS_WriteParam(uint16_t addr, int16_t value, bool *out_write_ok, uint32_t timeout_ms);

// Datasheet lookups (Cascadia "CAN Protocol" V6.3).
// FaultName: M171 fault bit 0..63 — POST Lo/Hi = bits 0..31, RUN Lo/Hi = bits
// 32..63. Returns NULL for reserved/undocumented bits.
const char *FEB_CAN_RMS_FaultName(uint8_t bit);

// ParamName: human name for a CAN parameter address (command or EEPROM), or NULL.
const char *FEB_CAN_RMS_ParamName(uint16_t addr);

// Documented parameter table, sorted by address (used by `readall`).
typedef struct
{
  uint16_t addr;
  const char *name;
} FEB_RMS_Param_t;
const FEB_RMS_Param_t *FEB_CAN_RMS_ParamTable(size_t *count);

// Accessor functions for console commands
float FEB_CAN_RMS_getDCBusVoltage(void);
int16_t FEB_CAN_RMS_getMotorSpeed(void);
int16_t FEB_CAN_RMS_getMotorAngle(void); // raw tenths of electrical degree
float FEB_CAN_RMS_getTorqueCommand(void);
float FEB_CAN_RMS_getTorqueFeedback(void);

// Inverter Internal States (M170) accessors
bool FEB_CAN_RMS_StatesSeen(void);     // true once an M170 frame has been received
uint8_t FEB_CAN_RMS_getVsmState(void); // INV_VSM_State (0..15)
uint8_t FEB_CAN_RMS_getInverterState(void);
bool FEB_CAN_RMS_getEnableState(void);    // inverter-reported enable state
bool FEB_CAN_RMS_getEnableLockout(void);  // true = inverter enable lockout active
bool FEB_CAN_RMS_getCommandModeVsm(void); // true = VSM mode, false = CAN mode
uint8_t FEB_CAN_RMS_getEchoRollingCounter(void);

// Fault codes (M171) accessors
bool FEB_CAN_RMS_FaultsSeen(void);     // true once an M171 frame has been received
bool FEB_CAN_RMS_HasActiveFault(void); // true if any POST/Run fault word is nonzero
uint16_t FEB_CAN_RMS_getPostFaultLo(void);
uint16_t FEB_CAN_RMS_getPostFaultHi(void);
uint16_t FEB_CAN_RMS_getRunFaultLo(void);
uint16_t FEB_CAN_RMS_getRunFaultHi(void);

#endif /* INC_FEB_CAN_RMS_H_ */
