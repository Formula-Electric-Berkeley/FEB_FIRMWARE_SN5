/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B_GATE_Pin GPIO_PIN_0
#define B_GATE_GPIO_Port GPIOA
#define Y_GATE_Pin GPIO_PIN_1
#define Y_GATE_GPIO_Port GPIOA
#define P_SENSE_1_BUFFER_Pin GPIO_PIN_4
#define P_SENSE_1_BUFFER_GPIO_Port GPIOA
#define P_SENSE_2_BUFFER_Pin GPIO_PIN_5
#define P_SENSE_2_BUFFER_GPIO_Port GPIOA
#define FROM_TSActivation_Pin GPIO_PIN_6
#define FROM_TSActivation_GPIO_Port GPIOA
#define DSMS_ON_Pin GPIO_PIN_1
#define DSMS_ON_GPIO_Port GPIOB
#define Driverless_Systems_Relay_Pin GPIO_PIN_10
#define Driverless_Systems_Relay_GPIO_Port GPIOB
#define TPS_PG_Pin GPIO_PIN_6
#define TPS_PG_GPIO_Port GPIOB
#define TPS_Alert_Pin GPIO_PIN_7
#define TPS_Alert_GPIO_Port GPIOB
#define TPS_Alert_EXTI_IRQn EXTI9_5_IRQn
#define TPS_SCL_Pin GPIO_PIN_8
#define TPS_SCL_GPIO_Port GPIOB
#define TPS_SDA_Pin GPIO_PIN_9
#define TPS_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
