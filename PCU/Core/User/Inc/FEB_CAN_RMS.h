#ifndef INC_FEB_CAN_RMS_H_
#define INC_FEB_CAN_RMS_H_

#include "stm32f4xx_hal.h"

#include "feb_can_lib.h"
#include "feb_can.h"
#include <string.h>

/* RMS Motor Controller CAN IDs (external device, not in generated library) */
#define FEB_CAN_ID_RMS_VOLTAGE ((uint32_t)0xa0)
#define FEB_CAN_ID_RMS_MOTOR ((uint32_t)0xa5)

#define FAULT_CLEAR_ADDR_UNDERVOLT 140
#define FAULT_CLEAR_ADDR_PARAM_SAFETY 20
#define FAULT_CLEAR_DATA 0
#define PARAM_BROADCAST_ADDR 148

/* Global variable - defined in FEB_CAN_RMS.c */
extern uint8_t PARAM_BROADCAST_DATA[2];

typedef struct RMS_MESSAGE_TYPE
{
  volatile int16_t HV_Bus_Voltage;     // Updated in ISR, read in main loop (raw)
  volatile int16_t Motor_Speed;        // Updated in ISR, read in main loop (RPM)
  volatile int16_t Motor_Angle;        // Updated in ISR, read in main loop
  volatile int16_t Torque_Command;     // Commanded torque (tenths of Nm)
  volatile int16_t Torque_Feedback;    // Feedback torque (tenths of Nm)
  volatile float DC_Bus_Voltage_V;     // DC bus voltage in Volts
  volatile uint32_t last_rx_timestamp; // 0 = never received, else HAL_GetTick() when last RX
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

#endif /* INC_FEB_CAN_RMS_H_ */
