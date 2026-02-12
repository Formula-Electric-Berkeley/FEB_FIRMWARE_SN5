#ifndef FEB_IO_H
#define FEB_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/* Constants */
#define IOEXP_ADDR ((uint16_t)0x20)
#define BTN_HOLD_TIME ((uint32_t)2000)
#define RTD_BUZZER_TIME ((uint32_t)2000)
#define RTD_BUZZER_EXIT_TIME ((uint32_t)500)

/* Initialization & reset */
void FEB_IO_Init(void);
void FEB_IO_Reset_All(void);

/* Modular handlers */
void FEB_IO_HandleTSSI_IMD(void);
void FEB_IO_HandleRTDButton(void);
void FEB_IO_HandleDataLoggerButton(void);
void FEB_IO_HandleSwitches(void);
void FEB_IO_HandleBuzzer(void);

/* Utilities & accessors */
uint8_t set_n_bit(uint8_t x, uint8_t n, uint8_t bit_value);
bool is_r2d(void);
void enable_r2d(void);
void disable_r2d(void);

#endif
