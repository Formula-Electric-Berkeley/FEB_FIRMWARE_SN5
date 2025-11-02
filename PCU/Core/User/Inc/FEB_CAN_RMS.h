#ifndef INC_FEB_CAN_RMS_H_
#define INC_FEB_CAN_RMS_H_

#include "stm32f4xx_hal.h"

#include "FEB_CAN_RX.h"
#include "FEB_CAN_TX.h"
#include "FEB_CAN_IDs.h"
#include <string.h>

#define FAULT_CLEAR_ADDR_UNDERVOLT 140
#define FAULT_CLEAR_ADDR_PARAM_SAFETY 20
#define FAULT_CLEAR_DATA 0
#define PARAM_BROADCAST_ADDR 148

/* Global variable - defined in FEB_CAN_RMS.c */
extern uint8_t PARAM_BROADCAST_DATA[2];

typedef struct RMS_MESSAGE_TYPE {
    volatile int16_t HV_Bus_Voltage;  // Updated in ISR, read in main loop
    volatile int16_t Motor_Speed;     // Updated in ISR, read in main loop
} RMS_MESSAGE_TYPE;

// Global variable - defined in FEB_CAN_RMS.c
extern RMS_MESSAGE_TYPE RMS_MESSAGE;

void FEB_CAN_RMS_Init(void);
void FEB_CAN_RMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, uint8_t *data, uint8_t length);
void FEB_CAN_RMS_Transmit_UpdateTorque(int16_t torque, uint8_t enabled);
void FEB_CAN_RMS_Transmit_Disable_Undervolt(void);
void FEB_CAN_RMS_Transmit_ParamSafety(void);
void FEB_CAN_RMS_Transmit_ParamBroadcast(void);
void FEB_CAN_RMS_Transmit_CommDisable(void);
#endif /* INC_FEB_CAN_RMS_H_ */