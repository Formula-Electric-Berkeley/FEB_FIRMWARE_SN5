/**
 ******************************************************************************
 * @file           : FEB_RTD.h
 * @brief          : RTD State Handler
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_RTD_H
#define FEB_RTD_H

#include <stdbool.h>

#define RTD_BUTTON_HOLD_DURATION 2000
#define BUZZER_DURATION_RTD_ENTER 2000
#define BUZZER_DURATION_RTD_EXIT 500

typedef enum
{
  NOT_BUZZED = 0,
  BUZZED_ENTER_RTD,
  BUZZED_EXIT_RTD
} Buzzing_State_t;

void FEB_State_Update_RTD(void);

bool FEB_State_GetLastRTD(void);

#endif /* FEB_RTD_H */
