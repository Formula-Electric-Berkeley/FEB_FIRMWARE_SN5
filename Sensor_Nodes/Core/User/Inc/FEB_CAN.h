#ifndef INC_FEB_CAN_H_
#define INC_FEB_CAN_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "FEB_CAN_Library_SN4/gen/feb_can.h"

// CAN instance selection
typedef enum
{
  FEB_CAN_BUS_1 = 0,
  FEB_CAN_BUS_2 = 1
} FEB_CAN_Bus_t;

// Callback type for CAN RX messages
typedef void (*FEB_CAN_Rx_Callback_t)(FEB_CAN_Bus_t bus, CAN_RxHeaderTypeDef *rx_header, void *data);

// Initialize both CAN buses with RX callback
void FEB_CAN_Init(FEB_CAN_Rx_Callback_t callback);

// Configure CAN filters for both buses
void FEB_CAN_Filter_Config(void);

// Transmit a CAN message on specified bus
void FEB_CAN_Transmit(FEB_CAN_Bus_t bus, uint32_t std_id, uint8_t *data, uint8_t length);

// Ping Pong debug functions (uses counter IDs 0xE0-0xE3)
void FEB_CAN_PingPong_Send(FEB_CAN_Bus_t bus, uint8_t counter_id, int32_t counter_value);

#endif /* INC_FEB_CAN_H_ */
