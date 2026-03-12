#include "FEB_IMU.h"
#include "FEB_MMeter.h"
#include "FEB_TPS.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

// static void FEB_SystemClock_Config(void);
// void MX_I2C1_Init(void);
static uint8_t tx_buffer[1000];
extern I2C_HandleTypeDef hi2c3;

extern UART_HandleTypeDef huart2;

void FEB_Update()
{
  // read_Acceleration();
  // read_Angular_Rate();
  // read_Magnetic_Field_Data();
  // read_TPS();
  snprintf((char *)tx_buffer, sizeof(tx_buffer), "Starting I2C Scanning: \r\n");
  HAL_UART_Transmit(&huart2, tx_buffer, strlen((char *)tx_buffer), HAL_MAX_DELAY);

  uint8_t i = 0, ret;
  for (i = 1; i < 128; i++)
  {
    ret = HAL_I2C_IsDeviceReady(&hi2c3, (uint16_t)(i << 1), 3, 5);
    if (ret != HAL_OK)
    { /* No ACK Received At That Address */
      snprintf((char *)tx_buffer, sizeof(tx_buffer), " - ");
      HAL_UART_Transmit(&huart2, tx_buffer, strlen((char *)tx_buffer), HAL_MAX_DELAY);
    }
    else if (ret == HAL_OK)
    {
      snprintf((char *)tx_buffer, sizeof(tx_buffer), "0x%X", i);
      HAL_UART_Transmit(&huart2, tx_buffer, strlen((char *)tx_buffer), HAL_MAX_DELAY);
    }
  }
  snprintf((char *)tx_buffer, sizeof(tx_buffer), "Done! \r\n\r\n");
  HAL_UART_Transmit(&huart2, tx_buffer, strlen((char *)tx_buffer), HAL_MAX_DELAY);
}

void FEB_Init(void)
{
  lsm6dsox_init();
  lis3mdl_init();
  // tps_init();
  FEB_Update();
}
