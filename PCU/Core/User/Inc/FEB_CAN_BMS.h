#ifndef INC_FEB_CAN_BMS_H_
#define INC_FEB_CAN_BMS_H_

#include "stm32f4xx_hal.h"

#include "FEB_CAN_RX.h"

// BMS States
typedef enum {
	FEB_SM_ST_BOOT,
	FEB_SM_ST_LV,
	FEB_SM_ST_HEALTH_CHECK,
	FEB_SM_ST_PRECHARGE,
	FEB_SM_ST_ENERGIZED,
	FEB_SM_ST_DRIVE,
	FEB_SM_ST_FREE,
	FEB_SM_ST_CHARGER_PRECHARGE,
	FEB_SM_ST_CHARGING,
	FEB_SM_ST_BALANCE,
	FEB_SM_ST_FAULT_BMS,
	FEB_SM_ST_FAULT_BSPD,
	FEB_SM_ST_FAULT_IMD,
	FEB_SM_ST_FAULT_CHARGING,
	FEB_SM_ST_DEFAULT
} FEB_SM_ST_t;

// Heart Beat
typedef enum {
	FEB_HB_NULL,
	FEB_HB_DASH,
	FEB_HB_PCU,
	FEB_HB_LVPDB,
	FEB_HB_DCU,
	FEB_HB_FSN,
	FEB_HB_RSN
} FEB_HB_t;

typedef struct BMS_MESSAGE_TYPE {
    uint16_t temperature;
    uint16_t voltage;
    FEB_SM_ST_t state;
    FEB_HB_t ping_ack; // ping message
} BMS_MESSAGE_TYPE;
BMS_MESSAGE_TYPE BMS_MESSAGE;

uint16_t FEB_CAN_BMS_getTemp(void);
uint16_t FEB_CAN_BMS_getVoltage(void);
uint8_t FEB_CAN_BMS_getDeviceSelect(void);
FEB_SM_ST_t FEB_CAN_BMS_getState(void);
void FEB_CAN_RMS_Init(void);
void FEB_CAN_BMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data, uint8_t length);
void FEB_CAN_HEARTBEAT_Transmit(void);

#endif /* INC_FEB_CAN_BMS_H_ */