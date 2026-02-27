#ifndef INC_FEB_CAN_BMS_H_
#define INC_FEB_CAN_BMS_H_

#include "stm32f4xx_hal.h"

#include "feb_can_lib.h"
#include "feb_can.h"

// BMS States - values must match BMS_State_t in BMS/Core/User/Inc/FEB_CAN_State.h
typedef enum
{
  FEB_SM_ST_BOOT = 0,
  FEB_SM_ST_ORIGIN,
  FEB_SM_ST_LV_POWER,
  FEB_SM_ST_BUS_HEALTH_CHECK,
  FEB_SM_ST_PRECHARGE,
  FEB_SM_ST_ENERGIZED,
  FEB_SM_ST_DRIVE,
  FEB_SM_ST_FAULT,
  FEB_SM_ST_CHARGING,
  FEB_SM_ST_BATTERY_FREE,
  FEB_SM_ST_BALANCE,
  FEB_SM_ST_COUNT
} FEB_SM_ST_t;

// Heart Beat
typedef enum
{
  FEB_HB_NULL,
  FEB_HB_DASH,
  FEB_HB_PCU,
  FEB_HB_LVPDB,
  FEB_HB_DCU,
  FEB_HB_FSN,
  FEB_HB_RSN
} FEB_HB_t;

typedef struct BMS_MESSAGE_TYPE
{
  volatile uint16_t temperature;       // Updated in ISR, read in main loop
  volatile uint16_t voltage;           // Updated in ISR, read in main loop (in 0.1V units)
  volatile FEB_SM_ST_t state;          // Updated in ISR, read in main loop
  volatile FEB_HB_t ping_ack;          // Updated in ISR, read in main loop
  volatile float max_temperature;      // Max accumulator temperature in C
  volatile float accumulator_voltage;  // Accumulator voltage in V
  volatile uint32_t last_rx_timestamp; // 0 = never received, else HAL_GetTick() when last RX
} BMS_MESSAGE_TYPE;

// Global variable - defined in FEB_CAN_BMS.c
extern BMS_MESSAGE_TYPE BMS_MESSAGE;

uint16_t FEB_CAN_BMS_getTemp(void);
uint16_t FEB_CAN_BMS_getVoltage(void);
uint8_t FEB_CAN_BMS_getDeviceSelect(void);
FEB_SM_ST_t FEB_CAN_BMS_getState(void);
float FEB_CAN_BMS_getAccumulatorVoltage(void);
float FEB_CAN_BMS_getMaxTemperature(void);
void FEB_CAN_BMS_Init(void);
void FEB_CAN_HEARTBEAT_Transmit(void);
void FEB_CAN_BMS_ProcessHeartbeat(void);
bool FEB_CAN_BMS_InDriveState(void);

#endif /* INC_FEB_CAN_BMS_H_ */
