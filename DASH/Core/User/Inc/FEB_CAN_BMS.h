#ifndef INC_FEB_CAN_BMS_H_
#define INC_FEB_CAN_BMS_H_

// ********************************** Includes **********************************

#include "stm32f4xx_hal.h"
#include "FEB_CAN_RX.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>


// ********************************** Functions **********************************


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

void FEB_CAN_BMS_Init(void);
void FEB_CAN_BMS_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data, uint8_t length);
FEB_SM_ST_t FEB_CAN_BMS_Get_State();
bool FEB_CAN_BMS_GET_FAULTS();
bool FEB_CAN_GET_IMD_FAULT();
bool FEB_CAN_BMS_is_stale();



#endif /* INC_FEB_CAN_BMS_H_ */
