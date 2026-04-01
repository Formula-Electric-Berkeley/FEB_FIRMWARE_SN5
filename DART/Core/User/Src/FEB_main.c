#include "FEB_main.h"

extern CAN_HandleTypeDef hcan;

extern UART_HandleTypeDef huart2;

extern uint16_t frequency[NUM_FANS];

void FEB_Init(void)
{
  FEB_CAN_Init();
  FEB_Fan_Init();
}

void FEB_Main_Loop(void)
{
  // Fan speed set by CAN receive interrupt

  //	FEB_CAN_Transmit(&hcan, (uint8_t *)frequency);

  HAL_Delay(1000);
}
