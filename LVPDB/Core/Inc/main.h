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

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CP_PG_Pin GPIO_PIN_0
#define CP_PG_GPIO_Port GPIOC
#define CP_Alert_Pin GPIO_PIN_1
#define CP_Alert_GPIO_Port GPIOC
#define CP_EN_Pin GPIO_PIN_2
#define CP_EN_GPIO_Port GPIOC
#define SH_EN_Pin GPIO_PIN_3
#define SH_EN_GPIO_Port GPIOC
#define SH_Alert_Pin GPIO_PIN_0
#define SH_Alert_GPIO_Port GPIOA
#define SH_PG_Pin GPIO_PIN_1
#define SH_PG_GPIO_Port GPIOA
#define RF_PWM_Pin GPIO_PIN_6
#define RF_PWM_GPIO_Port GPIOA
#define CP_PWM_Pin GPIO_PIN_7
#define CP_PWM_GPIO_Port GPIOA
#define AF1_EN_Pin GPIO_PIN_9
#define AF1_EN_GPIO_Port GPIOC
#define AF1_Alert_Pin GPIO_PIN_8
#define AF1_Alert_GPIO_Port GPIOA
#define AF1_PG_Pin GPIO_PIN_9
#define AF1_PG_GPIO_Port GPIOA
#define BL_Switch_Pin GPIO_PIN_15
#define BL_Switch_GPIO_Port GPIOA
#define RF_PG_Pin GPIO_PIN_12
#define RF_PG_GPIO_Port GPIOC
#define RF_Alert_Pin GPIO_PIN_2
#define RF_Alert_GPIO_Port GPIOD
#define RF_EN_Pin GPIO_PIN_5
#define RF_EN_GPIO_Port GPIOB
#define LV_PG_Pin GPIO_PIN_6
#define LV_PG_GPIO_Port GPIOB
#define LV_Alert_Pin GPIO_PIN_7
#define LV_Alert_GPIO_Port GPIOB
#define SCL_Pin GPIO_PIN_8
#define SCL_GPIO_Port GPIOB
#define SDA_Pin GPIO_PIN_9
#define SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
