#ifndef FEB_MAIN_H
#define FEB_MAIN_H

#include "FEB_CAN.h"

void FEB_Init(void);
void FEB_Update(void);
void FEB_CAN_Rx_Callback(FEB_CAN_Bus_t bus, CAN_RxHeaderTypeDef *rx_header, void *data);
void FEB_Main_Loop(void);

#endif
