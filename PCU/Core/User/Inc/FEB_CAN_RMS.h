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

#define FAULT_CLEAR_ADDR_UNDERVOLT 140
#define FAULT_CLEAR_ADDR_PARAM_SAFETY 20
#define FAULT_CLEAR_DATA 0
#define PARAM_BROADCAST_ADDR 148

/* Global variable - defined in FEB_CAN_RMS.c */
extern uint8_t PARAM_BROADCAST_DATA[2];

typedef struct RMS_MESSAGE_TYPE
{
  volatile int16_t HV_Bus_Voltage;     // M167 DC bus voltage, raw (tenths of V)
  volatile int16_t Motor_Speed;        // Updated in ISR, read in main loop (RPM)
  volatile int16_t Motor_Angle;        // Updated in ISR, read in main loop
  volatile int16_t Torque_Command;     // Commanded torque (tenths of Nm)
  volatile int16_t Torque_Feedback;    // Feedback torque (tenths of Nm)
  volatile float DC_Bus_Voltage_V;     // DC bus voltage in Volts (from M167)
  volatile uint32_t last_rx_timestamp; // 0 = never received, else HAL_GetTick() when last RX

  /* Inverter Internal States (M170 / 0x0AA) — written in ISR, read in main loop */
  volatile uint8_t vsm_state;            // INV_VSM_State (0..15)
  volatile uint8_t inverter_state;       // INV_Inverter_State (0..255)
  volatile uint8_t enable_state;         // 0 = disabled, 1 = enabled
  volatile uint8_t enable_lockout;       // 1 = inverter enable lockout active
  volatile uint8_t command_mode;         // 0 = CAN mode, 1 = VSM mode
  volatile uint8_t echo_rolling_counter; // inverter-echoed rolling counter (0..15)
  volatile uint32_t states_rx_timestamp; // 0 = never received an M170 frame

  /* Fault codes (M171 / 0x0AB) — each word is a fault bitfield (see PM100 manual) */
  volatile uint16_t post_fault_lo;
  volatile uint16_t post_fault_hi;
  volatile uint16_t run_fault_lo;
  volatile uint16_t run_fault_hi;
  volatile uint32_t faults_rx_timestamp; // 0 = never received an M171 frame
} RMS_MESSAGE_TYPE;

// Global variable - defined in FEB_CAN_RMS.c
extern RMS_MESSAGE_TYPE RMS_MESSAGE;

// Initialization and callback
void FEB_CAN_RMS_Init(void);

// Transmit functions
void FEB_CAN_RMS_Transmit_UpdateTorque(int16_t torque, uint8_t enabled);
void FEB_CAN_RMS_Transmit_Disable_Undervolt(void);
void FEB_CAN_RMS_Transmit_ParamSafety(void);
void FEB_CAN_RMS_Transmit_ParamBroadcast(void);
void FEB_CAN_RMS_Transmit_CommDisable(void);

// Accessor functions for console commands
float FEB_CAN_RMS_getDCBusVoltage(void);
int16_t FEB_CAN_RMS_getMotorSpeed(void);
int16_t FEB_CAN_RMS_getMotorAngle(void);
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
