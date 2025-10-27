#pragma once
#include <stdint.h>

// this interface focuses on the CAN application logic, so TX/RX scheduling

void app_can_init(void);                        
void app_can_tick_10ms(void);                   
void app_can_send_heartbeat(void);            
