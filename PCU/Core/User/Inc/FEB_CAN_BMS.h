#ifndef INC_FEB_CAN_BMS_H_
#define INC_FEB_CAN_BMS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32f4xx_hal.h"

#include "feb_can_lib.h"
#include "feb_can_db.h"

#include "feb_bms_state.h"

  // BMS States - PCU names for the shared, DBC-generated BMS_State_t values
  typedef enum
  {
    FEB_SM_ST_BOOT = BMS_STATE_BOOT,
    FEB_SM_ST_LV_POWER = BMS_STATE_LV_POWER,                 // LV in SN4
    FEB_SM_ST_BUS_HEALTH_CHECK = BMS_STATE_BUS_HEALTH_CHECK, // HEALTH_CHECK in SN4
    FEB_SM_ST_PRECHARGE = BMS_STATE_PRECHARGE,
    FEB_SM_ST_ENERGIZED = BMS_STATE_ENERGIZED,
    FEB_SM_ST_DRIVE = BMS_STATE_DRIVE,
    FEB_SM_ST_BATTERY_FREE = BMS_STATE_BATTERY_FREE, // FREE in SN4
    FEB_SM_ST_CHARGER_PRECHARGE = BMS_STATE_CHARGER_PRECHARGE,
    FEB_SM_ST_CHARGING = BMS_STATE_CHARGING,
    FEB_SM_ST_BALANCE = BMS_STATE_BALANCE,
    FEB_SM_ST_FAULT_FIRST = BMS_STATE_FAULT_FIRST,
    FEB_SM_ST_COUNT = BMS_STATE_COUNT
  } FEB_SM_ST_t;

  typedef struct BMS_MESSAGE_TYPE
  {
    volatile FEB_SM_ST_t state;          // Updated in ISR, read in main loop
    volatile float max_temperature;      // Max accumulator temperature in C
    volatile float accumulator_voltage;  // Accumulator voltage in V
    volatile uint32_t last_rx_timestamp; // 0 = never received, else HAL_GetTick() when last RX
  } BMS_MESSAGE_TYPE;

  // Global variable - defined in FEB_CAN_BMS.c
  extern BMS_MESSAGE_TYPE BMS_MESSAGE;

  FEB_SM_ST_t FEB_CAN_BMS_getState(void);
  float FEB_CAN_BMS_getAccumulatorVoltage(void);
  float FEB_CAN_BMS_getMaxTemperature(void);
  void FEB_CAN_BMS_Init(void);
  void FEB_CAN_HEARTBEAT_Transmit(void);
  void FEB_CAN_BMS_ProcessHeartbeat(void);
  bool FEB_CAN_BMS_InDriveState(void);
  bool FEB_CAN_BMS_IsSilent(void);
  bool FEB_CAN_BMS_SetStateSim(bool enabled, FEB_SM_ST_t state);

#ifdef __cplusplus
}
#endif

#endif /* INC_FEB_CAN_BMS_H_ */
