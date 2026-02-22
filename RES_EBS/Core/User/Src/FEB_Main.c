#include "FEB_Main.h"

#include "can.h"
#include "i2c.h"
#include "main.h"

#include <stdint.h>

/* External peripheral handles from CubeMX-generated modules. */
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
extern I2C_HandleTypeDef hi2c1;

uint8_t counter = 0u;

void FEB_Main_Setup(void)
{
  /* TODO(RES_EBS):
   * Match PCU structure: initialize everything needed once at startup.
   * - Initialize CAN runtime/library and register RX/TX callbacks.
   * - Initialize RES/EBS state machine context.
   * - Initialize sensors/actuators on I2C/SPI/GPIO.
   * - Initialize debug UART/logging if needed.
   */

  /* Example CAN bring-up once runtime code is ready:
   * HAL_CAN_Start(&hcan1);
   * HAL_CAN_Start(&hcan2);
   */
}

void FEB_Main_While(void)
{
  /* TODO(RES_EBS):
   * Match PCU structure: put recurring control logic here.
   * - Process CAN RX/dispatch.
   * - Run RES/EBS state machine transitions.
   * - Update safety outputs.
   * - Send periodic status/heartbeat frames.
   */

  counter++;
  if (counter >= 100u)
  {
    counter = 0u;

    /* TODO(RES_EBS): low-rate debug/telemetry print block. */
  }

  /* Main loop timing (adjust after real logic is added). */
  HAL_Delay(10);
}
