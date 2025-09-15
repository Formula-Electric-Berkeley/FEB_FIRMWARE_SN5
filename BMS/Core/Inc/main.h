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
#define INDICATOR_Pin GPIO_PIN_13
#define INDICATOR_GPIO_Port GPIOC
#define BMS_IND_Pin GPIO_PIN_0
#define BMS_IND_GPIO_Port GPIOC
#define BMS_A_Pin GPIO_PIN_1
#define BMS_A_GPIO_Port GPIOC
#define PC_AIR_Pin GPIO_PIN_2
#define PC_AIR_GPIO_Port GPIOC
#define CS_Pin GPIO_PIN_3
#define CS_GPIO_Port GPIOC
#define BUZZER_Pin GPIO_PIN_0
#define BUZZER_GPIO_Port GPIOA
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define AIR__SENSE_Pin GPIO_PIN_4
#define AIR__SENSE_GPIO_Port GPIOC
#define AIR__SENSEC5_Pin GPIO_PIN_5
#define AIR__SENSEC5_GPIO_Port GPIOC
#define WAKE_Pin GPIO_PIN_0
#define WAKE_GPIO_Port GPIOB
#define INTR_Pin GPIO_PIN_1
#define INTR_GPIO_Port GPIOB
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define SHS_IMD_Pin GPIO_PIN_10
#define SHS_IMD_GPIO_Port GPIOC
#define SHS_TSMS_Pin GPIO_PIN_11
#define SHS_TSMS_GPIO_Port GPIOC
#define SHS_IN_Pin GPIO_PIN_12
#define SHS_IN_GPIO_Port GPIOC
#define PC_REL_Pin GPIO_PIN_2
#define PC_REL_GPIO_Port GPIOD
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB
#define RST_Pin GPIO_PIN_5
#define RST_GPIO_Port GPIOB
#define Alert_Pin GPIO_PIN_7
#define Alert_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
