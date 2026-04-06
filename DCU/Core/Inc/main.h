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
#define RD_G0_Pin GPIO_PIN_0
#define RD_G0_GPIO_Port GPIOC
#define RD_G0_EXTI_IRQn EXTI0_IRQn
#define RD_RST_Pin GPIO_PIN_1
#define RD_RST_GPIO_Port GPIOC
#define RD_G1_Pin GPIO_PIN_2
#define RD_G1_GPIO_Port GPIOC
#define RD_G1_EXTI_IRQn EXTI2_IRQn
#define SD_CS_Pin GPIO_PIN_1
#define SD_CS_GPIO_Port GPIOA
#define RD_CS_Pin GPIO_PIN_15
#define RD_CS_GPIO_Port GPIOA
#define RD_EN_Pin GPIO_PIN_12
#define RD_EN_GPIO_Port GPIOC
#define PG_Pin GPIO_PIN_6
#define PG_GPIO_Port GPIOB
#define ALERT_Pin GPIO_PIN_7
#define ALERT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
