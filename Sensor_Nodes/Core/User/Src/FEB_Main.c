#include "FEB_IMU.h"
#include "FEB_MMeter.h"
#include "FEB_TPS.h"
#include "FEB_CAN.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

extern I2C_HandleTypeDef hi2c3;

static int32_t ping_pong_counter = 0;

void FEB_CAN_Rx_Callback(FEB_CAN_Bus_t bus, CAN_RxHeaderTypeDef *rx_header, void *data)
{
  // Handle received CAN messages
  (void)bus;
  (void)rx_header;
  (void)data;
}

void FEB_Update()
{
  // Send ping pong counter for testing
  FEB_CAN_PingPong_Send(FEB_CAN_BUS_1, 0, ping_pong_counter++);

  // read_Acceleration();
  // read_Angular_Rate();
  // read_Magnetic_Field_Data();
  // read_TPS();
  printf("Starting I2C Scanning: \r\n");

  uint8_t i = 0, ret;
  for (i = 1; i < 128; i++)
  {
    ret = HAL_I2C_IsDeviceReady(&hi2c3, (uint16_t)(i << 1), 3, 5);
    if (ret != HAL_OK)
    { /* No ACK Received At That Address */
      printf(" - ");
    }
    else if (ret == HAL_OK)
    {
      printf("0x%X", i);
    }
  }
  printf("Done! \r\n\r\n");
}

void FEB_Init(void)
{
  // Initialize printf redirect (already enabled by __io_putchar)
  printf("[SETUP] Sensor Node Starting...\r\n");

  // Initialize CAN (both CAN1 and CAN2)
  FEB_CAN_Init(FEB_CAN_Rx_Callback);
  printf("[SETUP] CAN initialized\r\n");

  // Initialize sensors
  lsm6dsox_init();
  lis3mdl_init();
  // tps_init();
}
