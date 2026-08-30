#ifndef DASH_IO_H
#define DASH_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C"
{
#endif
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
    bool switch_4;

    // Button states
    bool button_rtd;
    bool button_2;
    bool button_3;
    bool button_4;

    // Buzzer state
    bool buzzer_enabled;
  } IO_States_t;

  IO_States_t FEB_IO_GetLastIOStates(void);

  bool FEB_IO_StatusOk(void);

  void FEB_IO_Set_Buzzer(bool new_state);

  /* FreeRTOS task entry points. These override __weak stubs in Core/Src/freertos.c
   * by symbol name alone, so their linkage MUST be pinned here: if FEB_IO.c is
   * ever compiled as C++ without these declarations, the names mangle, the weak
   * stubs win silently, and the tasks become do-nothing loops. */
  void StartIoTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif
