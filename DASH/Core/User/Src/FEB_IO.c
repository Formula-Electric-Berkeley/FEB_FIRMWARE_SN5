#include "FEB_IO.h"
#include "FEB_CAN.h"
#include "FEB_CAN_PCU.h"
#include "FEB_i2c_protected.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// /* ------------------- External handles ------------------- */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart3;

// /* ------------------- State variables ------------------- */
// static uint32_t rtd_press_start_time;
// static uint32_t rtd_buzzer_start_time = 0;
// static uint8_t set_rtd_buzzer = 1;
// static uint8_t r2d = 0;

// // static FEB_SM_ST_t bms_state;
// // static FEB_SM_ST_t prev_state;

// static uint8_t entered_drive = 0;
// static uint8_t exited_drive = 0;
// static uint32_t exit_buzzer_start_time = 0;

static uint32_t end_buzzer_tick = 0;

static IO_Switch_States_t state = {.switch_coolant_pump_radiator_fan = false,
                                   .switch_accumulator_fans = false,
                                   .button_rtd = false,
                                   .switch_logging = false};

static bool buzzer_enabled = false;

// MARK: Initialization
void FEB_IO_Init(void)
{
  uint8_t init_val[2]; // To initialize the IO expander GPIO, we first transmit 2 bytes of all 1s
  // memset(init_val, 0xFF, sizeof(init_val));
  init_val[0] = 0b11100001;
  init_val[1] = 0b11111111;

  FEB_I2C_Master_Transmit(&hi2c1, IOEXP_ADDR << 1, init_val, 2, HAL_MAX_DELAY);
}

// /* ------------------- Reset ------------------- */
// void FEB_IO_Reset_All(void)
// {
//   rtd_press_start_time = 0;
//   rtd_buzzer_start_time = 0;
//   set_rtd_buzzer = 1;
//   r2d = 0;

//   entered_drive = 0;
//   exited_drive = 0;
//   exit_buzzer_start_time = 0;
// }

// /* ------------------- Ready To Drive Button ------------------- */
void FEB_IO_HandleRTDButton(void)
{
  // uint8_t received_data = 0x00;
  // FEB_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, &received_data, 1, HAL_MAX_DELAY);

  // uint8_t brake_pressure = FEB_CAN_PCU_GetLastBreakPosition();
  // uint8_t inv_enabled = FEB_CAN_PCU_GetLastRMSEnabled();

  // prev_state = bms_state;
  // bms_state = FEB_CAN_BMS_Get_State();

  // /* Reset all if LV */
  // if (bms_state == FEB_SM_ST_LV)
  //   FEB_IO_Reset_All();

  // /* Detect transitions for buzzer */
  // if (prev_state == FEB_SM_ST_ENERGIZED && bms_state == FEB_SM_ST_DRIVE)
  //   entered_drive = 1;
  // else if (prev_state == FEB_SM_ST_DRIVE && bms_state == FEB_SM_ST_ENERGIZED)
  //   exited_drive = 1;

  // /* UI colors */
  // if (bms_state == FEB_SM_ST_DRIVE)
  // {
  //   if (inv_enabled)
  //     lv_obj_set_style_bg_color(ui_ButtonRTD, lv_color_hex(0x019F02), LV_PART_MAIN);
  //   else
  //     lv_obj_set_style_bg_color(ui_ButtonRTD, lv_color_hex(0xFFFF00), LV_PART_MAIN);
  // }
  // else if (bms_state == FEB_SM_ST_ENERGIZED)
  // {
  //   lv_obj_set_style_bg_color(ui_ButtonRTD, lv_color_hex(0xFFFF00), LV_PART_MAIN);
  // }
  // else
  // {
  //   lv_obj_set_style_bg_color(ui_ButtonRTD, lv_color_hex(0xFE0000), LV_PART_MAIN);
  //   r2d = 0;
  // }

  // /* RTD button logic */
  // if (received_data & (1 << 1))
  // {
  //   if ((HAL_GetTick() - rtd_press_start_time) >= BTN_HOLD_TIME && brake_pressure >= 20 &&
  //       (bms_state == FEB_SM_ST_ENERGIZED || bms_state == FEB_SM_ST_DRIVE))
  //   {
  //     // Attempt to enter or exit drive
  //     if (bms_state == FEB_SM_ST_ENERGIZED)
  //     {
  //       r2d = 1; // Try entering Drive
  //     }
  //     else if (bms_state == FEB_SM_ST_DRIVE)
  //     {
  //       r2d = 0; // Try exiting Drive
  //     }

  //     // Send R2D over CAN
  //     IO_state = (uint8_t)set_n_bit(IO_state, 1, r2d);
  //     FEB_CAN_ICS_Transmit_Button_State(IO_state);
  //     rtd_press_start_time = HAL_GetTick(); // reset timer
  //   }
  //   else
  //   {
  //     IO_state = set_n_bit(IO_state, 1, r2d);
  //   }
  // }
  // else
  // {
  //   rtd_press_start_time = HAL_GetTick();
  //   IO_state = set_n_bit(IO_state, 1, r2d);
  // }
}

