/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
extern UART_HandleTypeDef huart2;
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BSPD_Indicator_Pin GPIO_PIN_0
#define BSPD_Indicator_GPIO_Port GPIOC
#define BSPD_Reset_Pin GPIO_PIN_1
#define BSPD_Reset_GPIO_Port GPIOC
#define Brake_Pressure_1_Pin GPIO_PIN_2
#define Brake_Pressure_1_GPIO_Port GPIOC
#define Brake_Pressure_2_Pin GPIO_PIN_3
#define Brake_Pressure_2_GPIO_Port GPIOC
#define Acc_Pedal_1_Pin GPIO_PIN_0
#define Acc_Pedal_1_GPIO_Port GPIOA
#define ACC_Pedal_2_Pin GPIO_PIN_1
#define ACC_Pedal_2_GPIO_Port GPIOA
#define Current_Sense_Pin GPIO_PIN_4
#define Current_Sense_GPIO_Port GPIOA
#define Shutdown_In_Pin GPIO_PIN_6
#define Shutdown_In_GPIO_Port GPIOA
#define Pre_Timing_Trip_Sense_Pin GPIO_PIN_7
#define Pre_Timing_Trip_Sense_GPIO_Port GPIOA
#define Brake_Input_Pin GPIO_PIN_4
#define Brake_Input_GPIO_Port GPIOC
#define PG_Pin GPIO_PIN_6
#define PG_GPIO_Port GPIOB
#define ALERT_Pin GPIO_PIN_9
#define ALERT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
