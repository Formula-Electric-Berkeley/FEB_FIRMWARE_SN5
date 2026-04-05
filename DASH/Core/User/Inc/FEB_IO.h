#ifndef FEB_IO_H
#define FEB_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* Constants */
#define IOEXP_ADDR ((uint16_t)0x20)

/* Initialization & reset */
void FEB_IO_Init(void);
// void FEB_IO_Reset_All(void);

/* Modular handlers */
// void FEB_IO_HandleTSSI_IMD(void);
void FEB_IO_Update_GPIO(void);
void FEB_IO_Update_Buzzer(void);
void FEB_IO_Play_Buzzer(uint32_t duration);

/* Utilities & accessors */
uint8_t set_n_bit(uint8_t x, uint8_t n, uint8_t bit_value);
bool is_r2d(void);
void enable_r2d(void);
void disable_r2d(void);

typedef struct
{
  // Switch states
  bool switch_coolant_pump_radiator_fan;
  bool switch_accumulator_fans;
  bool switch_logging;

  // Button states
  bool button_rtd;
} IO_State_t;

IO_State_t FEB_IO_GetLastIOStates(void);

void FEB_IO_Set_Buzzer(bool new_state);

#endif
