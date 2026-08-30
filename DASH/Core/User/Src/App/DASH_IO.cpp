#include "DASH_IO.h"
#include "DASH_CAN.h"
#include "DASH_I2C.h"
#include "feb_log.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "DASH_RTD.h"
#include "feb_can_subscriber.hpp"

namespace fc = feb::can;

extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart3;

static uint32_t end_buzzer_tick = 0;

static IO_States_t state = {.switch_coolant_pump_radiator_fan = false,
                            .switch_accumulator_fans = false,
                            .switch_logging = false,
                            .button_rtd = false,
                            .button_2 = false,
                            .button_3 = false,
                            .button_4 = false,
                            .buzzer_enabled = false};

static HAL_StatusTypeDef status;

// MARK: Initialization
void FEB_IO_Init(void)
{
  uint8_t init_val[2];
  init_val[0] = 0b11111111;
  init_val[1] = 0b11111111;

  status = FEB_I2C_Master_Transmit(&hi2c1, IOEXP_ADDR << 1, init_val, 2, HAL_MAX_DELAY);
}

// MARK: Switches
void FEB_IO_Update_GPIO(void)
{
  uint8_t received_data[2];
  memset(received_data, 0x00, sizeof(received_data));

  status = FEB_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, received_data, 2, HAL_MAX_DELAY);

  // Warn once, at boot, about the very first IO-expander read so a dead/floating
  // expander is loud on serial instead of silently shipping bad inputs onto CAN.
  static bool first_read_checked = false;
  if (!first_read_checked)
  {
    first_read_checked = true;
    if (status == HAL_ERROR || status == HAL_TIMEOUT)
    {
      LOG_E(TAG_I2C, "IO expander 0x%02X first read FAILED (status=%d)", (unsigned int)IOEXP_ADDR, (int)status);
    }
    else if (received_data[0] == 0xFF && received_data[1] == 0xFF)
    {
      LOG_W(TAG_I2C, "IO expander first read = FF FF (inputs floating? check harness/connector)");
    }
    else
    {
      LOG_I(TAG_I2C, "IO expander first read OK: %02X %02X", received_data[0], received_data[1]);
    }
  }

  //   00010010 received_data
  //   00010000 (1 << 5)
  // & 00010000 -> (bool) -> true

  // printf("[-] %X %X\n", received_data[0], received_data[1]);

  state.button_rtd = (bool)(received_data[0] & (1 << 1));
  state.button_2 = (bool)(received_data[0] & (1 << 2));
  state.button_3 = (bool)(received_data[0] & (1 << 3));
  state.button_4 = (bool)(received_data[0] & (1 << 4));

  state.switch_accumulator_fans = !(bool)(received_data[1] & (1 << 3));          // switch 1
  state.switch_coolant_pump_radiator_fan = !(bool)(received_data[1] & (1 << 2)); // switch 2
  state.switch_logging = !(bool)(received_data[1] & (1 << 1));                   // // switch 3
  state.switch_4 = !(bool)(received_data[1] & (1 << 0));                         // switch 4

  FEB_IO_Update_Buzzer();
}

// MARK: Buzzer
void FEB_IO_Set_Buzzer(bool new_state)
{
  state.buzzer_enabled = new_state;

  uint8_t send_val[2];
  send_val[0] = state.buzzer_enabled ? 0b11111110 : 0b11111111;
  send_val[1] = 0b11111111;

  status = FEB_I2C_Master_Transmit(&hi2c1, IOEXP_ADDR << 1, send_val, 2, HAL_MAX_DELAY);
}

void FEB_IO_Update_Buzzer(void)
{
  FEB_IO_Set_Buzzer(end_buzzer_tick > HAL_GetTick()); // end_buzzer_tick determines if and how long to play the buzzer
}

void FEB_IO_Play_Buzzer(uint32_t duration)
{
  end_buzzer_tick = HAL_GetTick() + duration;
}

IO_States_t FEB_IO_GetLastIOStates(void)
{
  return state;
}

bool FEB_IO_StatusOk(void)
{
  return status != HAL_ERROR && status != HAL_TIMEOUT;
}
