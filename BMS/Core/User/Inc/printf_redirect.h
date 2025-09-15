/**
  ******************************************************************************
  * @file           : printf_redirect.h
  * @brief          : Header for printf_redirect.c file.
  *                   This file contains the printf redirection function prototypes.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef PRINTF_REDIRECT_H
#define PRINTF_REDIRECT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* FreeRTOS includes - Always enabled in this project -----------------------*/
#ifndef UNIT_TEST
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"
#else
#include "freertos_mock.h"
#endif

/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/
#define PRINTF_UART_TIMEOUT_MS 100
#define PRINTF_MUTEX_TIMEOUT_MS 100
#define PRINTF_MUTEX_TIMEOUT_MS 100
#define UART_PRINTF_BUFFER_SIZE 512
#define PRINTF_ISR_BUFFER_SIZE 64

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

/* Primary UART functions (thread-safe when FreeRTOS initialized) ----------*/
int uart_putchar(int ch, UART_HandleTypeDef* huart);
int uart_puts(const char* str, UART_HandleTypeDef* huart);  
int uart_printf(UART_HandleTypeDef* huart, const char* format, ...);
int debug_printf_safe(const char* format, ...);
bool uart_is_ready(UART_HandleTypeDef* huart);

/* FreeRTOS-specific functions ----------------------------------------------*/
int uart_printf_isr(const char* format, ...);
void printf_redirect_init(void);
void printf_redirect_deinit(void);

/* Task function for testing (mock in unit tests, real in .ioc) -------------*/
#ifdef UNIT_TEST
void printf_isr_task(void *pvParameters);
void printf_redirect_reset_for_test(void);
#endif

#ifdef __GNUC__
int __io_putchar(int ch);
#else
int fputc(int ch, FILE *f);
#endif

/* Private defines -----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif