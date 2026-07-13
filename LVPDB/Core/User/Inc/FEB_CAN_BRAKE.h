/**
 ******************************************************************************
 * @file           : FEB_CAN_BRAKE.h
 * @brief          : CAN BRAKE Receiving Module
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_BRAKE_H
#define FEB_CAN_BRAKE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  volatile uint16_t brake_position; /* centi-percent (0-10000) */
  volatile uint32_t last_rx_tick;
} BRAKE_State_t;

void FEB_CAN_BRAKE_Init(void);
uint8_t FEB_CAN_BRAKE_GetPercent(void);
bool FEB_CAN_BRAKE_IsDataFresh(uint32_t timeout_ms);

#endif /* FEB_CAN_BRAKE_H */
