#include "FEB_IO.h"
#include "FEB_CAN.h"
#include "FEB_CAN_PCU.h"
#include "FEB_i2c_protected.h"
#include <stdbool.h>

// /* ------------------- External handles ------------------- */
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart3;

// /* ------------------- State variables ------------------- */
static uint32_t rtd_press_start_time;
static uint32_t rtd_buzzer_start_time = 0;
static uint8_t set_rtd_buzzer = 1;
static uint8_t r2d = 0;

// static FEB_SM_ST_t bms_state;
// static FEB_SM_ST_t prev_state;

static uint8_t entered_drive = 0;
static uint8_t exited_drive = 0;
static uint32_t exit_buzzer_start_time = 0;

static IO_State_t state = {.switch_coolant_pump_radiator_fan = false,
                           .switch_accumulator_fans = true,
                           .button_ready_to_drive = false,
                           .switch_logging = false};

// /* ------------------- Initialization ------------------- */
void FEB_IO_Init(void)
{
  uint8_t init_val = 0xF;
  FEB_I2C_Master_Transmit(&hi2c1, IOEXP_ADDR << 1, &init_val, 1, HAL_MAX_DELAY);
  // bms_state = FEB_CAN_BMS_Get_State();
}

// /* ------------------- Reset ------------------- */
void FEB_IO_Reset_All(void)
{
  rtd_press_start_time = 0;
  rtd_buzzer_start_time = 0;
  set_rtd_buzzer = 1;
  r2d = 0;

  entered_drive = 0;
  exited_drive = 0;
  exit_buzzer_start_time = 0;
}

// /* ------------------- Ready To Drive Button ------------------- */
void FEB_IO_HandleRTDButton(void)
{
  uint8_t received_data = 0x00;
  FEB_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, &received_data, 1, HAL_MAX_DELAY);

  uint8_t brake_pressure = FEB_CAN_PCU_GetLastBreakPosition();
  uint8_t inv_enabled = FEB_CAN_PCU_GetLastRMSEnabled();

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

// /* ------------------- Switches ------------------- */
void FEB_IO_Handle_Switches(void)
{
  uint8_t received_data = 0x00;
  FEB_I2C_Master_Receive(&hi2c1, IOEXP_ADDR << 1, &received_data, 1, HAL_MAX_DELAY);

  //   00010010 received_data
  //   00010000 (1 << 5)
  // & 00010000 -> (bool) -> true

  state.switch_logging = (bool)(received_data & (1 << 6));
  state.switch_coolant_pump_radiator_fan = (bool)(received_data & (1 << 5));
  state.switch_accumulator_fans = (bool)(received_data & (1 << 7));
}

// /* ------------------- Buzzer ------------------- */
// void FEB_IO_HandleBuzzer(void)
// {
//   uint8_t inv_enabled = FEB_CAN_APPS_Get_Enabled();

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

//   FEB_CAN_ICS_Transmit_Button_State(IO_state);
// }

IO_State_t FEB_IO_GetLastIOStates(void)
{
  return state;
}
