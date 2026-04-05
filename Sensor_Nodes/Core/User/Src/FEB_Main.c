#include "FEB_IMU.h"
#include "FEB_MMeter.h"
#include "FEB_TPS.h"
#include "FEB_CAN.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include "FEB_Main.h"

void FEB_CAN_Rx_Callback(FEB_CAN_Bus_t bus, CAN_RxHeaderTypeDef *rx_header, void *data)
{
  // Handle received CAN messages
  (void)bus;
  (void)rx_header;
  (void)data;
}

void FEB_Update()
{
  read_Acceleration();
  read_Angular_Rate();
  // read_Magnetic_Field_Data();
  printf("\r\n");
}

void FEB_Init(void)
{
  // LOGI("[SETUP] Sensor Node Starting - IMU Test Mode\r\n");

  // Initialize IMU only
  lsm6dsox_init();
  // LOGI("[SETUP] IMU initialized\r\n");
}

void FEB_Main_Loop(void)
{
  // Update sensor readings and send CAN messages
  FEB_Update();
  HAL_Delay(100);
}
