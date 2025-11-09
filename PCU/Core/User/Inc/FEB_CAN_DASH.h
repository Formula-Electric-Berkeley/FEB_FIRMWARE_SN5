#ifndef INC_FEB_CAN_DASH_H_
#define INC_FEB_CAN_DASH_H_

#include "stm32f4xx_hal.h"
#include "FEB_CAN_RX.h"
#include "FEB_CAN_IDs.h"
#include <stdbool.h>

/* Dashboard message structure */
typedef struct {
    bool ready_to_drive;        /* Ready to drive button state */
} DASH_MESSAGE_TYPE;

/* Global variable - defined in FEB_CAN_DASH.c */
extern DASH_MESSAGE_TYPE DASH_MESSAGE;

/* Function prototypes */
void FEB_CAN_DASH_Init(void);
void FEB_CAN_DASH_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, uint8_t *data, uint8_t length);
bool FEB_DASH_Ready_To_Drive(void);

#endif /* INC_FEB_CAN_DASH_H_ */