/*
 * FEB_CAN_Heartbeat.h
 *
 *  Created on: Apr 13, 2025
 *      Author: samnesh
 */

#ifndef INC_FEB_CAN_HEARTBEAT_H_
#define INC_FEB_CAN_HEARTBEAT_H_

#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdint.h>

void FEB_CAN_HEARTBEAT_Transmit();
void FEB_CAN_HEARTBEAT_Init();

#endif /* INC_FEB_CAN_HEARTBEAT_H_ */
