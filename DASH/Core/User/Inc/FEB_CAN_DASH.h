#ifndef INC_FEB_CAN_DASH_H_
#define INC_FEB_CAN_DASH_H_

// **************************************** Includes ****************************************

#include "stm32f4xx_hal.h"
#include "FEB_CAN_RX.h"
#include <stdint.h>
#include <string.h>

typedef struct
{
  volatile uint32_t id;
  volatile uint32_t dlc;
  volatile uint8_t data[8];
} DASH_CAN_Rx_t;

typedef struct
{
  volatile uint8_t speed;
} FEB_CAN_DASH_Message_t;

typedef struct
{
  volatile uint8_t bms_state;
  volatile float ivt_voltage;
  volatile uint16_t max_acc_temp;
  volatile uint16_t min_voltage;
  volatile uint16_t pack_voltage;
  volatile uint16_t motor_speed;
} DASH_UI_Values_t;

extern DASH_UI_Values_t DASH_UI_Values;
extern int16_t lv_voltage;

// **************************************** Functions ****************************************

void FEB_CAN_DASH_Init(void);
void FEB_CAN_DASH_Callback(FEB_CAN_Instance_t instance, uint32_t can_id, FEB_CAN_ID_Type_t id_type, const uint8_t *data,
                           uint8_t length);
void FEB_CAN_DASH_Transmit_Button_State(uint8_t transmit_button_state);

#endif /* INC_FEB_CAN_DASH_H_ */
