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

#define RTD_SAFETY_DURATION 2000
#define BUZZER_DURATION_RTD_ENTER 2000
#define BUZZER_DURATION_RTD_EXIT 500

void FEB_State_Update_RTD(void);

bool FEB_State_GetLastRTD(void);

#endif /* FEB_RTD_H */
