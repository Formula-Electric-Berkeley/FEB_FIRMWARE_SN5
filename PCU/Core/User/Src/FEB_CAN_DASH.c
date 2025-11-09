#include "FEB_CAN_DASH.h"

/* Global dashboard message data */
DASH_MESSAGE_TYPE DASH_MESSAGE;

void FEB_CAN_DASH_Init(void) {
    /* Register callback for dashboard IO frame */
    FEB_CAN_RX_Register(FEB_CAN_INSTANCE_1, FEB_CAN_ID_DASH_IO, FEB_CAN_ID_STD, FEB_CAN_DASH_Callback);
    
    /* Initialize dashboard message structure */
    DASH_MESSAGE.ready_to_drive = false;
}

void FEB_CAN_DASH_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, uint8_t *data, uint8_t length) {
    if (can_id == FEB_CAN_ID_DASH_IO) {
        bool current_button_state = ((data[0] | 0b11111101) == 0b11111111);
        DASH_MESSAGE.ready_to_drive = current_button_state;
    }
}

bool FEB_DASH_Ready_To_Drive(void) {
    return DASH_MESSAGE.ready_to_drive;
}