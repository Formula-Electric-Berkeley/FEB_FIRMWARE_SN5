/**
 ******************************************************************************
 * @file           : FEB_CAN_DASH.h
 * @brief          : CAN DASH Receiving Module
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_DASH_H
#define FEB_CAN_DASH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  volatile bool button1;
  volatile bool button2;
  volatile bool button3;
  volatile bool button4;
  volatile bool switch1;
  volatile bool switch2;
  volatile bool switch3;
  volatile bool switch4;
  volatile bool buzzer;
  volatile bool ready_to_drive;
  volatile uint32_t last_rx_tick;
} DASH_State_t;

void FEB_CAN_DASH_Init(void);
DASH_State_t FEB_CAN_DASH_GetLastState(void);
bool FEB_CAN_LVPDB_IsDataFresh(uint32_t timeout_ms);

#endif /* FEB_CAN_DASH_H */
