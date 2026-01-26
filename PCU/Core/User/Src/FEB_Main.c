#include "FEB_Main.h"

void FEB_Main_While()
{

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

  HAL_Delay(1000);

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  HAL_Delay(1000);

  FEB_CAN_RX_Init();
}