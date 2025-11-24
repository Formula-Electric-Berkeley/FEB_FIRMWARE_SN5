#ifndef INC_FEB_CAN_PCU_H_
#define INC_FEB_CAN_PCU_H_

#include <stdint.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "FEB_CAN_RX.h"

typedef struct {
	uint8_t brake_pedal; // Should be some value between 0 and 100
	float current;
	uint8_t enabled;
} FEB_CAN_PCU_Message_t;

extern FEB_CAN_PCU_Message_t FEB_CAN_PCU_Message;

void FEB_CAN_PCU_Init(void);
void FEB_CAN_PCU_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data, uint8_t length);
uint8_t FEB_CAN_PCU_Get_Enabled();
uint8_t FEB_CAN_PCU_Get_Brake_Pos();

#endif /* INC_FEB_CAN_PCU_H_ */
