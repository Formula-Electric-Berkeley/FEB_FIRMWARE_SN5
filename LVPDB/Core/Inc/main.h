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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SH_PG_Pin GPIO_PIN_13
#define SH_PG_GPIO_Port GPIOC
#define SH_Alert_Pin GPIO_PIN_0
#define SH_Alert_GPIO_Port GPIOC
#define CP_RF_EN_Pin GPIO_PIN_1
#define CP_RF_EN_GPIO_Port GPIOC
#define CP_RF_PG_Pin GPIO_PIN_2
#define CP_RF_PG_GPIO_Port GPIOC
#define CP_RF_Alert_Pin GPIO_PIN_3
#define CP_RF_Alert_GPIO_Port GPIOC
#define BM_PWM_G_Pin GPIO_PIN_0
#define BM_PWM_G_GPIO_Port GPIOA
#define BL_Switch_Pin GPIO_PIN_1
#define BL_Switch_GPIO_Port GPIOA
#define LT_Alert_Pin GPIO_PIN_4
#define LT_Alert_GPIO_Port GPIOA
#define LT_PG_Pin GPIO_PIN_5
#define LT_PG_GPIO_Port GPIOA
#define LT_EN_Pin GPIO_PIN_6
#define LT_EN_GPIO_Port GPIOA
#define AF1_AF2_Alert_Pin GPIO_PIN_7
#define AF1_AF2_Alert_GPIO_Port GPIOA
#define AF1_AF2_PG_Pin GPIO_PIN_4
#define AF1_AF2_PG_GPIO_Port GPIOC
#define AF1_AF2_EN_Pin GPIO_PIN_5
#define AF1_AF2_EN_GPIO_Port GPIOC
#define SM_EN_Pin GPIO_PIN_6
#define SM_EN_GPIO_Port GPIOC
#define SM_PG_Pin GPIO_PIN_7
#define SM_PG_GPIO_Port GPIOC
#define SM_Alert_Pin GPIO_PIN_8
#define SM_Alert_GPIO_Port GPIOC
#define BM_L_EN_Pin GPIO_PIN_9
#define BM_L_EN_GPIO_Port GPIOC
#define BM_L_Alert_Pin GPIO_PIN_8
#define BM_L_Alert_GPIO_Port GPIOA
#define BM_L_PG_Pin GPIO_PIN_9
#define BM_L_PG_GPIO_Port GPIOA
#define DSMS_ON_Pin GPIO_PIN_15
#define DSMS_ON_GPIO_Port GPIOA
#define IS_Vout_Pin GPIO_PIN_10
#define IS_Vout_GPIO_Port GPIOC
#define A0_Pin GPIO_PIN_11
#define A0_GPIO_Port GPIOC
#define A1_Pin GPIO_PIN_12
#define A1_GPIO_Port GPIOC
#define A2_Pin GPIO_PIN_2
#define A2_GPIO_Port GPIOD
#define SH_EN_Pin GPIO_PIN_5
#define SH_EN_GPIO_Port GPIOB
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