// MARK: Switches
void FEB_IO_Update_GPIO(void)
{
  uint8_t received_data[2];
  memset(received_data, 0x00, sizeof(received_data));

  FEB_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, received_data, 2, HAL_MAX_DELAY);

  //   00010010 received_data
  //   00010000 (1 << 5)
  // & 00010000 -> (bool) -> true

  printf("[-] %X %X\n", received_data[0], received_data[1]);

  state.button_rtd = (bool)(received_data[0] & (1 << 1));

  state.switch_logging = !(bool)(received_data[1] & (1 << 1));
  state.switch_coolant_pump_radiator_fan = !(bool)(received_data[1] & (1 << 2));
  state.switch_accumulator_fans = !(bool)(received_data[1] & (1 << 3));

  FEB_IO_Update_Buzzer();
}

// MARK: Buzzer
void FEB_IO_Set_Buzzer(bool new_state)
{
  buzzer_enabled = new_state;

  printf(buzzer_enabled ? "buzzing" : "silent");
  uint8_t send_val[2];
  send_val[0] = buzzer_enabled ? 0b11100000 : 0b11100001;
  send_val[1] = 0b11111111;

  FEB_I2C_Master_Transmit(&hi2c1, IOEXP_ADDR << 1, send_val, 2, HAL_MAX_DELAY);
}

void FEB_IO_Update_Buzzer(void)
{
  FEB_IO_Set_Buzzer(end_buzzer_tick > HAL_GetTick()); // end_buzzer_tick determines if and how long to play the buzzer
}

void FEB_IO_Play_Buzzer(uint32_t duration)
{
  end_buzzer_tick = HAL_GetTick() + duration;
}

// void FEB_IO_HandleBuzzer(void)
// void FEB_IO_HandleRTDButton(void)
// {
//   static FEB_SM_ST_t prev_state = FEB_SM_ST_LV;
//   FEB_SM_ST_t bms_state = FEB_CAN_BMS_Get_State();
//   uint8_t inv_enabled = FEB_CAN_PCU_GetLastRMSEnabled();

//   if (prev_state == FEB_SM_ST_ENERGIZED && bms_state == FEB_SM_ST_DRIVE && inv_enabled)
//   {
//     FEB_IO_Play_Buzzer(RTD_BUZZER_TIME);
//   }
//   else if (prev_state == FEB_SM_ST_DRIVE && bms_state == FEB_SM_ST_ENERGIZED && !inv_enabled)
//   {
//     FEB_IO_Play_Buzzer(RTD_BUZZER_EXIT_TIME);
//   }

//   prev_state = bms_state;
// }

// if (prev_state == FEB_SM_ST_ENERGIZED && bms_state == FEB_SM_ST_DRIVE && inv_enabled) {
//    FEB_IO_Play_Buzzer(RTD_BUZZER_TIME);
// }
// else if (prev_state == FEB_SM_ST_DRIVE && bms_state == FEB_SM_ST_ENERGIZED && !inv_enabled) {
//    FEB_IO_Play_Buzzer(RTD_BUZZER_EXIT_TIME);
//   uint8_t inv_enabled = FEB_CAN_APPS_Get_Enabled();
//
//   if (entered_drive && bms_state == FEB_SM_ST_DRIVE && inv_enabled)
//   {
//     if (rtd_buzzer_start_time == 0)
//     {
//       rtd_buzzer_start_time = HAL_GetTick();
//     }
//     set_rtd_buzzer = 0;
//     IO_state = set_n_bit(IO_state, 0, set_rtd_buzzer);
//   }
//   else if (exited_drive && bms_state == FEB_SM_ST_ENERGIZED && !inv_enabled)
//   {
//     if (exit_buzzer_start_time == 0)
//     {
//       exit_buzzer_start_time = HAL_GetTick();
//     }
//     set_rtd_buzzer = 0;
//     IO_state = set_n_bit(IO_state, 0, set_rtd_buzzer);
//   }
//   else
//   {
//     set_rtd_buzzer = 1;
//     IO_state = set_n_bit(IO_state, 0, set_rtd_buzzer);
//   }

//   if (((HAL_GetTick() - rtd_buzzer_start_time) >= RTD_BUZZER_TIME && rtd_buzzer_start_time > 0) ||
//       ((HAL_GetTick() - exit_buzzer_start_time) >= RTD_BUZZER_EXIT_TIME && exit_buzzer_start_time > 0))
//   {
//     rtd_buzzer_start_time = 0;
//     exit_buzzer_start_time = 0;
//     entered_drive = 0;
//     exited_drive = 0;
//     set_rtd_buzzer = 1;
//     IO_state = set_n_bit(IO_state, 0, set_rtd_buzzer);
//   }

//   uint8_t transmit_rtd = (0b1111111 << 1) + set_rtd_buzzer;
//   FEB_I2C_Master_Transmit(&hi2c1, IOEXP_ADDR << 1, &transmit_rtd, 1, HAL_MAX_DELAY);

// FEB_CAN_ICS_Transmit_Button_State(IO_state);
// }

IO_Switch_States_t FEB_IO_GetLastIOStates(void)
{
  return state;
}
