/**
  ******************************************************************************
  * @file           : FEB_Printf_Redirect.h
  * @brief          : Simple printf redirection to UART using DMA
  ******************************************************************************
  * @attention
  *
  * This file provides simple functionality to redirect printf() output to a UART
  * peripheral using DMA for STM32 microcontrollers.
  *
  * USAGE:
  * 1. Configure UART and DMA in STM32CubeMX
  * 2. Call FEB_Printf_Init(&huartX) after UART initialization
  * 3. Use printf() normally - it will output to UART
  *
  ******************************************************************************
  */

#ifndef __FEB_PRINTF_REDIRECT_H
#define __FEB_PRINTF_REDIRECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/**
 * @brief Initialize printf redirection to UART with DMA
 * @param huart Pointer to UART handle (must be initialized with DMA)
 */
void FEB_Printf_Init(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __FEB_PRINTF_REDIRECT_H */