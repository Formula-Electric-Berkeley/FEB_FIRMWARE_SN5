#include "FEB_Main.h"
#include "main.h"
#include "feb_can_lib.h"

// TODO: Move this stuff to other files for conciseness
// FEB_Init should initialize CAN usage, TPS usage, Pressure Sensor Reading, FEET setup, etc
void FEB_Init(void)
{

  // Initialize can config
  FEB_CAN_Config_t cfg = {
      .hcan1 = &hcan1,
      .hcan2 = &hcan2,
      .get_tick_ms = HAL_GetTick,
#if FEB_CAN_USE_FREERTOS
      .tx_queue = canTxQueueHandle,
      .rx_queue = canRxQueueHandle,
      .tx_mutex = canTxMutexHandle,
      .rx_mutex = canRxMutexHandle,
      .tx_mailbox_sem = canTxMailboxSemHandle,
#endif
  };

  if (FEB_CAN_Init(&cfg) != FEB_CAN_OK)
  {
    // Handle init failure
    while (1)
    {
    }
  }

  FEB_TPS_DeviceConfig_t cfg = {
      .hi2c = &hi2c1, // I2C peripheral
      .i2c_addr = FEB_TPS_ADDR(FEB_TPS_PIN_GND, FEB_TPS_PIN_GND),
      .r_shunt_ohms = 0.012f,
      .i_max_amps = 4.0f,
      .pg_gpio_port = GPIOB,     // From main.h (CubeMX)
      .pg_gpio_pin = TPS_PG_Pin, // From main.h (CubeMX)
      .name = "My Device",
  };
}

// TODO: Define Task starting functions here
