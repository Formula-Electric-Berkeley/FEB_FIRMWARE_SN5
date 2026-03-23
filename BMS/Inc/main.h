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
#define INDICATOR_Pin GPIO_PIN_13
#define INDICATOR_GPIO_Port GPIOC
#define BMS_IND_Pin GPIO_PIN_0
#define BMS_IND_GPIO_Port GPIOC
#define BMS_A_Pin GPIO_PIN_1
#define BMS_A_GPIO_Port GPIOC
#define PC_AIR_Pin GPIO_PIN_2
#define PC_AIR_GPIO_Port GPIOC
#define SPI1_CS_Pin GPIO_PIN_3
#define SPI1_CS_GPIO_Port GPIOC
#define BUZZER_EN_Pin GPIO_PIN_0
#define BUZZER_EN_GPIO_Port GPIOA
#define IV_VLT_Pin GPIO_PIN_4
#define IV_VLT_GPIO_Port GPIOA
#define AIR_M_SENSE_Pin GPIO_PIN_4
#define AIR_M_SENSE_GPIO_Port GPIOC
#define AIR_P_SENSE_Pin GPIO_PIN_5
#define AIR_P_SENSE_GPIO_Port GPIOC
#define WAKE1_Pin GPIO_PIN_0
#define WAKE1_GPIO_Port GPIOB
#define INT1_Pin GPIO_PIN_1
#define INT1_GPIO_Port GPIOB
#define TSSI_IN_Pin GPIO_PIN_2
#define TSSI_IN_GPIO_Port GPIOB
#define SPI2_CS_Pin GPIO_PIN_6
#define SPI2_CS_GPIO_Port GPIOC
#define WAKE2_Pin GPIO_PIN_7
#define WAKE2_GPIO_Port GPIOC
#define INT2_Pin GPIO_PIN_8
#define INT2_GPIO_Port GPIOC
#define SHS_IMD_Pin GPIO_PIN_10
#define SHS_IMD_GPIO_Port GPIOC
#define SHS_TSMS_Pin GPIO_PIN_11
#define SHS_TSMS_GPIO_Port GPIOC
#define SHS_IN_Pin GPIO_PIN_12
#define SHS_IN_GPIO_Port GPIOC
#define PC_RELAY_Pin GPIO_PIN_2
#define PC_RELAY_GPIO_Port GPIOD
#define BMS_RESET_Pin GPIO_PIN_5
#define BMS_RESET_GPIO_Port GPIOB
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
